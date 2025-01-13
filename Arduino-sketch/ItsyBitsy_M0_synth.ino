/*
 * File:       ItsyBitsy_M0_synth (.ino)
 *
 * Project:    Sigma-6 Sound Synth / SAMD21 Voice Module
 *
 * Platform:   Adafruit 'ItsyBitsy M0 Express' or compatible (MCU: ATSAMD21G18)
 *
 * Author:     M.J.Bauer, 2025 -- www.mjbauer.biz
 *
 * Licence:    Open Source (Unlicensed) -- free to copy, distribute, modify
 *
 * Version:    1.0  (See Revision History file for change notes)
 */
#include <fast_samd21_tc3.h>
#include "m0_synth_def.h"

ConfigParams_t  g_Config;   // structure holding configuration params

uint32_t startPeriod_1sec;  // for heartbeat LED (1Hz)
uint32_t last_millis;       // for synth B/G process
uint8  channelSwitches;

void  TC3_Handler(void);    // Audio ISR - defined in "m0_synth_engine"


void  setup() 
{
  pinMode(CHAN_SWITCH_S1, INPUT_PULLUP);
  pinMode(CHAN_SWITCH_S2, INPUT_PULLUP);
  pinMode(CHAN_SWITCH_S3, INPUT_PULLUP);
  pinMode(CHAN_SWITCH_S4, INPUT_PULLUP);

  pinMode(MODE_JUMPER, INPUT_PULLUP); 
  pinMode(ISR_TESTPOINT, OUTPUT);  // scope test-point, ISR duty
  pinMode(SPI_DAC_CS, OUTPUT); 
  digitalWrite(SPI_DAC_CS, HIGH);  // Set DAC CS High (idle)   
  
  DefaultConfigData();      // Load default config param's
  if (digitalRead(MODE_JUMPER) == LOW)  
      g_Config.AudioAmpldCtrlMode = 2;  // ENV1*Velocity (override patch)
  PresetSelect(4);          // Load preset 4 (recorder)

  // Read the MIDI channel-select switches and set receiver mode
  channelSwitches = 0;
  if (digitalRead(CHAN_SWITCH_S1) == HIGH)  channelSwitches += 1;
  if (digitalRead(CHAN_SWITCH_S2) == HIGH)  channelSwitches += 2;
  if (digitalRead(CHAN_SWITCH_S3) == HIGH)  channelSwitches += 4;
  if (digitalRead(CHAN_SWITCH_S4) == HIGH)  channelSwitches += 8;
  g_Config.MidiChannel = channelSwitches;
  if (channelSwitches == 0)  g_Config.MidiMode = OMNI_ON_MONO;
  else  g_Config.MidiMode = OMNI_OFF_MONO;
  
  // Set wave-table sampling interval for audio ISR - Timer/Counter #3
  fast_samd21_tc3_configure((float) 1000000 / SAMPLE_RATE_HZ);  // period = 31.25us
  fast_samd21_tc3_start();

  Serial1.begin(31250);     // Open serial port for MIDI IN

  startPeriod_1sec = millis();
  last_millis = millis();
}


void  loop() 
{
  MidiInputService();

  if (millis() != last_millis)  // once every millisecond...
  {
      last_millis = millis();
      SynthProcess();
  }
    
  if ((millis() - startPeriod_1sec) > 1000)  // 1 second period
  {
    startPeriod_1sec = millis();  // capture LED period start time
    //
    // *** todo:  Pulse heartbeat LED .. 1Hz at 100ms duty... ***
    // Adafruit M0 Express:  Use RGB LED DotStar (magenta - low brightness!)
    // RobotDyn M0 Mini:  Use on-board TX or RX LED  (* No LED on D13 *)
    //
  }
}


/*`````````````````````````````````````````````````````````````````````````````````````````````````
 * Function:     Copy patch parameters from a specified preset patch in flash
 *               program memory to the "active" patch parameter array in data memory.

 * Entry args:   preset = index into preset-patch definitions array g_PresetPatch[]
 */
void  PresetSelect(uint8 preset)
{
    if (preset < GetNumberOfPresets())
    {
        memcpy(&g_Patch, &g_PresetPatch[preset], sizeof(PatchParamTable_t));
        SynthPrepare();
    }
}


