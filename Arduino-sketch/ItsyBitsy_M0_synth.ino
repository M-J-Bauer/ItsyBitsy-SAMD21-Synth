/*
 * File:       ItsyBitsy_M0_synth (.ino)
 *
 * Project:    Sigma-6 Sound Synth / Voice Module
 *
 * Platform:   Adafruit 'ItsyBitsy M0 Express' or compatible (MCU: ATSAMD21G18)
 *
 * Author:     M.J.Bauer, 2025 -- www.mjbauer.biz
 *
 * Licence:    Open Source (Unlicensed) -- free to copy, distribute, modify
 *
 * Version:    2.0  (See Revision History file)
 */
#include <fast_samd21_tc3.h>
#include <Wire.h>
#include "m0_synth_def.h"

#define EEPROM_WRITE_INHIBIT()   {}    // Not used... WP tied to GND
#define EEPROM_WRITE_ENABLE()    {}

int   EEpromWriteData(uint8_t *pData, uint8_t promBlock, uint8_t promAddr, int nbytes);
int   EEpromReadData(uint8_t *pData, uint8_t promBlock, uint8_t promAddr, int nbytes);
void  TC3_Handler(void);       // Audio ISR - defined in "m0_synth_engine"

ConfigParams_t  g_Config;      // structure holding config param's

uint8_t  g_MidiChannel;        // 1..16  (16 = broadcast, omni)
uint8_t  g_MidiMode;           // OMNI_ON_MONO or OMNI_OFF_MONO
bool     g_MidiParamPending;   // Reserved Param Data Entry msg expected
uint8_t  g_GateState;          // GATE signal state (software)
bool     g_DisplayEnabled;     // True if OLED display enabled
bool     g_CVcontrolMode;      // True if CV control enabled
bool     g_MidiRxSignal;       // Signal MIDI message received (for GUI)
bool     g_EEpromFaulty;       // True if EEPROM error or not fitted
int      g_DebugData;
uint32_t g_NoteOnDelayBegin;   // Deferred Note-On usage
uint8_t  g_NotePlaying;
uint8_t  g_NotePending;
uint8_t  g_VeloPending;
uint8_t  g_LegatoMode;         // Switch ON or OFF using MIDI CC68 msg


void  setup()
{
  uint8_t  channelSwitches = 0;

  pinMode(CHAN_SWITCH_S1, INPUT_PULLUP);
  pinMode(CHAN_SWITCH_S2, INPUT_PULLUP);
  pinMode(CHAN_SWITCH_S3, INPUT_PULLUP);
  pinMode(CHAN_SWITCH_S4, INPUT_PULLUP);
  pinMode(CV_MODE_JUMPER, INPUT_PULLUP);
  pinMode(BUTTON_A_PIN, INPUT_PULLUP);
  pinMode(BUTTON_B_PIN, INPUT_PULLUP);

  pinMode(TESTPOINT1, OUTPUT);  // scope test-point TP1
  pinMode(TESTPOINT2, OUTPUT);  // scope test-point TP2
  pinMode(SPI_DAC_CS, OUTPUT);
  pinMode(GATE_INPUT, INPUT);
  digitalWrite(SPI_DAC_CS, HIGH);  // Set DAC CS High (idle)

  if (!USE_SPI_DAC_FOR_AUDIO) pinMode(A0, OUTPUT);  // Use MCU on-chip DAC for audio
  if (LEGATO_ENABLED_ALWAYS) g_LegatoMode = 1;

  if (digitalRead(CHAN_SWITCH_S1) == HIGH)  channelSwitches += 1;
  if (digitalRead(CHAN_SWITCH_S2) == HIGH)  channelSwitches += 2;
  if (digitalRead(CHAN_SWITCH_S3) == HIGH)  channelSwitches += 4;
  if (digitalRead(CHAN_SWITCH_S4) == HIGH)  channelSwitches += 8;
  if (digitalRead(CV_MODE_JUMPER) == LOW)  g_CVcontrolMode = TRUE;
  
  g_MidiChannel = channelSwitches;
  if (channelSwitches == 0)  g_MidiMode = OMNI_ON_MONO;
  else  g_MidiMode = OMNI_OFF_MONO;

  Serial1.begin(31250);        // initialize UART for MIDI IN
  Wire.begin();                // initialize IIC as master
  Wire.setClock(400*1000);     // set IIC clock to 400kHz
  analogReadResolution(12);    // set ADC resolution to 12 bits

  if (EEpromACKresponse() == FALSE)
    { g_EEpromFaulty = TRUE; }  // IIC bus error or EEprom not fitted

  if (FetchConfigData() == 0 || g_Config.EEpromCheckWord != 0xABCDEF00)
  {
    g_EEpromFaulty = TRUE;  // EEprom read failed or data error
    DefaultConfigData();
    StoreConfigData();
  }

  if (!g_Config.CV_ModeAutoSwitch)  g_CVcontrolMode = FALSE;  // MIDI control only
  if (g_Config.PresetLastSelected >= GetNumberOfPresets()) PresetSelect(0);
  else PresetSelect(g_Config.PresetLastSelected);

  // Set wave-table sampling interval for audio ISR - Timer/Counter #3
  fast_samd21_tc3_configure((float) 1000000 / SAMPLE_RATE_HZ);  // period = 31.25us
  fast_samd21_tc3_start();

  if (SH1106_Init())  // True if OLED controller responding on IIC bus
  {
    g_DisplayEnabled = TRUE;
    while (millis() < 100) ;   // delay for OLED init
    SH1106_Test_Pattern();     // test OLED display
    while (millis() < 600) ;   // delay to view test pattern
    Disp_ClearScreen();
    SH1106_SetContrast(30);
    GoToNextScreen(0);         // 0 => STARTUP SCREEN
  }
}

// Main background process loop...
//
void  loop()
{
  static uint32_t startPeriod_5ms;
  static uint32_t startPeriod_50ms;
  static uint32_t last_millis;

  MidiInputService();
  CVinputService();

  if (millis() != last_millis)  // once every millisecond...
  {
    last_millis = millis();
    SynthProcess();
  }

  if ((millis() - startPeriod_5ms) >= 5)  // 5ms period ended
  {
    startPeriod_5ms = millis();
    if (g_DisplayEnabled)  { ButtonScan();  PotService(); }
  }

  if ((millis() - startPeriod_50ms) >= 50)  // 50ms period ended
  {
    startPeriod_50ms = millis();
    if (g_DisplayEnabled)  UserInterfaceTask();
  }
}


/*`````````````````````````````````````````````````````````````````````````````````````````````````
 * Function:     Copy patch parameters from a specified preset patch in flash
 *               program memory to the "active" patch parameter array in data memory.
 *
 * Entry args:   preset = index into preset-patch definitions array g_PresetPatch[]
 *
 * Note:         If the selected preset patch parameter 'Ampld Control Mode' is set to
 *               Expression, then Legato Mode will be enabled; otherwise disabled.
 */
