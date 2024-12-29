/****************************************************************************************
 *
 * FileName:   sigma6_synth_main.h 
 *
 * File contains build options and particular def's for the application.
 * See also: "HardwareProfile.h"
 *
 * =======================================================================================
 */
#ifndef SIGMA6_SYNTH_MAIN_H
#define SIGMA6_SYNTH_MAIN_H

// =======================================================================================
//                       FIRMWARE VERSION NUMBER AND BUILD OPTIONS
//
#define BUILD_VER_MAJOR   1
#define BUILD_VER_MINOR   1
#define BUILD_VER_DEBUG   50
//
// =======================================================================================

#include "../Drivers/EEPROM_drv.h"
#include "../Drivers/UART_drv.h"
#include "../Drivers/SPI_drv.h"
#include "../Drivers/I2C_drv.h"

#ifdef __32MX440F256H__
#include "pic32mx440_low_level.h"
#else
#include "pic32_low_level.h"
#endif

#include "LCD_graphics_lib.h"
#include "MIDI_comms_lib.h"
#include "sigma6_synth_def.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

// These macros are aliases for UART2 serial I/O functions:
#define RxDataAvail()   UART2_RxDataAvail()
#define RxFlush()       UART2_RxFlush()
#define getch()         UART2_getch()
#define putch(b)        UART2_putch(b)
#define putstr(s)       UART2_putstr(s)     
#define kbhit()         RxDataAvail()
#define putNewLine()    putstr("\n")
#define NEW_LINE        putstr("\n")   // prefer putNewLine()

#if UART1_TX_USING_QUEUE
#define MidiOutputQueueHandler()  UART1_TxQueueHandler() 
#else
#define MidiOutputQueueHandler()  {}
#endif

// global data
extern  uint8   g_FW_version[];          // firmware version # (major, minor, build, 0)
extern  char   *g_AppTitleCLI;           // string printed by "ver"  command
extern  int     g_SoftTimerError;        // % error
extern  uint8   g_SelfTestFault[];       // Self-test fault codes (0 => no fault)
extern  uint32  g_counter;               // temp, for debug
extern  uint32  g_TaskCallCount;         // Debug usage
extern  uint32  g_TaskCallFrequency;     //  ..    ..
extern  bool    g_MidiRxSignal;          // Signal MIDI message received
extern  bool    g_MidiRxTimeOut;         // Signal MIDI message time-out (3 sec)


enum  Self_test_categories
{
    TEST_SOFTWARE_TIMER = 0,
    TEST_DEVICE_ID,
    TEST_MIDI_IN_COMMS,
    TEST_EEPROM,
    TEST_LCD_MODULE,
    NUMBER_OF_SELFTEST_ITEMS
};

typedef struct Eeprom_block0_structure
{
    uint32  checkDword;               // Constant value used to check data integrity
    ////
    uint8   MidiMode;                 // MIDI IN mode (Omni on/off, mono)
    uint8   MidiChannel;              // MIDI IN channel, dflt: 1
    uint8   MidiExpressionCCnum;      // MIDI IN breath/pressure CC number, dflt 2
    uint8   AudioAmpldCtrlMode;       // Override patch param AmpldControlSource
    uint8   VibratoCtrlMode;          // Vibrato Control Mode, dflt: 0 (Off)
    uint8   PitchBendEnable;          // Pitch Bend Enable (0: disabled)
    uint8   PitchBendRange;           // Pitch Bend range, semitones (1..12)
    uint8   ReverbAtten_pc;           // Reverberation attenuator gain (1..100 %)
    uint8   ReverbMix_pc;             // Reverberation wet/dry mix (1..100 %)
    uint8   PresetLastSelected;       // Preset last selected (0..127)
    ////
    uint32  EndOfDataBlockCode;       // Last entry, used to test if format has changed

} EepromBlock0_t;


typedef struct Eeprom_block1_structure
{
    uint32 checkDword;
    ////
    PatchParamTable_t  Params;        // User-programmable patch parameters
    ////
    uint32 EndOfDataBlockCode;

} EepromBlock1_t;

extern  EepromBlock0_t  g_Config;     // structure holding configuration data
extern  EepromBlock1_t  g_UserPatch;  // structure holding User Patch param's


// Functions defined in file: "sigma6_synth_main.c" ...
//
void  BackgroundTaskExec();
void  MidiInputService();
void  PresetSelect(uint8 preset);
bool  isLCDModulePresent();
void  DefaultConfigData(void);
void  DefaultUserPatch(void);
bool  FetchConfigData(void);
bool  FetchUserPatch(void);
bool  StoreConfigData(void);
bool  StoreUserPatch(void);
void  ListActivePatch(void);
void  ListParamsFromArray(short *src, short count, bool braces);

// Formatted numeric-string output functions...
void  putBoolean( uint8 );
void  putHexDigit( uint8 );
void  putHexByte( uint8 );
void  putHexWord( uint16 );
void  putHexLong( uint32 );
void  putDecimal(int32 iVal, uint8 fieldWidth);

// Defined in file: "sigma6_synth_data.c" ...
int   GetNumberOfPresets(void);


#endif // SIGMA6_SYNTH_MAIN_H
