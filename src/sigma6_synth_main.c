/* ================================================================================================
 *
 * FileName:    sigma6_synth_main.c
 * `````````
 * Overview:    Main module for Bauer 'Sigma-6' Sound Synthesizer application.
 * `````````
 * Processor:   PIC32MX340F512H or PIC32MX440F256H  (See "HardwareProfile.h")
 * ``````````
 * Hardware:    * Olimex PIC32-MX340 prototyping board plus synth add-ons, or
 * `````````    * JPM 'MAM' board + Olimex PIC32-Pinguino-micro (PIC32MX440 BoB), or
 *              * MJB custom Sigma-6 Synth board + MJB PIC32MX340 BoB (* todo *)
 *
 * Compiler:    Microchip MPLAB XC32 (free version) under MPLAB.X IDE.
 * `````````    
 * Author:      M.J.Bauer, Copyright 2022++, All rights reserved
 * ```````
 * Reference:   www.mjbauer.biz
 * ``````````  
 * ================================================================================================
 */
#include "sigma6_synth_main.h"
#include "sigma6_synth_def.h"
#include "sigma6_synth_panel.h"

PRIVATE  void   ProcessMidiMessage(uint8 *midiMessage, short msgLength); 
PRIVATE  void   ProcessControlChange(uint8 *midiMessage);
PRIVATE  void   ProcessMidiSystemExclusive(uint8 *midiMessage, short msgLength);
PRIVATE  void   MidiInputMonitor(uint8 *midiMessage, short msgLength);

// -------------  Global data  --------------------------------------------------------
//
uint8   g_FW_version[4];         // firmware version # (major, minor, build, 0)
uint8   g_SelfTestFault[16];     // Self-test fault codes (0 => no fault)
char   *g_AppTitleCLI;
bool    g_LCD_ModuleDetected;    // True if LCD module detected
uint32  g_TaskCallCount;         // Debug usage
uint32  g_TaskCallFrequency;     // Debug usage
bool    g_MidiRxSignal;          // Signal MIDI message received (for GUI)
bool    g_MidiRxTimeOut;         // Signal MIDI message time-out (for GUI)

EepromBlock0_t  g_Config;        // structure holding configuration data
EepromBlock1_t  g_UserPatch;     // structure holding user patch param's

// Module private variables...
uint32  m_MidiRxTimer_ms;        // MIDI message time-out timer (for GUI)

//=================================================================================================

void  Init_Application(void)
{
    short i;  char textBuf[100];
	
    for (i = 1;  i < NUMBER_OF_SELFTEST_ITEMS;  i++)
        g_SelfTestFault[i] = 0;  // clear fault codes (except item 0)

    g_FW_version[0] = BUILD_VER_MAJOR;
    g_FW_version[1] = BUILD_VER_MINOR;
    g_FW_version[2] = BUILD_VER_DEBUG;
    
    UART2_init(57600);  // Debug serial port uses UART2
    putstr("\n* MCU reset/startup \n");
    putstr("Bauer 'Sigma 6' Sound Synthesizer \n");
    sprintf(textBuf, "Firmware version: %d.%d.%02d - %s \n", 
            g_FW_version[0], g_FW_version[1], g_FW_version[2], __DATE__);
    putstr(textBuf);
    putstr("Running self-test routine... \n");
    
    // Check that the MCU device ID matches the firmware build...
    if (MCU_ID_CHECK() == FALSE)
    {
        g_SelfTestFault[TEST_DEVICE_ID] = 1;
        putstr("! PIC32 device type incompatible with firmware build.\n");
    }

    if (LCD_Init())
    {
        g_LCD_ModuleDetected = 1;
        LCD_ClearScreen();
        LCD_BACKLIGHT_SET_HIGH();
    }
    else
    {
        g_SelfTestFault[TEST_LCD_MODULE] = 1;
        putstr("! LCD module not detected.\n");
    }
    
    if (FetchConfigData() == FALSE)    // Read Config data from EEPROM
    {
        g_SelfTestFault[TEST_EEPROM] = 1;
        DefaultConfigData();
    }
    if (FetchUserPatch() == FALSE)    // Read User Patch param's from EEPROM
    {
        g_SelfTestFault[TEST_EEPROM] = 1;
        DefaultUserPatch();
    }
    if (g_SelfTestFault[TEST_EEPROM]) 
        putstr("! EEPROM data error -- Loading defaults.\n");
    putstr("* Self-test completed.\n");
    
    UART1_init(31250);      // MIDI IN port uses UART1
    ADC_Init();             // ADC is for 6 pot inputs on pins RB5:RB0
    PWM_audioDAC_init();    // use PWM on OC# pin for audio output
    
    PresetSelect(g_Config.PresetLastSelected);
}