/*`````````````````````````````````````````````````````````````````````````````````````````````````
 * Function:  MidiInputService()
 *
 * MIDI IN service routine, executed frequently from within main loop.
 * This routine monitors the serial MIDI INPUT stream and whenever a complete message is 
 * received, it is processed.
 *
 * The synth module responds to valid messages addressed to the configured MIDI IN channel and,
 * if MIDI mode is set to 'Omni On' (channel-select switches set to 0), it will respond to all
 * messages received, regardless of which channel(s) the messages are addressed to.
 * The module also responds to valid messages addressed to channel 16, regardless of the channel
 * switch setting, so that the host controller can transmit a "broadcast" message to all modules
 * on the MIDI network simultaneously.
 */
void  MidiInputService()
{
    static  uint8  midiMessage[MIDI_MSG_MAX_LENGTH];
    static  short  msgBytesExpected;
    static  short  msgByteCount;
    static  short  msgIndex;
    static  uint8  msgStatus;     // last command/status byte rx'd
    static  bool   msgComplete;   // flag: got msg status & data se
    
    uint8   msgByte;
    uint8   msgChannel;  // 1..16 !
    BOOL    gotSysExMsg = FALSE;
    
    if (Serial1.available() > 0)  // unread byte(s) available in Rx buffer
    {
        msgByte = Serial1.read();
        
        if (msgByte & 0x80)  // command/status byte received (bit7 High)
        {
            if (msgByte == SYSTEM_MSG_EOX)  
            {
                msgComplete = TRUE;
                gotSysExMsg = TRUE;
                midiMessage[msgIndex++] = SYSTEM_MSG_EOX;
                msgByteCount++;
            }
            else if (msgByte <= SYS_EXCLUSIVE_MSG)  // Regular command (not clock)
            {
                msgStatus = msgByte;
                msgComplete = FALSE;  // expecting data byte(s))
                midiMessage[0] = msgStatus;
                msgIndex = 1;
                msgByteCount = 1;  // have cmd already
                msgBytesExpected = MIDI_GetMessageLength(msgStatus);
            }
            // else ignore command/status byte
        }
        else    // data byte received (bit7 LOW)
        {
            if (msgComplete && msgStatus != SYS_EXCLUSIVE_MSG)  
            {
                if (msgByteCount == 0)  // start of new data set -- running status
                {
                    msgIndex = 1;
                    msgByteCount = 1;
                    msgBytesExpected = MIDI_GetMessageLength(msgStatus);
                }
            }
            if (msgIndex < MIDI_MSG_MAX_LENGTH)
            {
                midiMessage[msgIndex++] = msgByte;
                msgByteCount++;
            }
        }
        
        if ((msgByteCount != 0 && msgByteCount == msgBytesExpected) || gotSysExMsg)    
        {
            msgComplete = TRUE;
            msgChannel = (midiMessage[0] & 0x0F) + 1;  // 1..16
            
            if (msgChannel == g_Config.MidiChannel || msgChannel == 16
            ||  g_Config.MidiMode == OMNI_ON_MONO || msgStatus == SYS_EXCLUSIVE_MSG)
            {
                ProcessMidiMessage(midiMessage, msgByteCount); 
                msgBytesExpected = 0;
                msgByteCount = 0;
                msgIndex = 0;
            }
        }
    }
}


void  ProcessMidiMessage(uint8 *midiMessage, short msgLength)
{
    uint8  statusByte = midiMessage[0] & 0xF0;
    uint8  noteNumber = midiMessage[1];
    uint8  velocity = midiMessage[2];
    uint8  program = midiMessage[1];
    uint8  leverPosn_Lo = midiMessage[1];
    uint8  leverPosn_Hi = midiMessage[2];
    int16  bipolarPosn;
     
    switch (statusByte)
    {
        case NOTE_OFF_CMD:
        {
            SynthNoteOff(noteNumber);
            break;
        }
        case NOTE_ON_CMD:
        {
            if (velocity == 0)  SynthNoteOff(noteNumber);
            else  SynthNoteOn(noteNumber, velocity);
            break;
        }
        case CONTROL_CHANGE_CMD:
        {
            ProcessControlChange(midiMessage);
            break;
        }
        case PROGRAM_CHANGE_CMD:
        {
            PresetSelect(program);  // ignored if program N/A
            break;
        }
        case PITCH_BEND_CMD:
        {
            bipolarPosn = ((int16)(leverPosn_Hi << 7) | leverPosn_Lo) - 0x2000;
            SynthPitchBend(bipolarPosn);
            break;
        }
        case SYS_EXCLUSIVE_MSG: 
        {
            ProcessMidiSystemExclusive(midiMessage, msgLength);
            break;
        }
        default:  break;
    }  // end switch
}