void  PresetSelect(uint8 preset)
{
  if (preset < GetNumberOfPresets())
  {
    memcpy(&g_Patch, &g_PresetPatch[preset], sizeof(PatchParamTable_t));
    SynthPrepare();
    g_Config.PresetLastSelected = preset;
    StoreConfigData();
  }

  if (g_Patch.AmpControlMode == AMPLD_CTRL_EXPRESS) g_LegatoMode = 1;
  else if (!LEGATO_ENABLED_ALWAYS) g_LegatoMode = 0;

  // Override conflicting config param(s)
  if (g_CVcontrolMode)  g_Config.PitchBendMode = PITCH_BEND_DISABLED;
  else  g_Config.PitchBendMode = PITCH_BEND_BY_MIDI_MSG;  // but, vibrato takes precedence
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
 *
 * Whenever a MIDI 'Note-On' message is received, the synth 'control mode' is switched to MIDI,
 * i.e. CV control mode is inhibited so that oscillator pitch is not affected by the CV1 input,
 * except if CV1 is configured to control Pitch BEND.
 */
void  MidiInputService()
{
  static  uint8  midiMessage[MIDI_MSG_MAX_LENGTH];
  static  short  msgBytesExpected;
  static  short  msgByteCount;
  static  short  msgIndex;
  static  uint8  msgStatus;     // last command/status byte rx'd
  static  bool   msgComplete;   // flag: got msg status & data set

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
      else if (msgByte <= SYS_EXCLUSIVE_MSG)  // Ignore Real-Time messages
      {
        msgStatus = msgByte;
        msgComplete = FALSE;  // expecting data byte(s))
        midiMessage[0] = msgStatus;
        msgIndex = 1;
        msgByteCount = 1;  // have cmd already
        msgBytesExpected = MIDI_GetMessageLength(msgStatus);
      }
    }
    else  // data byte received (bit7 LOW)
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

      if (msgChannel == g_MidiChannel || msgChannel == 16
      ||  g_MidiMode == OMNI_ON_MONO  || msgStatus == SYS_EXCLUSIVE_MSG)
      {
        ProcessMidiMessage(midiMessage, msgByteCount);
        g_MidiRxSignal = TRUE;  // signal to GUI to flash MIDI Rx icon
        msgBytesExpected = 0;
        msgByteCount = 0;
        msgIndex = 0;
      }
    }
  }

  // Deferred Note-On event... This code block is executed if 'Legato Mode' is disabled
  // and a MIDI Note-On message is received while another note is already playing.
  // The new note is initiated after a 5ms delay from the old note being terminated.
  if (g_NotePending && (millis() - g_NoteOnDelayBegin) > 5)  // 5ms delay expired
  {
    SynthNoteOn(g_NotePending, g_VeloPending);
    g_NotePlaying = g_NotePending;
    g_NotePending = 0;  // done
  }
}