int  main(void)
{
    static uint32 startInterval_1sec;
    
    InitializeMCUclock();
    Init_MCU_IO_ports();
    Init_Application();
    
    while ( TRUE )   // main process loop
    {
        MidiInputService(); 
        ////
        ReadAnalogInputs();
        ////
        ControlPanelService();
        ////
        BackgroundTaskExec();
        ////
        if ((milliseconds() - startInterval_1sec) >= 1000)
        {
            startInterval_1sec = milliseconds();
            g_TaskCallFrequency = g_TaskCallCount;  // calls per second
            g_TaskCallCount = 0;
        }
    }
}


/*
 * Background task executive...  
 * Runs periodic tasks scheduled by the RTI timer ISR.
 *
 * Called frequently from the main loop and from inside wait loops.
 *
 */
void  BackgroundTaskExec()
{
    static uint8  toggle;
    
    if (isTaskPending_1ms())  SynthProcess(); // 1ms periodic task
    
    if (isTaskPending_50ms())  // 50ms periodic task(s)
    {
        if (m_MidiRxTimer_ms >= 2000)  g_MidiRxTimeOut = TRUE;
        else  m_MidiRxTimer_ms += 50;
    }
}


/*
 * Function:  isLCDModulePresent()
 *
 * Overview:  Return TRUE if LCD Module is present, as detected at startup.
 */
bool  isLCDModulePresent()
{
    return (g_LCD_ModuleDetected);
}


/*`````````````````````````````````````````````````````````````````````````````````````````````````
 * Function:     Copies patch parameters from a specified preset patch in flash
 *               program memory to the "active" patch parameter array in data memory.
 *               Exception: if preset == 0, copy 'User Patch' from EEPROM image.
 *
 * Entry args:   preset = index into preset-patch definitions array g_PresetPatch[]
 */
void  PresetSelect(uint8 preset)
{
    if (preset >= GetNumberOfPresets()) return;  // out of range - bail

    if (preset == 0)  // User Patch
        memcpy(&g_Patch, &g_UserPatch.Params, sizeof(PatchParamTable_t));
    else  // Preset Patch
        memcpy(&g_Patch, &g_PresetPatch[preset], sizeof(PatchParamTable_t));
    
    g_Config.PresetLastSelected = preset;  
    StoreConfigData();   // Save this Preset for next power-on/reset
    SynthPrepare();
}


/*^
 * Function:  MidiInputService()
 *
 * MIDI IN service routine, executed frequently from within main loop.
 * This routine monitors the MIDI INPUT stream and whenever a complete message is 
 * received, it is processed.
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
    
    if (UART1_RxDataAvail())    // unread byte(s) available in Rx buffer
    {
        msgByte = UART1_getch();
        
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
                // already processed complete message -- data repeating without command
                if (msgByteCount == 0)  // start of new data set 
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
            
            if (msgChannel == g_Config.MidiChannel 
            ||  g_Config.MidiMode == OMNI_ON_MONO
            ||  msgStatus == SYS_EXCLUSIVE_MSG)
            {
                g_MidiRxSignal = TRUE;  // signal to GUI
                m_MidiRxTimer_ms = 0;
                ////
                ProcessMidiMessage(midiMessage, msgByteCount); 
                ////
                msgBytesExpected = 0;
                msgByteCount = 0;
                msgIndex = 0;
            }
        }
    }
}


/*^
 * Function:  ProcessMidiMessage()
 *
 * This function processes a complete MIDI command/status message when received.
 * The message is analysed to determine what actions are required.
 */