void  ProcessControlChange(uint8 *midiMessage)
{
    static uint8  modulationHi = 0;    // High byte of CC data (7 bits)
    static uint8  pressureHi = 0;      // High byte of CC data (7 bits)
    uint8  CCnumber = midiMessage[1];  // Control Change 'register' number
    uint8  dataByte = midiMessage[2];  // Control Change data value
    int    data14;
    
    if (CCnumber == g_Config.MidiExpressionCCnum)
    {
        pressureHi = dataByte;
        data14 = (int) pressureHi << 7;
        SynthExpression(data14);
    }
    else if (CCnumber == (g_Config.MidiExpressionCCnum + 32))  // Low byte
    {
        data14 = (((int) pressureHi) << 7) + dataByte;
        SynthExpression(data14);
    }
    else if (CCnumber == 1)  // Modulation High Byte (01)
    {
        modulationHi = dataByte;
        data14 = ((int) modulationHi) << 7;
        SynthModulation(data14);
    }
    else if (CCnumber == 33)  // Modulation Low Byte
    {
        data14 = (((int) modulationHi) << 7) + dataByte;
        SynthModulation(data14);
    }
    // The following CC numbers are to set synth Config parameters:
    // ````````````````````````````````````````````````````````````````````````
    else if (CCnumber == 85)  // Set expression CC number (2, 7 or 11)
    {
        if (dataByte == 2 || dataByte == 7 || dataByte == 11)
            g_Config.MidiExpressionCCnum = dataByte;
    }
    else if (CCnumber == 86)  // Set audio ampld control mode
    {
        if (dataByte < 4)  g_Config.AudioAmpldCtrlMode = dataByte;
    }
    else if (CCnumber == 87)  // Set vibrato control mode
    {
        if (dataByte < 4)  g_Config.VibratoCtrlMode = dataByte;
    }
    else if (CCnumber == 88)  // Set pitch-bend control mode
    {
        if (dataByte < 4)  g_Config.PitchBendEnable = dataByte;
    }
    else if (CCnumber == 91)  // Set reverb mix level
    {
        if (dataByte <= 100)  g_Config.ReverbMix_pc = dataByte;
    }
    // The following CC numbers are to set synth Patch parameters:
    // ````````````````````````````````````````````````````````````````````````
    else if (CCnumber == 92)  // Set LFO FM (vibrato) depth
    {
        if (dataByte <= 200)  g_Patch.LFO_FM_Depth = dataByte;
    }
    else if (CCnumber == 70)  // Set osc. mixer output gain (x10)
    {
        g_Patch.MixerOutGain_x10 = dataByte; 
    }
    else if (CCnumber == 71)  // Set limiter clip level (%)
    {
        if (dataByte <= 100)  g_Patch.LimiterLevelPc = dataByte; 
    }
    else if (CCnumber == 75)  // Set LFO frequency (data = Hz, max 50)
    {
        if (dataByte <= 50)  g_Patch.LFO_Freq_x10 = dataByte * 10; 
    }
    else if (CCnumber == 76)  // Set LFO ramp time (data = msec / 100)
    {
        if (dataByte <= 100)  g_Patch.LFO_RampTime = dataByte * 100; 
    }
    // Mode Change messages
    // ````````````````````````````````````````````````````````````````````````
    else if (CCnumber == 120 || CCnumber == 121)
    {
        SynthPrepare();  // All Sound Off - Kill note playing
    }
}