void  ProcessMidiMessage(uint8 *midiMessage, short msgLength)
{
  static uint8  noteKeyedFirst;
  uint8  statusByte = midiMessage[0] & 0xF0;
  uint8  noteNumber = midiMessage[1];  // New note keyed
  uint8  velocity = midiMessage[2];
  uint8  program = midiMessage[1];
  uint8  leverPosn_Lo = midiMessage[1];  // modulation
  uint8  leverPosn_Hi = midiMessage[2];
  int16  bipolarPosn;
  bool   executeNoteOff = FALSE;
  bool   executeNoteOn = FALSE;

  switch (statusByte)
  {
    case NOTE_OFF_CMD:
    {
      executeNoteOff = TRUE;
      break;
    }
    case NOTE_ON_CMD:
    {
      if (velocity == 0) executeNoteOff = TRUE;
      else  executeNoteOn = TRUE;
      g_CVcontrolMode = FALSE;  // switch to MIDI control mode
      break;
    }
    case CONTROL_CHANGE_CMD:
    {
      ProcessControlChange(midiMessage);
      break;
    }
    case PROGRAM_CHANGE_CMD:
    {
      PresetSelect(program);  // ignored if program # undefined
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

  if (executeNoteOff)
  {
    if (g_LegatoMode)  // Here's where it gets tricky!
    {
      if (noteNumber == noteKeyedFirst)
      {
        SynthNoteOff(noteKeyedFirst);
        noteKeyedFirst = 0;
      }
      if (noteNumber == g_NotePlaying)
      {
        if (noteKeyedFirst)
        {
          SynthNoteOn(noteKeyedFirst, velocity);  // Change pitch (no attack)
          g_NotePlaying = noteKeyedFirst;
        }
        else  // only one note is keyed
        {
          SynthNoteOff(g_NotePlaying);
          g_NotePlaying = 0;
        }
      }
    }
    else  SynthNoteOff(noteNumber);  // Non-Legato
  }

  if (executeNoteOn)
  {
    if (g_LegatoMode)
    {
      SynthNoteOn(noteNumber, velocity);
      if (g_NotePlaying == 0) noteKeyedFirst = noteNumber;
      g_NotePlaying = noteNumber;
    }
    else if (g_NotePlaying)  // Non-Legato
    {
      SynthNoteOff(g_NotePlaying);  // End note playing
      g_NotePlaying = 0;
      g_NotePending = noteNumber;
      g_VeloPending = velocity;
      g_NoteOnDelayBegin = millis();  // Note-On deferred (see @line 250)
    }
    else  // No note playing and Legato not enabled...
    {
      SynthNoteOn(noteNumber, velocity);
      g_NotePlaying = noteNumber;
    }
  }
}


void  ProcessControlChange(uint8 *midiMessage)
{
  static uint8  modulationHi = 0;    // High byte of CC data (7 bits)
  static uint8  expressionHi = 0;    // High byte of CC data (7 bits)
  uint8  CCnumber = midiMessage[1];  // Control Change 'register' number
  uint8  dataByte = midiMessage[2];  // Control Change data value
  int    data14;

  if (CCnumber == 2 || CCnumber == 7 || CCnumber == 11)  // High byte
  {
    expressionHi = dataByte;
    data14 = (int) expressionHi << 7;
    SynthExpression(data14);
  }
  else if (CCnumber == 34 || CCnumber == 39 || CCnumber == 43)  // Low byte
  {
    data14 = (((int) expressionHi) << 7) + dataByte;
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
  else if (CCnumber == 100)  // Registered Parameter Low Byte
  {
    if (dataByte == 1) g_MidiParamPending = TRUE;  // Reg.Param. 01 is Master Tune
  }
  // The following CC numbers are to set synth Config parameters:
  // ````````````````````````````````````````````````````````````````````````
  else if (CCnumber == 38)  // Set Master Tune param. (= Data Entry LSB)
  {
    if (g_MidiParamPending)
    {
      g_Config.MasterTuneOffset = dataByte - 64;  // Offset by 64
      StoreConfigData();
      g_MidiParamPending = FALSE;
    }
  }
  else if (CCnumber == 68)  // Set Legato Keying mode on/off
  {
    if (dataByte >= 64)  g_LegatoMode = TRUE;
    else if (!LEGATO_ENABLED_ALWAYS) g_LegatoMode = FALSE;
    StoreConfigData();
  }
  else if (CCnumber == 85)  // Set pitch CV base note
  {
    if (dataByte >= 12 && dataByte < 60)  g_Config.Pitch_CV_BaseNote = dataByte;
    StoreConfigData();
  }
  else if (CCnumber == 86)  // Set audio ampld control mode
  {
    if (dataByte < 4)  g_Config.AudioAmpldCtrlMode = dataByte;
    StoreConfigData();
  }
  else if (CCnumber == 87)  // Set vibrato control mode
  {
    if (dataByte < 4)  g_Config.VibratoCtrlMode = dataByte;
    StoreConfigData();
  }
  else if (CCnumber == 88)  // Set pitch-bend control mode
  {
    if (dataByte < 4)  g_Config.PitchBendMode = dataByte;
    StoreConfigData();
  }
  else if (CCnumber == 89)  // Set reverb mix level
  {
    if (dataByte <= 100)  g_Config.ReverbMix_pc = dataByte;
    StoreConfigData();
  }
  // The following CC numbers are to set synth Patch parameters:
  // ````````````````````````````````````````````````````````````````````````
  else if (CCnumber == 70)  // Set osc. mixer output gain (unit = 0.1)
  {
    if (dataByte != 0)  g_Patch.MixerOutGain_x10 = dataByte;
  }
  else if (CCnumber == 71)  // Set ampld limiter level (%), 0 => OFF
  {
    if (dataByte <= 95)  g_Patch.LimiterLevelPc = dataByte;
  }
  else if (CCnumber == 72)  // Set Ampld ENV Release Time (unit = 100ms)
  {
    if (dataByte != 0 && dataByte <= 100)  g_Patch.EnvReleaseTime = (uint16) dataByte * 100;
  }
  else if (CCnumber == 73)  // Set Ampld ENV Attack Time (unit = 100ms)
  {
    if (dataByte != 0 && dataByte <= 100)  g_Patch.EnvAttackTime = (uint16) dataByte * 100;
  }
  else if (CCnumber == 74)  // Set Ampld ENV Peak Hold Time (unit = 100ms)
  {
    if (dataByte <= 100)  g_Patch.EnvHoldTime = (uint16) dataByte * 100;
  }
  else if (CCnumber == 75)  // Set Ampld ENV Deacy Time (unit = 100ms)
  {
    if (dataByte != 0 && dataByte <= 100)  g_Patch.EnvDecayTime = (uint16) dataByte * 100;
  }
  else if (CCnumber == 76)  // Set Ampld ENV Sustain Level (unit = 1%)
  {
    if (dataByte <= 100)  g_Patch.EnvSustainLevel = (uint16) dataByte;
  }
  else if (CCnumber == 77)  // Set LFO frequency (data = Hz, max 50)
  {
    if (dataByte != 0 && dataByte <= 50)  g_Patch.LFO_Freq_x10 = (uint16) dataByte * 10;
  }
  else if (CCnumber == 78)  // Set LFO ramp time (unit = 100ms)
  {
    if (dataByte <= 100)  g_Patch.LFO_RampTime = (uint16) dataByte * 100;
  }
  else if (CCnumber == 79)  // Set LFO FM (vibrato) depth (unit = 5 cents)
  {
    if (dataByte <= 120)  g_Patch.LFO_FM_Depth = (uint16) dataByte * 5;
  }
  // Mode Change messages
  // ````````````````````````````````````````````````````````````````````````
  else if (CCnumber == 120 || CCnumber == 123)
  {
    SynthPrepare();  // All Sound Off & Kill note playing
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
      // Nothing to be done in this version !
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
 * Function:  CVinputService()
 *
 * The CV input service routine is called frequently as possible from the main process loop.
 * If CV control mode is active, this routine monitors the 4 external analog (CV) inputs;
 * otherwise, only the GATE input is monitored. If a signal transition is detected on the GATE
 * input, the synth is switched to CV control mode (if it is not already in CV mode).
 * NB: The NE555 (Schmitt Trigger) on the GATE input inverts the logic state!
 *
 * Oscillator frequency (pitch) is updated according to the voltage on CV1 input,
 * using an exponential transfer function, i.e. 1 volt per octave.
 * Synth Modulation and Expression levels are updated according to the voltages on
 * CV2 and CV3 inputs (resp).  Input CV4 controls LFO FM depth, if the respective
 * vibrato control mode parameter is set accordingly.
 * The GATE input controls the synth envelope generators, triggering Attack and Release
 * events on the Gate signal leading and trailing edges (resp).
 */
void  CVinputService()
{
  static uint32 gateLeadingEdge;   // Captured time of GATE input Low-to-High (ms)
  static uint32 gateTrailingEdge;  // Captured time of GATE input High-to-Low (ms)
  static uint8 callCount;
  static bool resetCV1Filter;
  static bool attackPending;   // While true, waiting 5ms before trigger attack
  static int CV1readingFilt;   // CV1 reading smoothed (filtered) [16:16b fixed-pt]
  static int CV2readingPrev;   // CV2 reading on previous scan (mV)
  static int CV3readingPrev;   // CV3 reading on previous scan (mV)
  static int CV4readingPrev;   // CV4 reading on previous scan (mV)

  int  inputSignal_mV, noteNumber, freqLUTindex = 0;
  int  dataValue14b, FM_depth_cents;
  int  CV1Bound, deltaCV1;  // mV
  float  freqBound, freqStep, deltaFreq;  // Hz
  uint8  gateInput = (digitalRead(GATE_INPUT) == LOW) ? HIGH : LOW;

  if (g_GateState == LOW && gateInput == HIGH)  // GATE leading edge
  {
    if ((millis() - gateTrailingEdge) > 25)  // Hold-OFF time expired
    {
      gateLeadingEdge = millis();
      g_GateState = HIGH;
      if (g_Config.CV_ModeAutoSwitch) 
      {
        g_CVcontrolMode = TRUE;
        attackPending = TRUE;
      }
    }
  }

  if (attackPending && (millis() - gateLeadingEdge) >= 5)  // Gate delayed 5ms
  {
    SynthTriggerAttack();
	  resetCV1Filter = true;  // prevent CV1 input filter delay
    attackPending = FALSE;  // prevent repeat attacks
  }

  if (g_GateState == HIGH && gateInput == LOW)  // GATE trailing edge
  {
    if ((millis() - gateLeadingEdge) > 25)  // Hold-ON time expired
    {
      gateTrailingEdge = millis();
      g_GateState = LOW;
      if (g_Config.CV_ModeAutoSwitch) SynthTriggerRelease();
    }
  }

  if (!g_CVcontrolMode)  return;  // MIDI control mode... bail!
  
  if ((callCount & 1) == 0)  // on every alternate call...
  {
    inputSignal_mV = ((int) analogRead(A1) * g_Config.CV1_FullScale_mV) / 4095;
	  if (resetCV1Filter) CV1readingFilt = inputSignal_mV << 16;  // convert to fixed-pt
    CV1readingFilt -= CV1readingFilt >> 4;   // Apply 1st-order IIR filter, K = 1/16
    CV1readingFilt += inputSignal_mV << 12;  // input mV converted to fixed-pt * K
    inputSignal_mV = CV1readingFilt >> 16;   // integer part
    //
    if (g_Config.Pitch_CV_Quantize)  // Pitch quantized to nearest semitone
    {
      freqLUTindex = (60 * (inputSignal_mV + 41)) / 5000;  // 0..60 (spans 5 octaves)
      freqLUTindex += (g_Config.Pitch_CV_BaseNote - 12);  // offset by MIDI base-note
      SynthSetOscFrequency(g_NoteFrequency[freqLUTindex]);
    }
    else  // Pitch not quantized -- Apply linear LUT interpolation
    {
      noteNumber = (60 * inputSignal_mV) / 5000;  // 0..60 (spans 5 octaves)
      freqLUTindex = noteNumber + (g_Config.Pitch_CV_BaseNote - 12);
      freqBound = g_NoteFrequency[freqLUTindex];  // lower bound of LUT value
      CV1Bound = (noteNumber * 5000) / 60;  // mV at lower bound (multiple of 83.333)
      deltaCV1 = inputSignal_mV - CV1Bound;
      freqStep = g_NoteFrequency[freqLUTindex+1] - freqBound;
      deltaFreq = freqStep * (60 * deltaCV1) / 5000;  // = freqStep * (deltaCV1 / 83.333)
      SynthSetOscFrequency(freqBound + deltaFreq);  // interpolated frequency
    }
	  resetCV1Filter = false;
  }
  if (callCount == 1)
  {
    inputSignal_mV = ((int) analogRead(A2) * 5100) / 4095;
    if (abs(inputSignal_mV - CV2readingPrev) > 10)  // > 0.2% change
    {
      dataValue14b = (inputSignal_mV * 16384) / 5000;  // 0..16K
      if (dataValue14b >= 16384 )  dataValue14b = 16383;  // max.
      SynthModulation(dataValue14b);
      CV2readingPrev = inputSignal_mV;
    }
  }
  if (callCount == 3)
  {
    inputSignal_mV = ((int) analogRead(A3) * 5100) / 4095;
    if (abs(inputSignal_mV - CV3readingPrev) > 10)  // > 0.2% change
    {
      dataValue14b = (inputSignal_mV * 16384) / 5000;  // 0..16K
      if (dataValue14b >= 16384 )  dataValue14b = 16383;  // max.
      SynthExpression(dataValue14b);
      CV3readingPrev = inputSignal_mV;
    }
  }
  if (callCount == 5)
  {
    inputSignal_mV = ((int) analogRead(A4) * 5100) / 4095;
    if ((g_Config.VibratoCtrlMode == VIBRATO_BY_CV_AUXIN)
    &&  (abs(inputSignal_mV - CV4readingPrev) > 10 ))  // > 0.2% change
    {
      FM_depth_cents = (inputSignal_mV * 600) / 5000;  // 0..600
      g_Patch.LFO_FM_Depth = FM_depth_cents;  // override preset
      CV4readingPrev = inputSignal_mV;
    }
  }
  if (++callCount >= 6)  callCount = 0;  // repeat sequence every 6 calls
}


/*`````````````````````````````````````````````````````````````````````````````````````````````````
 *   Set default values for configuration param's, except those which are assigned values
 *   by reading config switches, jumpers, etc, at start-up, e.g. MIDI channel & CV mode.
 *   Config param's may be changed subsequently by MIDI CC messages or the control panel.
 *
 *   Options for AudioAmpldCtrlMode, VibratoCtrlMode, PitchBendMode and MasterTuneOffset
 *   are defined in the header file: "m0_synth_def.h".
 */
void  DefaultConfigData(void)
{
  g_Config.AudioAmpldCtrlMode = AUDIO_CTRL_ENV1_VELO;
  g_Config.VibratoCtrlMode = VIBRATO_DISABLED;
  g_Config.PitchBendMode = PITCH_BEND_BY_MIDI_MSG;
  g_Config.PitchBendRange = 2;         // semitones (max. 12)
  g_Config.ReverbMix_pc = 15;          // 0..100 % (typ. 15)
  g_Config.PresetLastSelected = 1;     // user preference
  g_Config.Pitch_CV_BaseNote = 36;     // MIDI note # (12..59)
  g_Config.Pitch_CV_Quantize = FALSE;
  g_Config.CV_ModeAutoSwitch = TRUE;
  g_Config.CV3_is_Velocity = FALSE;
  g_Config.CV1_FullScale_mV = 5100;    // 5100 => uncalibrated
  g_Config.MasterTuneOffset = DEFAULT_MASTER_TUNING;
  g_Config.EEpromCheckWord = 0xABCDEF00;  // last entry
}

void  StoreConfigData()
{
  uint8  promAddr = 0;
  short  bytesToCopy = (short) sizeof(g_Config);
  uint8  *pData = (uint8 *) &g_Config;
  int    errorCode;

  while (bytesToCopy > 0)
  {
    errorCode = EEpromWriteData(pData, 0, promAddr, (bytesToCopy >= 16) ? 16 : bytesToCopy);
    if (errorCode != 0)  break;
    promAddr += 16;  pData += 16;  bytesToCopy -= 16;
  }
}

uint8  FetchConfigData()
{
  uint8  promAddr = 0;
  short  bytesToCopy = (short) sizeof(g_Config);
  uint8  *pData = (uint8 *) &g_Config;
  uint8  count = 0;

  while (bytesToCopy > 0)
  {
    count += EEpromReadData(pData, 0, promAddr, (bytesToCopy >= 16) ? 16 : bytesToCopy);
    if (count == 0)  break;
    promAddr += 16;  pData += 16;  bytesToCopy -= 16;
  }
  return  count;  // number of bytes read;  0 if an error occurred
}


//=================  24LC08(B) IIC EEPROM Low-level driver functions  ===================
/*
 * Function:    EEpromACKresponse() -- Checks if an EEPROM responds on the IIC bus
 *
 * Returns:     TRUE if the device responds with ACK to a control byte
 */
bool  EEpromACKresponse(void)
{
  Wire.beginTransmission(0x50);  // Send control byte
  return  (Wire.endTransmission() == 0);  // ACK rec'd
}

/*
 * Function:    EEpromWriteData() -- Writes up to 16 bytes on a 16-byte boundary
 *
 * Entry arg's: pData = pointer to source data (byte array)
 *              promBlock = EEPROM block select (0, 1, 2, 3)
 *              promAddr = EEPROM beginning address in block for writing (0..255)
 *              nbytes = number of bytes to write (max = 16, not checked!)
 *
 * Notes:   1.  Maximum number of bytes written (16) is imposed by 24LC08 page size.
 *          2.  All bytes written must be in the same 16-byte page.
 *
 * Returns:     Error code, non-zero if I2C bus error detected, else 0 if write OK
 */
int  EEpromWriteData(uint8_t *pData, uint8_t promBlock, uint8_t promAddr, int nbytes)
{
  uint16  npolls = 1000;  // time-out = 25ms @ 400kHz SCK
  int   errcode = 0;

  EEPROM_WRITE_ENABLE();   // Set WP Low

  if (EEpromACKresponse())
  {
    Wire.beginTransmission(0x50 | (promBlock << 1));  // Control byte
    Wire.write(promAddr);
    Wire.write(pData, nbytes);
    errcode = Wire.endTransmission();

    while (npolls--)  // ACK polling -- exit when ACK rec'd
      { if (EEpromACKresponse()) break; }
    if (npolls == 0)  errcode += 10;
    else  EEpromACKresponse();  // Send "dummy" command
  }

  EEPROM_WRITE_INHIBIT();  // Set WP High (or float)
  return errcode;
}

/**
 * Function:    EEpromReadData() -- Reads up to 256 bytes sequentially from the EEPROM.
 *
 * Entry arg's: pData = pointer to destination (byte array)
 *              promBlock = EEPROM block select (0, 1, 2, 3)
 *              promAddr = EEPROM beginning address in block for reading (0..255)
 *              nbytes = number of bytes to read (max. 32, not checked) - see notes
 *
 * Notes:   1.  All bytes to be read must be in the same 256-byte block.
 *          2.  Arduino IIC 'Wire' library uses a 32-byte read/write buffer.
 *
 * Returns:     Number of bytes received from EEPROM;  = 0 if I2C bus error
 */
int  EEpromReadData(uint8_t *pData, uint8_t promBlock, uint8_t promAddr, int nbytes)
{
  int  bcount = 0;

  if (EEpromACKresponse())
  {
    Wire.beginTransmission(0x50 | (promBlock << 1));  // Control byte
    Wire.write(promAddr);
    delayMicroseconds(10);  // why? - see datasheet
    if (Wire.endTransmission() != 0)  return 0;  // an error occurred - bail

    Wire.requestFrom(0x50, nbytes);
    while (bcount < nbytes)  { *pData++ = Wire.read();  bcount++; }
  }

  return  bcount;
}


// ================================================================================================
// ==========  Instrument Presets -- Array of patch parameter tables in flash memory  =============
//
// ... Values defined for g_Patch.OscFreqMult[] ............................
// |  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  | 10  | 11  | <- index
// | 0.5 |  1  | 4/3 | 1.5 |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |
// `````````````````````````````````````````````````````````````````````````
//
// ... Values defined for g_Patch.MixerInputStep[] ...........................................
// | 0 | 1 | 2 | 3  | 4  | 5  | 6  | 7  | 8  | 9  | 10  | 11  | 12  | 13  | 14  |  15 |  16  |
// | 0 | 5 | 8 | 11 | 16 | 22 | 31 | 44 | 63 | 88 | 125 | 177 | 250 | 353 | 500 | 707 | 1000 |
// ```````````````````````````````````````````````````````````````````````````````````````````
//
// ... Values defined for g_Patch.OscAmpldModSource[] .........................
// |  0   |   1   |   2   |  3   |  4   |    5   |    6   |  7  |  8   |  9   | <- index
// | None | CONT+ | CONT- | ENV2 | MODN | EXPRN+ | EXPRN- | LFO | VEL+ | VEL- |
// ````````````````````````````````````````````````````````````````````````````
//
// For EWI controllers, Presets 24 thru 31 have 'Ampld Control Mode' set to 'Expression' (3).
// ````````````````````````````````````````````````````````````````````````````````````````````````
//
const  PatchParamTable_t  g_PresetPatch[] =
{
  {
    "Sound Test",                   // 00
    { 1, 4, 5, 6, 7, 8 },           // Osc Freq Mult index (0..11)
    { 0, 0, 0, 0, 0, 0 },           // Osc Ampld Modn src (0..9)
    { 0, 0, 0, 0, 0, 0 },           // Osc Detune cents (+/-600)
    { 15, 13, 11, 0, 0, 0 },        // Mixer Input levels (0..16)
    5, 0, 200, 80, 200, 2,          // Amp Env (A-H-D-S-R), Amp Mode
    5, 20, 500, 95,                 // Contour Env (S-D-R-H)
    500, 50,                        // ENV2: Decay, Sus %
    50, 500, 20, 20,                // LFO: Hz x10, Ramp, FM%, AM%
    10, 0,                          // Mixer Gain x10, Limit %FS
  },
  //  Presets with percussive ampld envelope profile, some with piano semblance
  {
    "Electric Piano #1",            // 01
    { 1, 3, 5, 7, 9, 11 },          // Osc Freq Mult index (0..11)
    { 0, 0, 0, 0, 0, 0 },           // Osc Ampld Modn source (0..7)
    { 0, 0, 0, 0, 0, 0 },           // Osc Detune cents (+/-600)
    { 14, 12, 8, 8, 5, 0 },         // Osc Mixer level/step (0..16)
    10, 70, 1500, 0, 500, 2,        // Ampld Env (A-H-D-S-R), Amp Mode
    5, 20, 1000, 95,                // Contour Env (S-D-R-H)
    200, 16,                        // ENV2: Dec, Sus %
    30, 500, 0, 20,                 // LFO: Hz x10, Ramp, FM %, AM %
    33, 60,                         // Mixer Gain x10, Limit %FS
  },
  {
    "Electric Piano #2",            // 02  
    { 1, 4, 5, 6, 7, 8 },           // Osc Freq Mult index (0..11)
    { 0, 3, 3, 3, 0, 0 },           // Osc Ampld Modn source (0..9)
    { 0, 0, 0, 0, 0, 0 },           // Osc Detune, cents (+/-600)
    { 14, 12, 13, 9, 9, 6 },        // Osc Mixer level/step (0..16)
    10, 50, 1500, 0, 300, 2,        // Ampld Env (A-H-D-S-R), Amp Mode
    5, 20, 1000, 95,                // Contour Env (S-D-R-H)
    700, 50,                        // ENV2: Decay/Rel, Sus %
    30, 500, 0, 20,                 // LFO: Hz x10, Ramp, FM %, AM %
    20, 50,                         // Mixer Gain x10, Limit %FS
  },
  {
    "Trashy Toy Piano",             // 03
    { 1, 1, 1, 4, 6, 7 },           // Osc Freq Mult index (0..11)
    { 0, 0, 0, 0, 3, 3 },           // Osc Ampld Modn source (0..9)
    { -18, 0, 19, -14, 0, 16 },     // Osc Detune, cents (+/-600)
    { 13, 13, 13, 11, 9, 7 },       // Osc Mixer level/step (0..16)
    5, 50, 500, 0, 300, 2,          // Ampld Env (A-H-D-S-R), Amp Mode
    5, 20, 1000, 95,                // Contour Env (S-D-R-H)
    200, 50,                        // ENV2: Decay/Rel, Sus %
    30, 500, 30, 20,                // LFO: Hz x10, Ramp, FM %, AM %
    7, 0,                           // Mixer Gain x10, Limit %FS
  },
  {
    "Steel-tine Clavier",           // 04
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
    "Tubular Bells",                // 05
    { 1, 6, 9, 8, 0, 11 },          // Osc Freq Mult index (0..11)
    { 0, 0, 7, 0, 0, 0 },           // Osc Ampld Modn src (0..9)
    { 0, 33, -35, 0, 0, 0 },        // Osc Detune, cents (-600..+600)
    { 9, 13, 13, 0, 0, 0 },         // Mixer Input levels (0..16)
    5, 100, 2000, 0, 2000, 2,       // Amp Env (A-H-D-S-R), Amp Mode
    5, 20, 500, 95,                 // Contour Env (S-D-R-H)
    500, 50,                        // ENV2: Decay, Sus %
    30, 500, 10, 20,                // LFO: Hz x10, Ramp, FM%, AM%
    10, 0,                          // Mixer Gain x10, Limit %FS
  },
  {
    "Smart Vibraphone",             // 06
    { 0, 1, 4, 6, 7, 11 },          // Osc Freq Mult index (0..11)
    { 7, 7, 0, 7, 0, 3 },           // Osc Ampld Modn source (0..9)
    { 0, 0, 0, 0, 0, 0 },           // Osc Detune, cents (+/-600)
    { 0, 13, 0, 9, 0, 13 },         // Osc Mixer level/step (0..16)
    5, 50, 2000, 0, 2000, 2,        // Ampld Env (A-H-D-S-R), Amp Mode
    0, 0, 200, 100,                 // Contour Env (S-D-R-H)
    500, 35,                        // ENV2: Decay/Rel, Sus %
    80, 5, 0, 40,                   // LFO: Hz x10, Ramp, FM %, AM %
    10, 0,                          // Mixer Gain x10, Limit %FS
  },
  {
    "Guitar Synthetique",           // 07
    { 1, 5, 6, 8, 9, 10 },          // Osc Freq Mult index (0..11)
    { 0, 0, 0, 1, 1, 1 },           // Osc Ampld Modn source (0..9)
    { 0, 0, 0, 0, 0, 0 },           // Osc Detune, cents (+/-600)
    { 13, 7, 11, 8, 9, 8 },         // Osc Mixer level/step (0..16)
    5, 200, 2000, 4, 700, 2,        // Ampld Env (A-H-D-S-R), Amp Mode
    25, 0, 500, 95,                 // Contour Env (S-D-R-H)
    500, 50,                        // ENV2: Decay/Rel, Sus %
    50, 500, 20, 20,                // LFO: Hz x10, Ramp, FM %, AM %
    20, 60,                         // Mixer Gain x10, Limit %FS
  },
  // Presets with organ-like sounds; some with transient envelope(s)...
  {
    "Jazz Organ #1",                // 08
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
    "Jazz Organ #2",                // 09
    { 0, 1, 4, 5, 8, 0 },           // Osc Freq Mult index (0..11)
    { 0, 0, 0, 0, 3, 0 },           // Osc Ampld Modn src (0..9)
    { 0, 0, -8, 4, -10, 0 },        // Osc Detune cents (+/-600)
    { 11, 14, 7, 14, 11, 0 },       // Osc Mixer levels (0..16)
    10, 0, 400, 100, 300, 2,        // Amp Env (A-H-D-S-R), Amp Mode
    5, 20, 600, 40,                 // Contour Env (S-D-R-H)
    200, 50,                        // ENV2: Dec, Sus %
    70, 500, 30, 0,                 // LFO: Hz x10, Ramp, FM %, AM %
    7, 0,                           // Mixer Gain x10, Limit %FS
  },
  {
    "Rock Organ #3",                // 10  (aka 'Rock Organ #3')
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
  {
    "Pink Floyd Organ",             // 11
    { 0, 3, 6, 0, 3, 6 },           // Osc Freq Mult index (0..11)
    { 0, 0, 0, 0, 0, 0 },           // Osc Ampld Modn src (0..9)
    { 6, 5, 4, -6, -5, -4 },        // Osc Detune cents (+/-600)
    { 13, 10, 10, 13, 10, 10 },     // Osc Mixer levels (0..16)
    30, 0, 200, 100, 200, 2,        // Amp Env (A-H-D-S-R), Amp Mode
    5, 20, 500, 95,                 // Contour Env (S-D-R-H)
    500, 50,                        // ENV2: Dec, Sus %
    50, 500, 15, 0,                 // LFO: Hz x10, Ramp, FM %, AM %
    7, 0,                           // Mixer Gain x10, Limit %FS
  },
  {
    "Hammond Orgessence",           // 12
    { 1, 3, 4, 5, 7, 8 },           // Osc Freq Mult index (0..11)
    { 0, 0, 0, 0, 0, 3 },           // Osc Ampld Modn source (0..7)
    { 0, -7, 12, 4, 0, 3 },         // Osc Detune cents (+/-600)
    { 13, 3, 0, 9, 0, 15 },         // Osc Mixer level/step (0..16)
    10, 0, 400, 100, 300, 2,        // Ampld Env (A-H-D-S-R), Amp Mode
    5, 20, 600, 40,                 // Contour Env (S-D-R-H)
    200, 25,                        // ENV2: Dec, Sus %
    70, 500, 20, 0,                 // LFO: Hz x10, Ramp, FM %, AM %
    7, 0,                           // Mixer Gain x10, Limit %FS
  },
  {
    "Bauer Organ #1",               // 13
    { 1, 4, 6, 8, 10, 0 },          // Osc Freq Mult index (0..11)
    { 0, 0, 0, 0, 3, 0 },           // Osc Ampld Modn src (0..9)
    { 0, 4, -4, 3, -2, 3 },         // Osc Detune, cents (-600..+600)
    { 13, 13, 0, 9, 13, 11 },       // Mixer Input levels (0..16)
    20, 20, 400, 70, 300, 2,        // Amp Env (A-H-D-S-R), Amp Mode
    5, 20, 600, 40,                 // Contour Env (S-D-R-H)
    500, 50,                        // ENV2: Decay, Sus %
    70, 500, 30, 0,                 // LFO: Hz x10, Ramp, FM%, AM%
    7, 0,                           // Mixer Gain x10, Limit %FS
  },
  {
    "Bauer Organ #2",               // 14
    { 1, 3, 4, 5, 8, 0 },           // Osc Freq Mult index (0..11)
    { 0, 0, 0, 0, 3, 0 },           // Osc Ampld Modn src (0..9)
    { 0, 4, -4, 3, -2, 3 },         // Osc Detune, cents (-600..+600)
    { 13, 13, 10, 12, 14, 12 },     // Mixer Input levels (0..16)
    20, 20, 400, 70, 300, 2,        // Amp Env (A-H-D-S-R), Amp Mode
    5, 20, 600, 40,                 // Contour Env (S-D-R-H)
    500, 50,                        // ENV2: Decay, Sus %
    70, 500, 30, 0,                 // LFO: Hz x10, Ramp, FM%, AM%
    7, 0,                           // Mixer Gain x10, Limit %FS
  },
  {
    "Full Swell Organ",             // 15  (Good for bass!)
    { 0, 1, 4, 5, 6, 7 },           // Osc Freq Mult index (0..11)
    { 0, 0, 0, 0, 0, 0 },           // Osc Ampld Modn src (0..9)
    { 0, 0, 4, -3, 3, -3 },         // Osc Detune cents (+/-600)
    { 8, 14, 13, 11, 10, 7 },       // Osc Mixer levels (0..16)
    5, 0, 5, 100, 300, 2,           // Amp Env (A-H-D-S-R), Amp Mode
    0, 50, 300, 100,                // Contour Env (S-D-R-H)
    500, 50,                        // ENV2: Dec, Sus %
    70, 500, 20, 0,                 // LFO: Hz x10, Ramp, FM %, AM %
    7, 0,                           // Mixer Gain x10, Limit %FS
  },
  // Miscellaneous experimental "instruments"  --------------------------------
  {
    "Morph Harmonium",              // 16
    { 0, 3, 1, 7, 8, 10 },          // Osc Freq Mult index (0..11)
    { 0, 0, 0, 1, 2, 1 },           // Osc Ampld Modn src (0..9)
    { 3, -3, 0, -3, 3, -3 },        // Osc Detune cents (+/-600)
    { 13, 12, 13, 8, 9, 11 },       // Osc Mixer levels (0..16)
    30, 0, 10, 70, 500, 2,          // Amp Env (A-H-D-S-R), Amp Mode
    10, 50, 300, 90,                // Contour Env (S-D-R-H)
    500, 50,                        // ENV2: Dec, Sus %
    70, 500, 10, 55,                // LFO: Hz x10, Ramp, FM %, AM %
    7, 0,                           // Mixer Gain x10, Limit %FS
  },
  {
    "Ring Modulator",               // 17
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
  {
    "Bass Overdrive",               // 18
    { 0, 1, 4, 6, 7, 8 },           // Osc Freq Mult index (0..11)
    { 0, 0, 0, 3, 0, 3 },           // Osc Ampld Modn source (0..7)
    { 0, 0, 0, 0, 0, 0 },           // Osc Detune cents (+/-600)
    { 14, 12, 8, 10, 0, 12 },       // Osc Mixer level/step (0..16)
    5, 0, 200, 80, 200, 2,          // Ampld Env (A-H-D-S-R), Amp Mode
    5, 20, 500, 95,                 // Contour Env (S-D-R-H)
    500, 50,                        // ENV2: Dec, Sus %
    50, 500, 20, 20,                // LFO: Hz x10, Ramp, FM %, AM %
    33, 50,                         // Mixer Gain x10, Limit %FS
  },
  {
    "Bellbird  (JPM)",              // 19  (created by JPM)
    { 9, 5, 8, 1, 8, 5 },           // Osc Freq Mult index (0..11)
    { 7, 3, 3, 3, 7, 7 },           // Osc Ampld Modn source (0..7)
    { 0, 0, 0, 0, 0, 0 },           // Osc Detune cents (+/-600)
    { 4, 8, 2, 10, 14, 15 },        // Osc Mixer level/step (0..16)
    70, 50, 100, 50, 700, 2,        // Ampld Env (A-H-D-S-R), Amp Mode
    0, 200, 500, 100,               // Contour Env (S-D-R-H)
    3000, 50,                       // ENV2: Dec, Sus %
    30, 70, 40, 35,                 // LFO: Hz x10, Ramp, FM %, AM %
    7, 0,                           // Mixer Gain x10, Limit %FS
  },
  {
    "Dull Steel Drum",              // 20  (old name: Dull Tone)
    { 1, 4, 5, 6, 8, 10 },          // Osc Freq Mult index (0..11)
    { 0, 0, 0, 1, 1, 1 },           // Osc Ampld Modn source (0..9)
    { 0, -14, 0, 23, 0, 0 },        // Osc Detune, cents (+/-600)
    { 15, 10, 4, 11, 8, 6 },        // Osc Mixer level/step (0..16)
    10, 50, 500, 0, 500, 2,         // Ampld Env (A-H-D-S-R), Amp Mode
    20, 0, 50, 80,                  // Contour Env (S-D-R-H)
    200, 25,                        // ENV2: Decay/Rel, Sus %
    100, 0, 0, 0,                   // LFO: Hz x10, Ramp, FM %, AM %
    7, 0,                           // Mixer Gain x10, Limit %FS
  },
  {
    "Wobulator",                    // 21
    { 0, 1, 2, 3, 4, 5 },           // Osc Freq Mult index (0..11)
    { 0, 0, 0, 7, 7, 7 },           // Osc Ampld Modn source (0..9)
    { 0, 0, 0, 0, 0, 0 },           // Osc Detune, cents (+/-600)
    { 10, 0, 13, 14, 0, 14 },       // Osc Mixer level/step (0..16)
    30, 300, 1000, 25, 1000, 2,     // Ampld Env (A-H-D-S-R), Amp Mode
    20, 0, 200, 80,                 // Contour Env (S-D-R-H)
    200, 25,                        // ENV2: Decay/Rel, Sus %
    80, 200, 30, 25,                // LFO: Hz x10, Ramp, FM %, AM %
    7, 0,                           // Mixer Gain x10, Limit %FS
  },
  {
    "Hollow Wood Drum",             // 22  (created by JPM)
    { 0, 1, 2, 3, 4, 5 },           // Osc Freq Mult index (0..11)
    { 6, 7, 6, 2, 0,  2 },          // Osc Ampld Modn source (0..7)
    { 0, 0, 0, 0, 0, 0 },           // Osc Detune cents (+/-600)
    { 5, 5, 4, 12, 8, 14 },         // Osc Mixer level/step (0..16)
    5, 20, 100, 0, 300, 2,          // Ampld Env (A-H-D-S-R), Amp Mode
    20, 0, 200, 80,                 // Contour Env (S-D-R-H)
    1500, 25,                       // ENV2: Dec, Sus %
    40, 5, 20, 20,                  // LFO: Hz x10, Ramp, FM %, AM %
    7, 0,                           // Mixer Gain x10, Limit %FS
  },
  {
    "Soft-attack Accordian",        // 23
    { 1, 4, 5, 6, 7, 8 },           // Osc Freq Mult index (0..11)
    { 0, 0, 1, 1, 1, 1 },           // Osc Ampld Modn source (0..9)
    { 0, 0, 0, 0, 0, 0 },           // Osc Detune, cents (+/-600)
    { 14, 12, 11, 10, 9, 8 },       // Osc Mixer level/step (0..16)
    100, 200, 2000, 10, 70, 2,      // Ampld Env (A-H-D-S-R), Amp Mode
    100, 0, 300, 30,                // Contour Env (S-D-R-H)
    500, 50,                        // ENV2: Decay/Rel, Sus %
    50, 500, 20, 20,                // LFO: Hz x10, Ramp, FM %, AM %
    10, 0,                          // Mixer Gain x10, Limit %FS
  },
  // Presets with Amp Control by Expression (for EWI controllers)...
  {
    "Terrible Recorder",            // 24  (aka 'Treble Recorder')
    { 1, 5, 7, 9, 11, 0 },          // Osc Freq Mult index (0..11)
    { 0, 0, 5, 0, 5, 0 },           // Osc Ampld Modn source (0..9)
    { 0, 0, 0, 0, 0, 0 },           // Osc Detune, cents (+/-600)
    { 14, 11, 13, 9, 13, 0 },       // Osc Mixer level/step (0..16)
    50, 0, 200, 80, 200, 3,         // Ampld Env (A-H-D-S-R), Amp Mode
    5, 20, 500, 95,                 // Contour Env (S-D-R-H)
    500, 50,                        // ENV2: Decay/Rel, Sus %
    50, 500, 20, 0,                 // LFO: Hz x10, Ramp, FM %, AM %
    7, 0,                           // Mixer Gain x10, Limit %FS
  },
  {
    "Psychedelic Oboe",             // 25  (* Add AM using exprn &/or mod'n *)
    { 1, 3, 4, 5, 6, 9 },           // Osc Freq Mult index (0..11)
    { 0, 0, 0, 0, 0, 0 },           // Osc Ampld Modn src (0..9)  <== todo
    { 0, 0, 0, 0, 0, 0 },           // Osc Detune cents (+/-600)
    { 11, 0, 11, 12, 14, 0 },       // Osc Mixer levels (0..16)
    30, 0, 200, 80, 200, 3,         // Amp Env (A-H-D-S-R), Amp Mode
    100, 10, 1000, 25,              // Contour Env (S-D-R-H)
    500, 50,                        // ENV2: Dec, Sus %
    50, 500, 20, 0,                 // LFO: Hz x10, Ramp, FM %, AM %
    7, 0,                           // Mixer Gain x10, Limit %FS
  },
  {
    "Stopped Flute",                // 26  (* Add AM using exprn &/or mod'n *)
    { 1, 4, 5, 6, 7, 8 },           // Osc Freq Mult index (0..11)
    { 0, 0, 0, 0, 0, 0 },           // Osc Ampld Modn src (0..9) <== todo
    { 0, 0, 0, 0, 0, 0 },           // Osc Detune cents (+/-600)
    { 15, 9, 6, 0, 0, 5 },          // Osc Mixer levels (0..16)
    30, 0, 5, 100, 300, 3,          // Amp Env (A-H-D-S-R), Amp Mode
    0, 50, 300, 100,                // Contour Env (S-D-R-H)
    500, 50,                        // ENV2: Dec, Sus %
    50, 500, 15, 0,                 // LFO: Hz x10, Ramp, FM %, AM %
    10, 0,                          // Mixer Gain x10, Limit %FS
  },
  {
    "Spaced Out Pipe",              // 27  (aka 'Pink Floyd Organ')
    { 0, 3, 6, 0, 3, 6 },           // Osc Freq Mult index (0..11)
    { 0, 0, 0, 0, 0, 0 },           // Osc Ampld Modn src (0..9)
    { 6, 5, 4, -6, -5, -4 },        // Osc Detune cents (+/-600)
    { 13, 10, 10, 13, 10, 10 },     // Osc Mixer levels (0..16)
    30, 0, 200, 100, 200, 3,        // Amp Env (A-H-D-S-R), Amp Mode
    5, 20, 500, 95,                 // Contour Env (S-D-R-H)
    500, 50,                        // ENV2: Dec, Sus %
    50, 500, 15, 0,                 // LFO: Hz x10, Ramp, FM %, AM %
    7, 0,                           // Mixer Gain x10, Limit %FS
  },
  {
    "Mellow Reed",                  // 28  (* Add AM using exprn &/or mod'n *)
    { 1, 5, 6, 7, 8, 0 },           // Osc Freq Mult index (0..11)
    { 0, 0, 0, 0, 0, 0 },           // Osc Ampld Modn src (0..9) <== todo
    { 0, 0, 0, 0, 0, 0 },           // Osc Detune cents (+/-600)
    { 14, 9, 6, 12, 12, 0 },        // Osc Mixer levels (0..16)
    30, 0, 200, 80, 200, 3,         // Amp Env (A-H-D-S-R), Amp Mode
    5, 20, 500, 95,                 // Contour Env (S-D-R-H)
    500, 50,                        // ENV2: Dec, Sus %
    50, 500, 20, 0,                 // LFO: Hz x10, Ramp, FM %, AM %
    10, 0,                          // Mixer Gain x10, Limit %FS
  },
  {
    "Meditation Pipe",              // 29  (* Add AM using exprn &/or mod'n *)
    { 1, 4, 6, 7, 8, 10 },          // Osc Freq Mult index (0..11)
    { 0, 0, 0, 0, 0, 6 },           // Osc Ampld Modn src (0..9) <== todo
    { 0, -5, 0, 4, 0, 0 },          // Osc Detune cents (+/-600)
    { 13, 14, 0, 9, 10, 9 },        // Osc Mixer levels (0..16)
    10, 0, 400, 100, 300, 3,        // Amp Env (A-H-D-S-R), Amp Mode
    5, 20, 600, 40,                 // Contour Env (S-D-R-H)
    100, 50,                        // ENV2: Dec, Sus %
    70, 500, 30, 0,                 // LFO: Hz x10, Ramp, FM %, AM %
    7, 0,                           // Mixer Gain x10, Limit %FS
  },
  {
    "Reed Overdrive",               // 30
    { 1, 4, 5, 7, 8, 9 },           // Osc Freq Mult index (0..11)
    { 0, 5, 5, 5, 5, 5 },           // Osc Ampld Modn source (0..9)
    { 0, 0, 0, 0, 0, 0 },           // Osc Detune, cents (+/-600)
    { 14, 12, 10, 10, 13, 13 },     // Osc Mixer level/step (0..16)
    70, 0, 200, 80, 200, 3,         // Ampld Env (A-H-D-S-R), Amp Mode
    5, 20, 500, 95,                 // Contour Env (S-D-R-H)
    500, 50,                        // ENV2: Decay/Rel, Sus %
    50, 500, 20, 0,                 // LFO: Hz x10, Ramp, FM %, AM %
    25, 50,                         // Mixer Gain x10, Limit %FS

  },
  {
    "Deep Saxophoney",              // 31
    { 0, 1, 4, 5, 6, 7 },           // Osc Freq Mult index (0..11)
    { 5, 0, 5, 0, 5, 5 },           // Osc Ampld Modn source (0..9)
    { 0, 0, 0, 0, 0, 0 },           // Osc Detune, cents (+/-600)
    { 9, 10, 13, 0, 13, 12 },       // Osc Mixer level/step (0..16)
    70, 0, 200, 70, 200, 3,         // Ampld Env (A-H-D-S-R), Amp Mode
    0, 50, 300, 100,                // Contour Env (S-D-R-H)
    500, 50,                        // ENV2: Decay/Rel, Sus %
    50, 500, 20, 0,                 // LFO: Hz x10, Ramp, FM %, AM %
    20, 50,                         // Mixer Gain x10, Limit %FS
  },
};


// Function returns the number of Predefined Patch definitions...
//
int  GetNumberOfPresets(void)
{
  return  (int) sizeof(g_PresetPatch) / sizeof(PatchParamTable_t);
}