PRIVATE  void  ProcessMidiMessage(uint8 *midiMessage, short msgLength)
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


PRIVATE  void  ProcessControlChange(uint8 *midiMessage)
{
    static  uint8  modulationHi = 0;  // High byte of CC data (7 bits)
    static  uint8  modulationLo = 0;  // Low byte  ..   ..
    static  uint8  pressureHi = 0;    // High byte of CC data (7 bits)
    static  uint8  pressureLo = 0;    // Low byte  ..   ..
    int    data14;
    
    if (midiMessage[1] == g_Config.MidiExpressionCCnum)
    {
        pressureHi = midiMessage[2];
        data14 = (((int) pressureHi) << 7) + pressureLo;
        SynthExpression(data14);
    }
    else if (midiMessage[1] == (g_Config.MidiExpressionCCnum + 32))
    {
        pressureLo = midiMessage[2];
        data14 = (((int) pressureHi) << 7) + pressureLo;
        SynthExpression(data14);
    }
    else if (midiMessage[1] == 0x01)  // Mod'n CC Hi Byte (01)
    {
        modulationHi = midiMessage[2];
        data14 = (((int) modulationHi) << 7) + modulationLo;
        SynthModulation(data14);
    }
    else if (midiMessage[1] == 0x41)  // Mod'n CC Lo Byte (33)
    {
        modulationLo = midiMessage[2];
        data14 = (((int) modulationHi) << 7) + modulationLo;
        SynthModulation(data14);
    }
    else if (midiMessage[1] == 120 || midiMessage[1] == 121)
    {
        SynthPrepare();  // All Sound Off - Kill note playing and reset synth engine
    }
}


/*^
 * Function:  ProcessMidiSystemExclusive()
 *
 * This function processes a recognized System Exclusive message when received.
 * 
 * The "manufacturer ID" (2nd byte of msg) is first validated to ensure the message
 * can be correctly interpreted, i.e. it's a 'Bauer exclusive' message which contains
 * information about a MIDI controller connected to the MIDI IN serial port.
 * Byte 3 of the message is a code to identify the type of message content.
 */
PRIVATE  void  ProcessMidiSystemExclusive(uint8 *midiMessage, short msgLength)
{
    if (midiMessage[1] == SYS_EXCL_REMI_ID)  // "Manufacturer ID" match
    {
        // nothing specified yet!
    }
}


/*`````````````````````````````````````````````````````````````````````````````````````````````````
 *   Function writes default values for persistent data in EEPROM block 0.
 *   These are "factory" defaults which are applied only in the event of erasure or
 *   corruption of EEPROM data, and of course in first-time programming.
 */
void  DefaultConfigData(void)
{
    g_Config.checkDword = 0xFEEDFACE;
    g_Config.EndOfDataBlockCode = 0xE0DBC0DE;

    g_Config.MidiMode = OMNI_OFF_MONO;
    g_Config.MidiChannel = 1;
    g_Config.AudioAmpldCtrlMode = 0;
    g_Config.MidiExpressionCCnum = 2;
    g_Config.VibratoCtrlMode = 0;
    g_Config.PitchBendEnable = 1;
    g_Config.PitchBendRange = 2;
    g_Config.ReverbAtten_pc = 70;
    g_Config.ReverbMix_pc = 15;
    g_Config.PresetLastSelected = 1;

    StoreConfigData(); 
}


/*
 *   Function writes default values for persistent data in EEPROM block 1.
 *   These are "factory" defaults which are applied only in the event of erasure or
 *   corruption of EEPROM data, and of course in first-time programming.
 */