/*
 * The "manufacturer ID" (2nd byte of msg) is first validated to ensure the message
 * can be correctly interpreted, i.e. it's a Bauer exclusive message which contains
 * information about a Bauer MIDI controller (e.g. REMI) connected to the MIDI IN port.
 * Byte 3 of the message is a code to identify the type of message content.
 */
void  ProcessMidiSystemExclusive(uint8 *midiMessage, short msgLength)
{
    if (midiMessage[1] == SYS_EXCL_REMI_ID)  // "Manufacturer ID" match
    {
        // Nothing to be done in this application !
    }
}


int  MIDI_GetMessageLength(uint8 statusByte)
{
    uint8  command = statusByte & 0xF0;
    uint8  length = 0;  // assume unsupported or unknown msg type
    
    if (command == PROGRAM_CHANGE_CMD || command == CHAN_PRESSURE_CMD)  length = 2;
    if (command == NOTE_ON_CMD || command == NOTE_OFF_CMD
    ||  command == CONTROL_CHANGE_CMD || command == PITCH_BEND_CMD)  
    {
        length = 3;
    }
    if (statusByte == SYS_EXCLUSIVE_MSG)  length = MIDI_MSG_MAX_LENGTH;
      
    return  length;
}


/*`````````````````````````````````````````````````````````````````````````````````````````````````
 *   Set default values for configuration param's, except those which are assigned values
 *   by reading config switches, jumpers, etc, at start-up, e.g. MIDI channel & mode.
 *   Config param's may be changed subsequently by MIDI CC messages.
 */
void  DefaultConfigData(void)
{
    g_Config.AudioAmpldCtrlMode = 0;   // 0: use patch param
    g_Config.MidiExpressionCCnum = 2;
    g_Config.VibratoCtrlMode = 0;
    g_Config.PitchBendEnable = 1;
    g_Config.PitchBendRange = 2;
    g_Config.ReverbAtten_pc = 80;
    g_Config.ReverbMix_pc = 15;
}