void  DefaultUserPatch(void)
{
    g_UserPatch.checkDword = 0xDEADBEEF;
    g_UserPatch.EndOfDataBlockCode = 0xE0DBC0DE;

    // Copy PatchProgram[0] from flash to User Patch in EEPROM
    memcpy(&g_UserPatch.Params, &g_PresetPatch[0], sizeof(PatchParamTable_t));

    StoreUserPatch();
}


/*
 *  Function copies data from EEPROM block #0 to a RAM buffer where persistent data
 *  can be accessed by the application.
 *
 *  If the block is erased or the data is found to be corrupt, or if the structure size
 *  exceeds the block size (256 bytes), the function returns FALSE;  otherwise TRUE.
 */
bool  FetchConfigData(void)
{
    BOOL   result = TRUE;

    g_Config.checkDword = 0xFFFFFFFF;
    g_Config.EndOfDataBlockCode = 0xFFFFFFFF;

    EepromReadData((uint8 *) &g_Config, 0, 0, sizeof(EepromBlock0_t));
    
    if (g_Config.checkDword != 0xFEEDFACE) result = FALSE;
    if (g_Config.EndOfDataBlockCode != 0xE0DBC0DE) result = FALSE;

    return result;
}


/*
 *  Function copies data from EEPROM block #1 to a RAM buffer where persistent data
 *  can be accessed by the application.
 *
 *  If the block is erased or the data is found to be corrupt, or if the structure size
 *  exceeds the block size (256 bytes), the function returns FALSE;  otherwise TRUE.
 */
bool  FetchUserPatch(void)
{
    BOOL   result = TRUE;

    g_UserPatch.checkDword = 0xFFFFFFFF;
    g_UserPatch.EndOfDataBlockCode = 0xFFFFFFFF;

    EepromReadData((uint8 *) &g_UserPatch, 1, 0, sizeof(EepromBlock1_t));
    
    if (g_UserPatch.checkDword != 0xDEADBEEF) result = FALSE;
    if (g_UserPatch.EndOfDataBlockCode != 0xE0DBC0DE) result = FALSE;

    return result;
}


/*
 *  Function copies data from a RAM buffer (holding current working
 *  values of persistent parameters) to the EEPROM block #0.
 *  <!> The size of the structure g_Config must not exceed 256 bytes.
 *
 *  Return val:  TRUE if the operation was successful, else FALSE.
 */
bool  StoreConfigData()
{
    int    promAddr = 0;
    int    bytesToCopy = sizeof(EepromBlock0_t);
    int    chunkSize;  // max. 16 bytes
    uint8  *pData = (uint8 *) &g_Config;
    BOOL   result = TRUE;

    if (sizeof(g_Config) > 256)
    {
        bytesToCopy = 256;
        result = FALSE;
    }

    while (bytesToCopy > 0)
    {
        chunkSize = (bytesToCopy >= 16) ? 16 : (bytesToCopy % 16);
        if (EepromWriteData(pData, 0, promAddr, chunkSize) == ERROR)
        {
            result = FALSE;
            break;
        }
        promAddr += 16;
        pData += 16;
        bytesToCopy -= 16;
    }

    return result;
}


/*
 *  Function copies data from a RAM buffer (holding current working
 *  values of persistent parameters) to the EEPROM block #1.
 *  <!> The size of the structure g_UserPatch must not exceed 256 bytes.
 *
 *  Return val:  TRUE if the operation was successful, else FALSE.
 */
bool  StoreUserPatch()
{
    int    promAddr = 0;
    int    bytesToCopy = sizeof(EepromBlock1_t);
    int    chunkSize;  // max. 16 bytes
    uint8  *pData = (uint8 *) &g_UserPatch;
    BOOL   result = TRUE;

    if (sizeof(g_UserPatch) > 256)
    {
        bytesToCopy = 256;
        result = FALSE;
    }

    while (bytesToCopy > 0)
    {
        chunkSize = (bytesToCopy >= 16) ? 16 : (bytesToCopy % 16);
        if (EepromWriteData(pData, 1, promAddr, chunkSize) == ERROR)
        {
            result = FALSE;
            break;
        }
        promAddr += 16;
        pData += 16;
        bytesToCopy -= 16;
    }

    return result;
}


/*
 *  Utility to list the active patch parameter values via the 'console' serial port.
 *  Output text is in C source code format, suitable for importing into the array of
 *  preset patch definitions: (PatchParamTable_t) g_PresetPatch[].  Example output:
 * 
 *       { 1, 2, 5, 7, 9, 11 },             // Osc Freq Mult (6)
 *       { 0, 0, 0, 0, 0, 0 },              // Osc Ampld Modn Source (6)
 *       { 0, 0, 0, 0, 0, 0 },              // Osc Detune, cents (6)
 *       { 13, 0, 11, 10, 9, 8 },           // Mixer Input levels (6)
 *       5, 0, 200, 80, 200, 0,             // Ampld Envelope A-H-D-S-R-V
 *       5, 20, 500, 95,                    // Contour Envelope S-D-R-H
 *       500, 50,                           // Transient/ENV2 Decay, Sus %
 *       50, 500, 50, 20,                   // LFO Freq, Ramp, FM %st, AM %
 *       50, 3                              // Mixer Gain (x10), Ampld Ctrl
 * 
 *  The function is invoked from the GUI "Misc Utilities" menu screen.
 *  The console port uses baud rate = 57600.  Set PC terminal app accordingly.
 */
void  ListActivePatch(void)
{
    putNewLine();
    ListParamsFromArray((short *) &g_Patch.OscFreqMult[0], 6, 1);
    putstr("Osc Freq Mult index (0..11)\n");
    
    ListParamsFromArray((short *) &g_Patch.OscAmpldModSource[0], 6, 1);
    putstr("Osc Ampld Modn src (0..7)\n");
    
    ListParamsFromArray((short *) &g_Patch.OscDetune[0], 6, 1);
    putstr("Osc Detune cents (+/-600)\n");
    
    ListParamsFromArray((short *) &g_Patch.MixerInputStep[0], 6, 1);
    putstr("Osc Mixer levels (0..16)\n");
    
    ListParamsFromArray((short *) &g_Patch.EnvAttackTime, 6, 0);
    putstr("Ampld Env (A-H-D-S-R-V) \n");
    
    ListParamsFromArray((short *) &g_Patch.ContourStartLevel, 4, 0);
    putstr("Contour Env (S-D-R-H) \n");
    
    ListParamsFromArray((short *) &g_Patch.Env2DecayTime, 2, 0);
    putstr("ENV2: Dec, Sus % \n");
    
    ListParamsFromArray((short *) &g_Patch.LFO_Freq_x10, 4, 0);
    putstr("LFO: Hz x10, Ramp, FM %, AM %\n");
    
    ListParamsFromArray((short *) &g_Patch.MixerOutGain_x10, 2, 0);
    putstr("Mixer Gain x10, Ampld Ctrl\n");
}


void  ListParamsFromArray(short *sourceData, short paramCount, bool putBraces)
{
    char  numBuf[20], outBuff[100];
    uint8  padSize, n, b;
    
    if (putBraces) strcpy(outBuff, "        { ");  // indent 8 places, put brace
    else  strcpy(outBuff, "        ");  // indent but no brace
    
    for (n = 0;  n < paramCount;  n++)
    {
        sprintf(numBuf, "%d", (int) sourceData[n]);
        strcat(outBuff, numBuf);
        if (!putBraces || (n < 5)) strcat(outBuff, ", ");
    }
    if (putBraces) strcat(outBuff, " }, ");  

    padSize = 40 - strlen(outBuff);  // pad to column 41
    for (b = 0;  b < padSize;  b++)  { strcat(outBuff, " "); }
    strcat(outBuff, "// ");
    putstr(outBuff);
}