// ================================================================================================
// ==========  Instrument Presets -- Array of patch parameter tables in flash memory  =============
//
// ... Values defined for g_Patch.OscFreqMult[] ..............................
// |  0  |  1  |   2   |  3  |  4  |  5  |  6  |  7  |  8  |  9  | 10  |  11 | <- index
// | 0.5 |  1  | 1.333 | 1.5 |  2  |  3  |  4  |  5  |  6  |  7  |  8  |   9 |
// ```````````````````````````````````````````````````````````````````````````
//
// ... Values defined for g_Patch.MixerInputStep[] ...........................................
// | 0 | 1 | 2 | 3  | 4  | 5  | 6  | 7  | 8  | 9  | 10  | 11  | 12  | 13  | 14  |  15 |  16  | <- step
// | 0 | 5 | 8 | 11 | 16 | 22 | 31 | 44 | 63 | 88 | 125 | 177 | 250 | 353 | 500 | 707 | 1000 |
// ```````````````````````````````````````````````````````````````````````````````````````````
//
// ... Values defined for g_Patch.OscAmpldModSource[] .........................
// |  0   |   1   |   2   |  3   |  4   |    5   |    6   |  7  |  8   |  9   | <- index
// | None | CONT+ | CONT- | ENV2 | MODN | EXPRN+ | EXPRN- | LFO | VEL+ | VEL- |
// ````````````````````````````````````````````````````````````````````````````
//
const  PatchParamTable_t  g_PresetPatch[] =
{
    {
        "Default Patch",                // 00
        { 1, 4, 5, 6, 7, 8 },           // Osc Freq Mult index (0..11)
        { 0, 0, 0, 0, 0, 0 },           // Osc Ampld Modn src (0..7)
        { 0, 0, 0, 0, 0, 0 },           // Osc Detune, cents (-600..+600)
        { 14, 14, 10, 0, 0, 0 },        // Mixer Input levels (0..16)
        20, 0, 20, 100, 200, 0,         // Amp Env (A-H-D-S-R), Amp Mode
        5, 20, 500, 95,                 // Contour Env (S-D-R-H)
        500, 50,                        // Transient ENV2: Decay, Sus %
        50, 500, 0, 0,                  // LFO: Hz x10, Ramp, FM %, AM %
        10, 0,                          // Mixer Gain x10, Limit %FS
    },

    //----------  Presets with percussive ampld envelope profile  -------------
    {
        "Electric Piano",               // 01
        { 1, 4, 5, 6, 7, 8 },           // Osc Freq Mult index (0..11)
        { 0, 0, 0, 0, 3, 3 },           // Osc Ampld Modn src (0..9)
        { 0, 0, -2, 3, -7, 6 },         // Osc Detune cents (+/-600)
        { 15, 13, 11, 9, 9, 8 },        // Osc Mixer levels (0..16)
        5, 50, 500, 0, 300, 2,          // Amp Env (A-H-D-S-R), Amp Mode
        5, 20, 1000, 95,                // Contour Env (S-D-R-H)
        200, 50,                        // ENV2: Dec, Sus %
        30, 500, 30, 20,                // LFO: Hz x10, Ramp, FM %, AM %
        10, 0,                          // Mixer Gain x10, Limit %FS
    },
    {
        "Steel Tine Clavier",           // 02
        { 1, 4, 5, 8, 9, 10 },          // Osc Freq Mult index (0..11)
        { 0, 2, 1, 2, 7, 1 },           // Osc Ampld Modn src (0..9)
        { 0, -21, 19, -27, -31, 0 },    // Osc Detune cents (+/-600)
        { 12, 12, 12, 10, 12, 12 },     // Osc Mixer levels (0..16)
        5, 20, 700, 0, 700, 2,          // Amp Env (A-H-D-S-R), Amp Mode
        5, 50, 800, 80,                 // Contour Env (S-D-R-H)
        500, 50,                        // ENV2: Dec, Sus %
        100, 500, 0, 55,                // LFO: Hz x10, Ramp, FM %, AM %
        10, 0,                          // Mixer Gain x10, Limit %FS
    },
    {
        "Tubular Bell",                 // 03
        { 1, 6, 9, 8, 0, 11 },          // Osc Freq Mult index (0..11)
        { 0, 0, 0, 0, 0, 0 },           // Osc Ampld Modn src (0..9)
        { 0, 23, -23, 0, 0, 0 },        // Osc Detune, cents (-600..+600)
        { 9, 14, 14, 0, 0, 0 },         // Mixer Input levels (0..16)
        5, 100, 2000, 0, 2000, 2,       // Amp Env (A-H-D-S-R), Amp Mode
        5, 20, 500, 95,                 // Contour Env (S-D-R-H)
        500, 50,                        // ENV2: Decay, Sus %
        30, 500, 10, 20,                // LFO: Hz x10, Ramp, FM%, AM%
        10, 0,                          // Mixer Gain x10, Limit %FS
    },

    //-------  Presets designed for ampld control by MIDI expression CC -------
    {
        "Recorder",                     // 04
        { 1, 5, 7, 9, 11, 0 },          // Osc Freq Mult index (0..11)
        { 0, 0, 0, 0, 0, 0 },           // Osc Ampld Modn Source ID (6)
        { 0, 0, 0, 0, 0, 0 },           // Osc Detune (6)
        { 16, 13, 11, 10, 9, 0 },       // Mixer Input levels (6)
        30, 0, 200, 80, 200, 3,         // Amp Env (A-H-D-S-R), Amp Mode
        5, 20, 500, 95,                 // Contour Env (S-D-R-H)
        500, 50,                        // ENV2: Decay, Sus %
        50, 500, 20, 0,                 // LFO: Hz x10, Ramp, FM%, AM%
        10, 0,                          // Mixer Gain x10, Limit %FS
    },
    {
        "Psychedelic Oboe",             // 05
        { 1, 3, 4, 5, 6, 9 },           // Osc Freq Mult index (0..11)
        { 0, 0, 0, 0, 0, 0 },           // Osc Ampld Modn src (0..9)
        { 0, 0, 0, 0, 0, 0 },           // Osc Detune cents (+/-600)
        { 11, 0, 11, 12, 14, 0 },       // Osc Mixer levels (0..16)
        30, 0, 200, 80, 200, 3,         // Amp Env (A-H-D-S-R), Amp Mode
        100, 10, 1000, 25,              // Contour Env (S-D-R-H)
        500, 50,                        // ENV2: Dec, Sus %
        50, 500, 20, 0,                 // LFO: Hz x10, Ramp, FM %, AM %
        10, 0,                          // Mixer Gain x10, Limit %FS
    },
    {
        "Stopped Flute",                // 06
        { 1, 4, 5, 6, 7, 8 },           // Osc Freq Mult index (0..11)
        { 0, 0, 0, 0, 0, 0 },           // Osc Ampld Modn src (0..9)
        { 0, 0, 0, 0, 0, 0 },           // Osc Detune cents (+/-600)
        { 15, 9, 6, 0, 0, 5 },          // Osc Mixer levels (0..16)
        30, 0, 5, 100, 300, 3,          // Amp Env (A-H-D-S-R), Amp Mode
        0, 50, 300, 100,                // Contour Env (S-D-R-H)
        500, 50,                        // ENV2: Dec, Sus %
        50, 500, 15, 0,                 // LFO: Hz x10, Ramp, FM %, AM %
        10, 0,                          // Mixer Gain x10, Limit %FS

    },
    {
        "Spaced Out Pipe",              // 07
        { 0, 3, 6, 0, 3, 6 },           // Osc Freq Mult index (0..11)
        { 0, 0, 0, 0, 0, 0 },           // Osc Ampld Modn src (0..9)
        { 6, 5, 4, -6, -5, -4 },        // Osc Detune cents (+/-600)
        { 13, 10, 10, 13, 10, 10 },     // Osc Mixer levels (0..16)
        30, 0, 200, 100, 200, 3,        // Amp Env (A-H-D-S-R), Amp Mode
        5, 20, 500, 95,                 // Contour Env (S-D-R-H)
        500, 50,                        // ENV2: Dec, Sus %
        50, 500, 15, 0,                 // LFO: Hz x10, Ramp, FM %, AM %
        10, 0,                          // Mixer Gain x10, Limit %FS
    },

    //-------  Presets with organ sounds and ampld envelope profile  ----------
    {
        "Melody Organ",                 // 08
        { 0, 1, 4, 5, 6, 7 },           // Osc Freq Mult index (0..11)
        { 0, 0, 0, 0, 0, 0 },           // Osc Ampld Modn src (0..9)
        { 0, 0, 0, 0, 0, 0 },           // Osc Detune cents (+/-600)
        { 15, 10, 12, 0, 0, 9 },        // Osc Mixer levels (0..16)
        30, 0, 5, 100, 300, 2,          // Amp Env (A-H-D-S-R), Amp Mode
        0, 50, 300, 100,                // Contour Env (S-D-R-H)
        500, 50,                        // ENV2: Dec, Sus %
        70, 500, 20, 0,                 // LFO: Hz x10, Ramp, FM %, AM %
        10, 0,                          // Mixer Gain x10, Limit %FS
    },
    {
        "Jazz Organ #1",                // 09
        { 0, 1, 5, 8, 0, 0 },           // Osc Freq Mult index (0..11)
        { 0, 0, 0, 3, 0, 0 },           // Osc Ampld Modn src (0..9)
        { 0, 0, 0, -3, 4, 0 },          // Osc Detune cents (+/-600)
        { 10, 13, 15, 12, 0, 0 },       // Osc Mixer levels (0..16)
        10, 0, 400, 100, 300, 2,        // Amp Env (A-H-D-S-R), Amp Mode
        5, 20, 600, 40,                 // Contour Env (S-D-R-H)
        100, 50,                        // ENV2: Dec, Sus %
        70, 500, 30, 0,                 // LFO: Hz x10, Ramp, FM %, AM %
        7, 0,                           // Mixer Gain x10, Limit %FS
    },
    {
        "Jazz Organ #2",                // 10
        { 0, 1, 4, 5, 8, 0 },           // Osc Freq Mult index (0..11)
        { 0, 0, 0, 0, 3, 0 },           // Osc Ampld Modn src (0..9)
        { 0, 0, -8, 4, -10, 0 },        // Osc Detune cents (+/-600)
        { 12, 15, 8, 15, 12, 0 },       // Osc Mixer levels (0..16)
        10, 0, 400, 100, 300, 2,        // Amp Env (A-H-D-S-R), Amp Mode
        5, 20, 600, 40,                 // Contour Env (S-D-R-H)
        200, 50,                        // ENV2: Dec, Sus %
        70, 500, 30, 0,                 // LFO: Hz x10, Ramp, FM %, AM %
        7, 0,                           // Mixer Gain x10, Limit %FS
    },
    {
        "Full Swell Organ",             // 11  (good bass!)
        { 0, 1, 4, 5, 6, 7 },           // Osc Freq Mult index (0..11)
        { 0, 0, 0, 0, 0, 0 },           // Osc Ampld Modn src (0..9)
        { 0, 0, 4, -3, 3, -3 },         // Osc Detune cents (+/-600)
        { 9, 15, 14, 12, 11, 8 },       // Osc Mixer levels (0..16)
        5, 0, 5, 100, 300, 2,           // Amp Env (A-H-D-S-R), Amp Mode
        0, 50, 300, 100,                // Contour Env (S-D-R-H)
        500, 50,                        // ENV2: Dec, Sus %
        70, 500, 20, 0,                 // LFO: Hz x10, Ramp, FM %, AM %
        7, 0,                           // Mixer Gain x10, Limit %FS
    },

    //---------  Misc. Presets with organ-like ampld envelope profile  --------
    {
        "Morph Harmonium",              // 12
        { 0, 3, 1, 7, 8, 10 },          // Osc Freq Mult index (0..11)
        { 0, 0, 0, 1, 2, 1 },           // Osc Ampld Modn src (0..9)
        { 3, -3, 0, -3, 3, -3 },        // Osc Detune cents (+/-600)
        { 15, 14, 15, 10, 11, 13 },     // Osc Mixer levels (0..16)
        30, 0, 10, 70, 500, 2,          // Amp Env (A-H-D-S-R), Amp Mode
        10, 50, 300, 90,                // Contour Env (S-D-R-H)
        500, 50,                        // ENV2: Dec, Sus %
        70, 500, 10, 55,                // LFO: Hz x10, Ramp, FM %, AM %
        5, 0,                           // Mixer Gain x10, Limit %FS
    },
    {
        "Meditation Pipe",              // 13
        { 1, 4, 6, 7, 8, 10 },          // Osc Freq Mult index (0..11)
        { 0, 0, 0, 0, 0, 6 },           // Osc Ampld Modn src (0..9)
        { 0, -5, 0, 4, 0, 0 },          // Osc Detune cents (+/-600)
        { 13, 14, 0, 9, 10, 9 },        // Osc Mixer levels (0..16)
        10, 0, 400, 100, 300, 2,        // Amp Env (A-H-D-S-R), Amp Mode
        5, 20, 600, 40,                 // Contour Env (S-D-R-H)
        100, 50,                        // ENV2: Dec, Sus %
        70, 500, 30, 0,                 // LFO: Hz x10, Ramp, FM %, AM %
        10, 0,                          // Mixer Gain x10, Limit %FS
    },
    {
        "Mellow Reed",                  // 14
        { 1, 5, 6, 7, 8, 0 },           // Osc Freq Mult index (0..11)
        { 0, 0, 0, 0, 0, 0 },           // Osc Ampld Modn src (0..9)
        { 0, 0, 0, 0, 0, 0 },           // Osc Detune cents (+/-600)
        { 14, 9, 6, 12, 12, 0 },        // Osc Mixer levels (0..16)
        30, 0, 200, 80, 200, 3,         // Amp Env (A-H-D-S-R), Amp Mode
        5, 20, 500, 95,                 // Contour Env (S-D-R-H)
        500, 50,                        // ENV2: Dec, Sus %
        50, 500, 20, 0,                 // LFO: Hz x10, Ramp, FM %, AM %
        10, 0,                          // Mixer Gain x10, Limit %FS
    },
    {
        "Rock Organ #3",                // 15
        { 0, 3, 1, 4, 6, 8 },           // Osc Freq Mult index (0..11)
        { 0, 0, 0, 0, 0, 0 },           // Osc Ampld Modn src (0..9)
        { 0, 0, -6, -7, 0, 0 },         // Osc Detune cents (+/-600)
        { 13, 13, 13, 13, 0, 0 },       // Osc Mixer levels (0..16)
        10, 0, 400, 100, 300, 2,        // Amp Env (A-H-D-S-R), Amp Mode
        5, 20, 600, 40,                 // Contour Env (S-D-R-H)
        500, 50,                        // ENV2: Dec, Sus %
        70, 500, 30, 0,                 // LFO: Hz x10, Ramp, FM %, AM %
        7, 0,                           // Mixer Gain x10, Limit %FS
    },
    
    //  More organ presets (as if there were not enough already) ..............
    {
        "Bauer Organ #1",               // 16
        { 1, 4, 6, 8, 10, 0 },          // Osc Freq Mult index (0..11)
        { 0, 0, 0, 0, 3, 0 },           // Osc Ampld Modn src (0..9)
        { 0, 4, -4, 3, -2, 3 },         // Osc Detune, cents (-600..+600)
        { 14, 14, 0, 10, 14, 10 },      // Mixer Input levels (0..16)
        20, 20, 400, 70, 300, 2,        // Amp Env (A-H-D-S-R), Amp Mode
        5, 20, 600, 40,                 // Contour Env (S-D-R-H)
        500, 50,                        // ENV2: Decay, Sus %
        70, 500, 30, 0,                 // LFO: Hz x10, Ramp, FM%, AM%
        7, 0,                           // Mixer Gain x10, Limit %FS
    },
    {
        "Bauer Organ #2",               // 17
        { 1, 3, 4, 5, 8, 0 },           // Osc Freq Mult index (0..11)
        { 0, 0, 0, 0, 3, 0 },           // Osc Ampld Modn src (0..9)
        { 0, 4, -4, 3, -2, 3 },         // Osc Detune, cents (-600..+600)
        { 13, 13, 10, 12, 14, 12 },     // Mixer Input levels (0..16)
        20, 20, 400, 70, 300, 2,        // Amp Env (A-H-D-S-R), Amp Mode
        5, 20, 600, 40,                 // Contour Env (S-D-R-H)
        500, 50,                        // ENV2: Decay, Sus %
        70, 500, 30, 0,                 // LFO: Hz x10, Ramp, FM%, AM%
        10, 0,                          // Mixer Gain x10, Limit %FS
    },
    {
        "Bauer Organ #3",               // 18
        { 1, 4, 6, 8, 10, 0 },          // Osc Freq Mult index (0..11)
        { 0, 0, 0, 0, 3, 0 },           // Osc Ampld Modn src (0..9)
        { 0, 4, -4, 3, -2, 3 },         // Osc Detune, cents (-600..+600)
        { 14, 14, 0, 10, 14, 10 },      // Mixer Input levels (0..16)
        20, 20, 400, 70, 300, 2,        // Amp Env (A-H-D-S-R), Amp Mode
        5, 20, 600, 40,                 // Contour Env (S-D-R-H)
        500, 50,                        // ENV2: Decay, Sus %
        70, 500, 30, 0,                 // LFO: Hz x10, Ramp, FM%, AM%
        7, 0,                           // Mixer Gain x10, Limit %FS
    },
    {
        "Drain Pipe",                   // 19  (aka "Rock Organ #1")
        { 1, 3, 4, 5, 8, 0 },           // Osc Freq Mult index (0..11)
        { 0, 0, 0, 0, 0, 0 },           // Osc Ampld Modn src (0..9)
        { 0, 0, 0, 0, 0, 0 },           // Osc Detune cents (+/-600)
        { 0, 11, 14, 0, 0, 0 },         // Osc Mixer levels (0..16)
        10, 0, 400, 100, 300, 2,        // Amp Env (A-H-D-S-R), Amp Mode
        5, 20, 600, 40,                 // Contour Env (S-D-R-H)
        500, 50,                        // ENV2: Dec, Sus %
        70, 500, 30, 0,                 // LFO: Hz x10, Ramp, FM %, AM %
        10, 0,                          // Mixer Gain x10, Limit %FS
    },
    //
    //
    //
};


// Function returns the number of Predefined Patch definitions...
//
int  GetNumberOfPresets(void)
{
    return  (int) sizeof(g_PresetPatch) / sizeof(PatchParamTable_t);
}

