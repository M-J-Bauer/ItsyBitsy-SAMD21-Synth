/*^**************************************************************************************
 *
 * Module:      sigma6_synth_panel.c
 *
 * Overview:    Control panel functions for the Sigma-6 Sound Synth...
 *              GLCD module (128 x 64 px), 6 push-buttons and 6 potentiometers.
 *
 * Author:      M.J.Bauer, Copyright 2022++  All rights reserved
 *
 * ======================================================================================
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "sigma6_synth_main.h"
#include "sigma6_synth_def.h"
#include "sigma6_synth_panel.h"

void  DisplayTitleBar(uint16 scnIndex);
void  ScreenFunc_Startup(bool);
void  ScreenFunc_SelfTestReport(bool);
void  ScreenFunc_Home(bool);
void  ScreenFunc_SelectPreset(bool);
void  ScreenFunc_DiagnosticInfo(bool);
void  ScreenFunc_SaveUserPatch(bool);
void  ScreenFunc_MiscUtilityMenu(bool);
void  ScreenFunc_RestoreDefaultConfig(bool);

void  ScreenFunc_OscFreqControls(bool);
void  ScreenFunc_MixerLevelControls(bool);
void  ScreenFunc_OscDetuneControls(bool);
void  ScreenFunc_OscModnControls(bool);
void  ScreenFunc_EnvelopeControls(bool);
void  ScreenFunc_ContourEnvControls(bool);
void  ScreenFunc_OtherControls(bool);

void  ScreenFunc_SetAudioControlMode(bool);
void  ScreenFunc_SetVibratoMode(bool);
void  ScreenFunc_SetPitchBendMode(bool);
void  ScreenFunc_SetReverbControls(bool);
void  ScreenFunc_MIDI_Settings(bool);

void  SerialDiagnosticOutput();

/*
 * Bitmap image definition
 * Image name: sigma_6_icon_24x21, width: 24, height: 21 pixels
 */
bitmap_t  sigma_6_icon_24x21[] =
{
    0x00, 0x00, 0x7C, 0x00, 0x01, 0xFC, 0x00, 0x03, 0xFC, 0x00, 0x03, 0xC0, 0x00, 0x07, 0x80, 0x00,
    0x07, 0x00, 0x00, 0x07, 0x00, 0x07, 0xF7, 0xF0, 0x1F, 0xF7, 0xFC, 0x3F, 0xF7, 0xFE, 0x71, 0x87,
    0x8E, 0x71, 0xC7, 0x0F, 0xE0, 0xE7, 0x07, 0xE0, 0xE7, 0x07, 0xE0, 0xE7, 0x07, 0xE0, 0xE7, 0x07,
    0xF1, 0xE7, 0x8F, 0x71, 0xC3, 0x8E, 0x7F, 0xC3, 0xFE, 0x3F, 0x81, 0xFC, 0x0E, 0x00, 0x70
};

/*
 * Bitmap image definition
 * Image name: flat_up_arrow_8x4, width: 8, height: 4 pixels
 */
bitmap_t  flat_up_arrow_8x4[] =
{
    0x18, 0x3C, 0x66, 0xC3
};

/*
 * Bitmap image definition
 * Image name: flat_down_arrow_8x4, width: 8, height: 4 pixels
 */
bitmap_t  flat_down_arrow_8x4[] =
{
    0xC3, 0x66, 0x3C, 0x18
};

/*
 * Bitmap image definition
 * Image name: patch_icon_7x7,  width: 7, height: 7 pixels
 */
bitmap_t  patch_icon_7x7[] =
{
    0x54, 0xFE, 0x54, 0xFE, 0x54, 0xFE, 0x54
};

/*
 * Bitmap image definition
 * Image name: midi_conn_icon_9x9,  width: 9, height: 9 pixels
 */
bitmap_t  midi_conn_icon_9x9[] =
{
    0x3E, 0x00, 0x77, 0x00, 0xDD, 0x80, 0xFF, 0x80, 0xBE, 0x80,
    0xFF, 0x80, 0xFF, 0x80, 0x7F, 0x00, 0x3E, 0x00
};


static  bool    m_screenSwitchDone;      // flag: Screen switch occurred
static  uint32  m_lastUpdateTime;        // captured timer value (ms)
static  uint16  m_CurrentScreen;         // ID number of current screen displayed
static  uint16  m_PreviousScreen;        // ID number of previous screen displayed
static  uint16  m_NextScreen;            // ID number of next screen to be displayed
static  bool    m_ScreenSwitchFlag;      // Trigger to change to next screen
static  uint32  m_ElapsedTime_ms;        // Time elapsed since last key hit (ms)
static  int     m_presetLastShown;       // Preset # of last display update

extern  volatile  uint32 v_ISRexecTime;  // ISR execution time (CPU cycles)

static uint8  percentQuantized[] =       // 16 values, 3dB log scale (approx.)
        { 0, 1, 2, 3, 4, 5, 8, 10, 12, 16, 25, 35, 50, 70, 100, 100  };

static uint16 timeValueQuantized[] =     // 16 values, logarithmic scale
        { 0, 10, 20, 30, 50, 70, 100, 200, 300, 500, 700, 1000, 1500, 2000, 3000, 5000 };

// Screen descriptors (below) may be arranged in any arbitrary order in the array
// m_ScreenDesc[], i.e. the table doesn't need to be sorted into screen_ID order.
// Function ScreenIndexFind() is used to find the index of an element within
// the array m_ScreenDesc[], for a specified screen_ID.
//
static  const  ScreenDescriptor_t  m_ScreenDesc[] =
{
    {
        SCN_STARTUP,                 // screen ID
        ScreenFunc_Startup,          // screen update function
        NULL                         // title bar text (none)
    },
    {
        SCN_SELFTEST_REPORT,         // screen ID
        ScreenFunc_SelfTestReport,   // screen update function  SCN_CTRL_POTS_TEST
        "SELF-TEST FAIL"             // title bar text
    },
    {
        SCN_HOME,
        ScreenFunc_Home,
        NULL
    },
    {
        SCN_SELECT_PRESET,
        ScreenFunc_SelectPreset,
        "SELECT PRESET"
    },
    {
        SCN_SAVE_USER_PATCH,
        ScreenFunc_SaveUserPatch,
        "CONFIRM"
    },  
    {
        SCN_DIAGNOSTIC_INFO,
        ScreenFunc_DiagnosticInfo,
        "Diagnostic Info"
    },    
    {
        SCN_MISC_UTILITY,
        ScreenFunc_MiscUtilityMenu,
        "MISC UTILITY"
    },  
    {
        SCN_DEFAULT_CONFIG,
        ScreenFunc_RestoreDefaultConfig,
        "CONFIRM"
    },  
    // Patch screens
    {
        SCN_CTRL_OSC_FREQ,
        ScreenFunc_OscFreqControls,
        "OSC FREQ MULT"
    },
    {
        SCN_CTRL_MIXER_LEVELS,
        ScreenFunc_MixerLevelControls,
        "OSC MIXER LEVELS"
    },
    {
        SCN_CTRL_OSC_DETUNE,
        ScreenFunc_OscDetuneControls,
        "OSC DETUNE (cents)"
    },
    {
        SCN_CTRL_OSC_MODN,
        ScreenFunc_OscModnControls,
        "OSC MOD'N SOURCE"
    },
    {
        SCN_CTRL_CONTOUR_PARAMS,
        ScreenFunc_ContourEnvControls,
        "CONTOUR & ENV2"
    },	
    {
        SCN_CTRL_ENVELOPE_PARAMS,
        ScreenFunc_EnvelopeControls,
        "AMPLD ENVELOPE"
    },
    {
        SCN_CTRL_OTHER_PARAMS,
        ScreenFunc_OtherControls,
        "LFO & AMPLD CTRL"
    },
    // Config screens
    {
        SCN_SET_AUDIO_CTRL_MODE,
        ScreenFunc_SetAudioControlMode,
        "AUDIO O/P CONTROL"
    },
    {
        SCN_SET_REVERB_PARAMS,
        ScreenFunc_SetReverbControls,
        "REVERB"
    },
    {
        SCN_SET_VIBRATO_MODE,
        ScreenFunc_SetVibratoMode,
        "VIBRATO"
    },
    {
        SCN_SET_PITCH_BEND_MODE,
        ScreenFunc_SetPitchBendMode,
        "PITCH BEND"
    },
    {
        SCN_SET_MIDI_PARAMS,
        ScreenFunc_MIDI_Settings,
        "MIDI SETTINGS"
    }
};

#define NUMBER_OF_SCREENS_DEFINED  ARRAY_SIZE(m_ScreenDesc)

/*=================================================================================================
 *
 * Function returns the number of screens defined (with initialized data) in the
 * array of screen descriptors, m_ScreenDesc[].
 */
int  GetNumberOfScreensDefined()
{
    return  ARRAY_SIZE(m_ScreenDesc);
}

/*
 * Function returns the screen ID number of the currently displayed screen.
 */
uint16  GetCurrentScreenID()
{
    return  m_CurrentScreen;
}

/*
 * Function returns the screen ID number of the previously displayed screen.
 */
uint16  GetPreviousScreenID()
{
   return  m_PreviousScreen;
}

/*
 * Function triggers a screen switch to a specified new screen.
 * The real screen switch business is done by the routine ControlPanelService(),
 * when next executed following the call to GoToNextScreen().
 *
 * Entry arg:  nextScreenID = ID number of next screen required.
 */
void  GoToNextScreen(uint16 nextScreenID)
{
    m_NextScreen = nextScreenID;
    m_ScreenSwitchFlag = 1;
}

/*=================================================================================================
 *
 * Control Panel Service Routine
 *
 * Function called frequently from the main application loop.
 */
void  ControlPanelService(void)
{
    static bool    prep_done;
    static uint32  intervalStartTime_ms;
    short  current, next;  // index values of current and next screens
    
    // Check that LCD module is present and operational -- if not bail...
    if (!isLCDModulePresent())  return;

    if (!prep_done)
    {
        m_CurrentScreen = SCN_STARTUP;
        m_PreviousScreen = SCN_STARTUP;
        m_ScreenSwitchFlag = 1;
        m_screenSwitchDone = 0;
        intervalStartTime_ms = milliseconds();
        prep_done = TRUE;
    }

    if (m_ScreenSwitchFlag)   // Screen switch requested
    {
        m_ScreenSwitchFlag = 0;
        m_ElapsedTime_ms = 0;
        m_lastUpdateTime = milliseconds();
        next = ScreenDescIndexFind(m_NextScreen);

        if (next < NUMBER_OF_SCREENS_DEFINED)  // found next screen ID
        {
            m_PreviousScreen = m_CurrentScreen;     // Make the switch...
            m_CurrentScreen = m_NextScreen;         // next screen => current

            if (m_NextScreen != m_PreviousScreen)
            {
                LCD_ClearScreen();

                if (m_ScreenDesc[next].TitleBarText != NULL)
                    DisplayTitleBar(next);
            }

            (*m_ScreenDesc[next].ScreenFunc)(1);  // Render new screen
            m_screenSwitchDone = TRUE;
        }
    }
    else  // no screen switch -- check update timer
    {
        if ((milliseconds() - m_lastUpdateTime) >= SCREEN_UPDATE_INTERVAL)
        {
            current = ScreenDescIndexFind(m_CurrentScreen);

            (*m_ScreenDesc[current].ScreenFunc)(0);  // Update current screen

            m_lastUpdateTime = milliseconds();
            m_ElapsedTime_ms += SCREEN_UPDATE_INTERVAL;
        }
    }
    
    if (POT_MODULE_CONNECTED) ControlPotService();

    if ((milliseconds() - intervalStartTime_ms) >= 6)  // Every 6 ms...
    {
        intervalStartTime_ms = milliseconds();
        ButtonInputService();
    }
}


/*
 * Function returns TRUE if a screen switch has occurred since the previous call.
 */
bool  ScreenSwitchOccurred(void)
{
    bool  result = m_screenSwitchDone;

    m_screenSwitchDone = FALSE;
    return result;
}


/*
 * Function returns the index of a specified screen in the array of Screen Descriptors,
 * (ScreenDescriptor_t)  m_ScreenDesc[].
 *
 * Entry arg(s):  search_ID = ID number of required screen descriptor
 *
 * Return value:  index of screen descriptor in array m_ScreenDesc[], or...
 *                NUMBER_OF_SCREENS_DEFINED, if search_ID not found.
 */
int  ScreenDescIndexFind(uint16 searchID)
{
    int   index;

    for (index = 0; index < NUMBER_OF_SCREENS_DEFINED; index++)
    {
        if (m_ScreenDesc[index].screen_ID == searchID)  break;
    }

    return index;
}


/*
 * This function displays a single-line menu option, i.e. keytop image plus text string.
 * The keytop image is simply a square with a character drawn inside it in reverse video.
 * The given text string is printed just to the right of the keytop image.
 * The character font(s) used are fixed within the function.
 *
 * Entry args:   x = X-coord of keytop image (2 pixels left of key symbol)
 *               y = Y-coord of keytop symbol, same as text to be printed after
 *               symbol = ASCII code of keytop symbol (5 x 7 mono font)
 *               text = string to print to the right of the keytop image
 */
void  DisplayMenuOption(uint16 x, uint16 y, char symbol, char *text)
{
    uint16  xstring = x + 12;  // x-coord on exit

    LCD_Mode(SET_PIXELS);
    LCD_PosXY(x, y-1);
    LCD_DrawBar(9, 9);

    LCD_SetFont(MONO_8_NORM);
    LCD_Mode(CLEAR_PIXELS);
    LCD_PosXY(x+2, y);
    if (symbol > 0x20) LCD_PutChar(symbol);

    LCD_SetFont(PROP_8_NORM);
    LCD_Mode(SET_PIXELS);
    LCD_PosXY(xstring, y);
    if (text != NULL) LCD_PutText(text);
}

/*
 * This function displays a text string (str) centred in a specified field width (nplaces)
 * using 8pt mono-spaced font, at the specified upper-left screen position (x, y).
 * On exit, the display write mode is restored to 'SET_PIXELS'.
 */
void  DisplayTextCenteredInField(uint16 x, uint16 y, char *str, uint8 nplaces)
{
    int  len = strlen(str);
    int  i;
    
    if (len > 20) len = 20;
    x += 3 * (nplaces - len);  
    
    LCD_SetFont(MONO_8_NORM);
    LCD_PosXY(x, y);
    
    for (i = 0;  i < len;  i++)
    {
        LCD_PutChar(*str++);
    }
    
    LCD_Mode(SET_PIXELS);
}

/*
 * Function renders the Title Bar (background plus text) of a specified screen.
 * The title bar text (if any) is defined in the respective screen descriptor
 * given by the argument scnIndex. The function is called by ControlPanelService();
 * it is not meant to be called directly by application-specific screen functions.
 *
 * The location and size of the Title Bar and the font used for its text string
 * are fixed inside the function.
 *
 * Entry arg:  scnIndex = index (not ID number!) of respective screen descriptor
 *
 */
void  DisplayTitleBar(uint16 scnIndex)
{
    int i;
    char  *titleString = m_ScreenDesc[scnIndex].TitleBarText;

    LCD_Mode(SET_PIXELS);
    LCD_PosXY(0, 0);
    LCD_BlockFill(128, 10);
    LCD_Mode(CLEAR_PIXELS);
    LCD_PosXY(0, 0);  
    LCD_PutPixel();
    LCD_PosXY(127, 0);  
    LCD_PutPixel();

    if (titleString != NULL)
    {
        LCD_Mode(CLEAR_PIXELS);
        DisplayTextCenteredInField(1, 1, titleString, 21);
    }
}


//-------------------------------------------------------------------------------------------------
// Functions to support user input using 6 push-buttons on the front-panel.
//-------------------------------------------------------------------------------------------------
 
static  BOOL    m_ButtonHitDetected;     // flag: button hit detected
static  uint8   m_ButtonStates;          // Button states, de-glitched (bits 5:0)
static  short   m_ButtonLastHit;         // ASCII code of last button hit

/* 
 * Function:  ButtonInputService()
 *
 * Overview:  Service Routine for 6-button input.
 *
 * Detail:    Background task called periodically at 6ms intervals from the main loop.
 *            The routine reads the button inputs looking for a change in states.
 *            When a button "hit" is detected, the function sets a flag to register the event.
 *            The state of the flag can be read by a call to function ButtonHit().
 *            An ASCII key-code is stored to identify the button last pressed.
 *            The keycode can be read anytime by a call to function ButtonCode().
 */
void  ButtonInputService()
{
    static  short   taskState = 0;  // startup/reset state
    static  uint16  buttonStatesLastRead = 0;
    static  int     debounceTimer_ms = 0;
    uint16  buttonStatesNow;  // 6 LS bits, active HIGH
    
#ifdef __32MX440F256H__  // using PIC32-Pinguino-micro (MX440) module
    buttonStatesNow = ReadButtonInputs();
#else    // assume MX340 variant
    buttonStatesNow = READ_BUTTON_INPUTS() ^ 0x003F;  // macro in pic32_low-level.h
#endif
    
    debounceTimer_ms += 6;

    if (taskState == 0)  // Waiting for all buttons released
    {
        if (buttonStatesNow == ALL_BUTTONS_RELEASED)
        {
            debounceTimer_ms = 0;
            taskState = 3;
        }
    }
    else if (taskState == 1)  // Waiting for any button(s) pressed
    {
        if (buttonStatesNow != ALL_BUTTONS_RELEASED)
        {
            buttonStatesLastRead = buttonStatesNow;
            debounceTimer_ms = 0;
            taskState = 2;
        }
    }
    else if (taskState == 2)  // De-bounce delay after hit (30ms)
    {
        if (buttonStatesNow != buttonStatesLastRead)
            taskState = 1;    // glitch -- retry

        if (debounceTimer_ms >= 30)
        {
            m_ButtonHitDetected = 1;
            m_ButtonStates = buttonStatesNow;
            if (m_ButtonStates & MASK_BUTTON_A)  m_ButtonLastHit = 'A';
            else if (m_ButtonStates & MASK_BUTTON_B)  m_ButtonLastHit = 'B';
            else if (m_ButtonStates & MASK_BUTTON_C)  m_ButtonLastHit = 'C';
            else if (m_ButtonStates & MASK_BUTTON_D)  m_ButtonLastHit = 'D';
            else if (m_ButtonStates & MASK_BUTTON_STAR)  m_ButtonLastHit = '*';
            else if (m_ButtonStates & MASK_BUTTON_HASH)  m_ButtonLastHit = '#';
            else  m_ButtonLastHit = 0;  // NUL
            m_ElapsedTime_ms = 0;  // reset screen timeout
            taskState = 0;
        }
    }
    else if (taskState == 3)  // De-bounce delay after release (150ms)
    {
        if (buttonStatesNow != ALL_BUTTONS_RELEASED)  // glitch - retry
            taskState = 0;

        if (debounceTimer_ms >= 150)
        {
            m_ButtonStates = buttonStatesNow;
            taskState = 1;
        }
    }
}

/*
 * Function:     ButtonStates()
 *
 * Overview:     Returns the current input states of the 6 buttons, de-bounced,
 *               as 6 LS bits of the data.  A button pressed is represented by a High bit.
 *
 * Return val:   (uint8) m_ButtonStates
 */
uint8  ButtonStates(void)
{
    return  m_ButtonStates;
}

/*
 * Function:     uint8 ButtonHit()
 *
 * Overview:     Tests for a button hit, i.e. transition from not pressed to pressed.
 *               A private flag, m_ButtonHitDetected, is cleared on exit so that the
 *               function will return TRUE once only for each button press event.
 *
 * Return val:   TRUE (1) if a key hit was detected since the last call, else FALSE (0).
 */
bool  ButtonHit(void)
{
    bool  result = m_ButtonHitDetected;

    m_ButtonHitDetected = FALSE;

    return  result;
}

/*
 * Function:     short ButtonCode()
 *
 * Overview:     Returns the ASCII keycode of the last button press detected, i.e.
 *               following a call to function ButtonHit() which returned TRUE.
 *
 * Note:         ButtonCode() may be called multiple times after a call to ButtonHit().
 *               The function will return the same keycode on each call, up to
 *               100 milliseconds after the last button hit registered.
 *
 * Return val:   (uint8) ASCII keycode, one of: 'A', 'B', 'C', 'D', '*', or '#'.
 */
uint8  ButtonCode(void)
{
    return  m_ButtonLastHit;
}


//-------------------------------------------------------------------------------------------------
// Functions to support up to 6 control potentiometers on the front-panel.
//-------------------------------------------------------------------------------------------------
static  int32  m_PotReadingAve[6];  // Rolling average of pot readings [24:8 fixed-pt]
static  bool   m_PotMoved[6];  // Flags: Pot reading changed since last read

/*
 * Function:  ControlPotService()
 *
 * Overview:  Service Routine for 6 front-panel control pots.
 *            Non-blocking "task" called frequently as possible.
 *
 * Detail:    The routine reads the pot inputs and keeps a rolling average of raw ADC
 *            readings in fixed-point format (16:16 bits).  Reading range is 0.0 ~ 1023.0
 *            Each pot reading is compared with its respective reading on the previous pass.
 *            If a change of more than 2% (approx) is found, then a flag is raised.
 * 
 *            The state of the flag can be read by a call to function PotMoved(potnum).
 *            The current pot position can be read by a call to function PotReading(n).
 * 
 * Outputs:   (bool) m_PotMoved[6],  (int32) m_PotReadingAve[6].
 * 
 */
void  ControlPotService()
{
    static uint8  potInput[] = POT_CHANNEL_LIST;  // defined in pic32_low_level.h
    static uint32 startInterval_3ms;
    static uint32 startInterval_37ms;
    static bool   prep_done;
    static short  potSel;
    static int32  pastReading[6];  // readings on past scan
    int32  potReading;
    uint8  potRand;
    
    if (!prep_done)  // One-time initialization at power-on/reset
    {
        startInterval_3ms = milliseconds();
        startInterval_37ms = milliseconds();
        potSel = 0;
        prep_done = TRUE;
    }
    
    // Compute rolling average of pot ADC readings -- sampling period = 18ms per pot
    if ((milliseconds() - startInterval_3ms) >= 3)
    {
        potReading = (int32) AnalogResult(potInput[potSel]);  // get 10 bit raw result
        potReading = potReading << 16;  // convert to fixed-point (16:16 bits)

        // Apply rolling average algorithm (1st-order IIR filter, K = 0.25)
        m_PotReadingAve[potSel] -= m_PotReadingAve[potSel] >> 2;
        m_PotReadingAve[potSel] += potReading >> 2;

        if (++potSel >= 6)  potSel = 0;  // next pot
        startInterval_3ms = milliseconds();
    }
    
    // Every 23ms*, choose a pot at random^, check if it has been moved.
    // The 6 pots will be serviced in under 150ms (on average).
    // [*Interval not critical. ^Why random, not cyclic? - Because it works!]
    if ((milliseconds() - startInterval_37ms) >= 23)
    {
        potRand = rand() % 6;  // 0..5
        if (abs(m_PotReadingAve[potRand] - pastReading[potRand]) > (20 << 16))  // 20 => 2%
        {
            m_PotMoved[potRand] = TRUE;
            pastReading[potRand] = m_PotReadingAve[potRand];  // update
        }
        startInterval_37ms = milliseconds();
    }
}

/*
 * Function:     PotFlagsClear()
 *
 * Overview:     Clears all (6) "pot moved" flags in array m_PotMoved[].
 * 
 * Note:         To clear one individual flag, simply call PotMoved(num).
 */
void  PotFlagsClear()
{
    short  i;
    
    for (i = 0;  i < 6;  i++)  { m_PotMoved[i] = FALSE; }
}

/*
 * Function:     PotMoved()
 *
 * Overview:     Returns a flag (TRUE or FALSE) indicating if the specified control pot
 *               (potnum) position has changed since the previous call to the function
 *               (with the same arg value).
 *
 * Note:         The flag is cleared on exit, so subsequent calls (with the same arg value)
 *               will return FALSE, until the pot position changes again.
 *
 * Arg val:      (uint8) ID number of required pot (0..5)
 *
 * Return val:   (bool) status flag, value = TRUE or FALSE
 */
bool  PotMoved(uint8 potnum)
{
    bool  result = m_PotMoved[potnum];

    if (potnum < 6) m_PotMoved[potnum] = FALSE;
    
    return  result;
}

/*
 * Function:     PotReading()
 *
 * Overview:     Returns the current setting (position) of the specified control pot,
 *               averaged over several ADC readings, as an 8-bit integer.
 *
 * Arg val:      (uint8) ID number of required pot (0..5)
 *
 * Return val:   (uint8) Pot reading, 8 bits unsigned, range 0..255.
 */
uint8  PotReading(uint8 potnum)
{
    return  (uint8) (m_PotReadingAve[potnum] >> 18);  // = (Integer part) / 4
}


/*================================================================================================
 *============  Following are application-specific Screen Functions  =============================
 *
 * These functions are not called directly by the application. They are called by function
 * ControlPanelService() with arg isNewScreen = TRUE on the first call following a screen
 * switch to render initial text and images (if any);  isNewScreen = FALSE on subsequent
 * periodic calls to refresh screen information and to monitor events, usually button hits
 * which need to be actioned. Some events may cause a screen switch.
 *
 */
void  ScreenFunc_Startup(bool isNewScreen)
{
    int   i;
    bool  isFailedSelfTest;

    if (isNewScreen)  // Render static display info
    {
        LCD_Mode(SET_PIXELS);
        LCD_PosXY(0, 0);
        LCD_BlockFill(28, 25);
        LCD_Mode(CLEAR_PIXELS);
        LCD_PosXY(2, 2);
        LCD_PutImage(sigma_6_icon_24x21, 24, 21);
        LCD_Mode(SET_PIXELS);
        
        LCD_SetFont(PROP_12_BOLD);
        LCD_PosXY(32, 2);
        LCD_PutText("Sigma 6");
        
        LCD_SetFont(MONO_8_NORM);
        LCD_PosXY(32, 16);
        LCD_PutText("monosynth");
        LCD_PosXY(12, 32);
        LCD_PutText("www.mjbauer.biz");
        
        LCD_PosXY(0, 43);
        LCD_DrawLineHoriz(128);

        LCD_SetFont(PROP_8_NORM);
        LCD_PosXY(3, 56);
        LCD_PutText("Running self-test...");
    }
    else  // do periodic update...
    {
        if (m_ElapsedTime_ms >= SELF_TEST_WAIT_TIME_MS)
        {
            isFailedSelfTest = 0;
            // Check self-test results... if fail, go to test results screen
            for (i = 0;  i < NUMBER_OF_SELFTEST_ITEMS;  i++)
            {
                if (g_SelfTestFault[i] != 0) isFailedSelfTest = TRUE;
            }

            if (isFailedSelfTest)  GoToNextScreen(SCN_SELFTEST_REPORT);
            else  GoToNextScreen(SCN_HOME);
        }
    }
}


void  ScreenFunc_SelfTestReport(bool isNewScreen)
{
    static  char  *SelfTestName[] = { "Software Timer", "MCU device ID", "MIDI comm's",
                                      "EEPROM defaulted", NULL, NULL, NULL };
    int     i;
    uint16  y;

    if (isNewScreen)  // Render static display info
    {
        LCD_SetFont(PROP_8_NORM);
        LCD_Mode(SET_PIXELS);
        LCD_PosXY(0, 53);
        LCD_DrawLineHoriz(128);
        DisplayMenuOption(0, 56, '*', "Restart");
        DisplayMenuOption(80, 56, '#', "Ignore");

        for ((y = 12, i = 0);  i < NUMBER_OF_SELFTEST_ITEMS;  i++)
        {
            if (g_SelfTestFault[i])  // this test failed...
            {
                LCD_PosXY(10, y);
                LCD_PutText(SelfTestName[i]);
                y = y + 10;
            }
            if (y >= (12 + 40))  break;  // screen full
        }
    }
    else  // do periodic update...
    {
        if (ButtonHit())
        {
            if (ButtonCode() == '*') BootReset();
            else if (ButtonCode() == '#') GoToNextScreen(SCN_HOME);
        }
    }
}


void  ScreenFunc_Home(bool isNewScreen)
{
    static  uint8  lastPresetShown;
    short   xpos = 112;
    uint8   preset_id = g_Config.PresetLastSelected;

    if (isNewScreen)  // Render static display info
    {
        LCD_Mode(SET_PIXELS);
        LCD_PosXY(0, 0);
        LCD_BlockFill(28, 25);
        LCD_Mode(CLEAR_PIXELS);
        LCD_PosXY(2, 2);
        LCD_PutImage(sigma_6_icon_24x21, 24, 21);
        LCD_Mode(SET_PIXELS);
        
        LCD_SetFont(PROP_12_BOLD);
        LCD_PosXY(32, 2);
        LCD_PutText("Sigma 6");
        
        LCD_SetFont(MONO_8_NORM);
        LCD_PosXY(32, 16);
        LCD_PutText("monosynth");
        
        LCD_PosXY(0, 43);
        LCD_DrawLineHoriz(128);

        DisplayMenuOption(0, 46, 'A', "Preset");
        DisplayMenuOption(0, 56, 'B', "Config");
        DisplayMenuOption(48, 46, 'C', "Patch");
        DisplayMenuOption(48, 56, 'D', "Diag");
        DisplayMenuOption(92, 46, '*', "Save");
        DisplayMenuOption(92, 56, '#', "Util");
        lastPresetShown = 255;  // force refresh
    }
    else  // do periodic update...
    {
        if (ButtonHit())
        {
            // Main menu options...
            if (ButtonCode() == 'A')  GoToNextScreen(SCN_SELECT_PRESET);
            if (ButtonCode() == 'B')  GoToNextScreen(SCN_SET_AUDIO_CTRL_MODE);  // 1st Config scn
            if (ButtonCode() == 'C')  GoToNextScreen(SCN_CTRL_OSC_FREQ);  // 1st Patch screen
            if (ButtonCode() == 'D')  GoToNextScreen(SCN_DIAGNOSTIC_INFO);
            if (ButtonCode() == '*')  GoToNextScreen(SCN_SAVE_USER_PATCH);
            if (ButtonCode() == '#')  GoToNextScreen(SCN_MISC_UTILITY);
        }
        
        // Refresh Preset displayed if selection changed...
        if (lastPresetShown != g_Config.PresetLastSelected)
        {
            LCD_SetFont(PROP_8_NORM);
            LCD_PosXY(0, 32);
            LCD_BlockClear(128, 8);  // erase existing line
            LCD_PosXY(2, 32);
            LCD_PutDecimal(g_Config.PresetLastSelected, 2);  // Preset #
            LCD_PosXY(16, 32);
            LCD_PutText((char *) g_PresetPatch[preset_id].PresetName);  // Preset name
            lastPresetShown = g_Config.PresetLastSelected;
        }
        
        if (g_MidiRxSignal)  // if MIDI Rx active, show MIDI connector icon
        {
            LCD_PosXY(112, 2);
            LCD_PutImage(midi_conn_icon_9x9, 9, 9);
            g_MidiRxSignal = FALSE;
        }
        if (g_MidiRxTimeOut)  // if 2 seconds elapsed since last MIDI Rx msg...
        {
            LCD_PosXY(112, 2);
            LCD_BlockClear(10, 10);  // erase MIDI icon
            g_MidiRxTimeOut = FALSE;
        }
    }
}


/*
 * Function allows the user to select a synth patch from the table of presets.
 * A list of four patch names is displayed from the array of predefined patches.
 * The user can select a patch from the list or scroll down to the next 4 presets.
 */
void  ScreenFunc_SelectPreset(bool isNewScreen)
{
    static  int   itop = 0;   // index into g_PatchProgram[], top line of 4 listed
    int     line, ypos;       // line # of displayed/selected patch (0..3)
    int     presetIdx;  // index into Preset table: PatchProgram[]
    char    keyLabel;
    bool    doRefresh = 0;

    if (isNewScreen)  // Render static display info
    {
        LCD_PosXY(0, 53);
        LCD_DrawLineHoriz(128);
        DisplayMenuOption(8, 56, '*', "Exit");
        DisplayMenuOption(88, 56, '#', "Page");
        itop = 0;
        doRefresh = 1;
    }
    else  // do periodic update
    {
        if (ButtonHit())
        {
            if (ButtonCode() == '*')  GoToNextScreen(SCN_HOME);  // exit
            else if (ButtonCode() == '#')    // next page
            {
                itop = itop + 4;
                if (itop >= GetNumberOfPresets())  itop = 0;  // wrap
                doRefresh = 1;
            }
            else if (ButtonCode() >= 'A')  // A, B, C or D -> Preset selected
            {
                line = ButtonCode() - 'A';  // 0, 1, 2 or 3
                presetIdx = itop + line;
                PresetSelect(presetIdx);
                GoToNextScreen(SCN_HOME);  // done
            }
        }
    }

    if (doRefresh)   // List first/next four patch names
    {
        LCD_PosXY(0, 12);
        LCD_BlockClear(128, 40);  // erase existing list

        for (line = 0;  line < 4;  line++)
        {
            if ((itop + line) < GetNumberOfPresets())
            {
                ypos = 12 + (line * 10);
                keyLabel = 'A' + line;
                DisplayMenuOption(0, ypos, keyLabel, "");
                LCD_SetFont(PROP_8_NORM);
                LCD_PosXY(12, ypos);
                LCD_PutDecimalWord(itop+line, 2);  // Preset number
                LCD_PutChar(' ');
                LCD_PutText((uint8 *) g_PresetPatch[itop+line].PresetName);
            }
        }
    }
}


void  ScreenFunc_SaveUserPatch(bool isNewScreen)
{
    static bool  saved;
    
    if (isNewScreen)  // Render static display info
    {
        LCD_Mode(SET_PIXELS);
        LCD_SetFont(PROP_8_NORM);
        LCD_PosXY(8, 22);
        LCD_PutText("Save active patch");
        LCD_PosXY(8, 32);
        LCD_PutText("as User Patch ?");
        
        LCD_PosXY(0, 53);
        LCD_DrawLineHoriz(128);
        DisplayMenuOption(8, 56, '*', "Cancel");
        DisplayMenuOption(88, 56, '#', "Yes");
        saved = FALSE;
    }
    else  // do periodic update...
    {
        if (ButtonHit())
        {
            if (ButtonCode() == '*')  GoToNextScreen(SCN_HOME);  // Cancel
            if (ButtonCode() == '#')  
            {
                // Copy active patch param's to User Patch RAM buffer
                memcpy(&g_UserPatch.Params, &g_Patch, sizeof(PatchParamTable_t));
                StoreUserPatch();   // Write User Patch RAM buffer to EEPROM
                PresetSelect( 0 );  // Save as "last loaded preset"
                saved = TRUE;
                LCD_PosXY(8, 22);
                LCD_BlockClear(120, 20);  // Erase existing text
                LCD_SetFont(PROP_8_NORM);
                LCD_PutText("User Patch saved!");
            }
        }
        
        if (saved && m_ElapsedTime_ms >= 1500)  GoToNextScreen(SCN_HOME);
    }
}


void  ScreenFunc_DiagnosticInfo(bool isNewScreen)
{
    static fixed_t  lastPitchBendFac;
    static int  lastISRduty, timer_500ms;
    char   textBuf[32];
    int  execTime_us = ((int) v_ISRexecTime + 20) / 40;  // @ 40 counts/us
    int  ISR_duty_pc = (execTime_us * 100) / 25;  // duty = % of ISR period (25us)

    if (isNewScreen)  // Render static display info   CONTROL_PANEL_CONNECTED
    {
        sprintf(textBuf, "%d.%d.%02d", g_FW_version[0], g_FW_version[1], g_FW_version[2]);
        LCD_Mode(SET_PIXELS);
        LCD_PosXY(0, 12);
        LCD_SetFont(PROP_8_NORM);
        LCD_PutText("Firmware vn: ");
        LCD_SetFont(MONO_8_NORM);
        LCD_PutText(textBuf);
        
        LCD_SetFont(PROP_8_NORM);
        LCD_PosXY(0, 22);
        LCD_PutText("Control pots: ");
        if (POT_MODULE_CONNECTED) LCD_PutText("Enabled");
        else  LCD_PutText("Disabled");
        
        LCD_PosXY(0, 32);
        LCD_PutText("Audio ISR duty (%): ");
        LCD_PosXY(0, 42);
        LCD_PutText("PitchBend /oct: ");

        LCD_PosXY(0, 53);
        LCD_DrawLineHoriz(128);
        DisplayMenuOption(8,  56, '*', "Exit");
//      DisplayMenuOption(88, 56, '#', "Page");
        lastPitchBendFac = IntToFixedPt(99);  // force refresh
        lastISRduty = 999;
    }
    else  // do periodic update...
    {
        if (ButtonHit())
        {
            if (ButtonCode() == '*')  GoToNextScreen(SCN_HOME);
            if (ButtonCode() == '#')  GoToNextScreen(SCN_HOME);  // next page (todo)
            if (ButtonCode() == 'D')  SerialDiagnosticOutput();  // debug function (hidden)
        }
        
        if (timer_500ms >= 500 && ISR_duty_pc != lastISRduty)
        {
            LCD_SetFont(MONO_8_NORM);
            LCD_PosXY(96, 32);
            LCD_BlockClear(32, 8);  // erase existing
            if (ISR_duty_pc == 0)  LCD_PutText("--");  // v_SynthEnable == 0
            else  LCD_PutDecimal(ISR_duty_pc, 1);
            lastISRduty = ISR_duty_pc;
            timer_500ms = 0;
        }
        else  timer_500ms += SCREEN_UPDATE_INTERVAL;  // typ. 50ms
        
        if (GetPitchBendFactor() != lastPitchBendFac)
        {
            sprintf(textBuf, "%+6.3f", FixedToFloat(GetPitchBendFactor()));
            LCD_SetFont(MONO_8_NORM);
            LCD_PosXY(80, 42);
            LCD_BlockClear(40, 8);  // erase existing
            LCD_PutText(textBuf);
            lastPitchBendFac = GetPitchBendFactor();
        }
    }
}


void  ScreenFunc_MiscUtilityMenu(bool isNewScreen)
{
    static uint32  captureTime_ms;
    static bool  showConfirmation;
    static bool  soundGate;
    
    if (isNewScreen)  // Render static display info
    {
        DisplayMenuOption(8, 12, 'A', " List active patch ");
        DisplayMenuOption(8, 22, 'B', " Sound test (A440) ");
        DisplayMenuOption(8, 32, 'C', " Config'n default  ");
        DisplayMenuOption(8, 42, 'D', " Display dim/bright");
        
        LCD_PosXY(0, 53);
        LCD_DrawLineHoriz(128);
        DisplayMenuOption(8, 56, '*', "Home");
//      DisplayMenuOption(88, 56, '#', "Page");
        showConfirmation = FALSE;
        soundGate = 0;
    }
    else  // do periodic update, monitor button hits
    {
        if (ButtonHit())
        {
            if (ButtonCode() == '*')  GoToNextScreen(SCN_HOME);  // exit
            if (ButtonCode() == '#')  GoToNextScreen(SCN_HOME);  // exit
            
            if (ButtonCode() == 'A')  ListActivePatch();
            if (ButtonCode() == 'B')  // Sound test (play note 69 = A440)
            {
                LCD_SetFont(PROP_8_NORM);
                LCD_PosXY(64, 56);
                LCD_PutText("Note Playing");
                SynthNoteOn(69, 80);
                SynthExpression(8000);  // FS = 16K
                soundGate = 1;
            }
            if (ButtonCode() == 'C')  GoToNextScreen(SCN_DEFAULT_CONFIG);
            if (ButtonCode() == 'D')  ToggleBacklight();
        }
        
    }
    // Show message for 1.5 second
    if (showConfirmation && (milliseconds() - captureTime_ms) > 1500)  
    {
        showConfirmation = FALSE;
        LCD_PosXY(64, 56);
        LCD_BlockClear(96, 8);  // erase message
    }
    
    if (soundGate && (m_ButtonStates & MASK_BUTTON_B) == 0)
    {
        SynthNoteOff(69);
        SynthExpression(0);
        soundGate = 0;
        LCD_PosXY(64, 56);
        LCD_BlockClear(96, 8);  // erase message
    }
}


void  ScreenFunc_RestoreDefaultConfig(bool isNewScreen)
{
    static bool  done;
    
    if (isNewScreen)  // Render static display info
    {
        LCD_Mode(SET_PIXELS);
        LCD_SetFont(PROP_8_NORM);
        LCD_PosXY(8, 12);
        LCD_PutText("Restore configuration");
        LCD_PosXY(8, 22);
        LCD_PutText("parameters to factory");
        LCD_PosXY(8, 32);
        LCD_PutText("default settings ?");
        
        LCD_PosXY(0, 53);
        LCD_DrawLineHoriz(128);
        DisplayMenuOption(8, 56, '*', "Cancel");
        DisplayMenuOption(88, 56, '#', "Yes");
        done = FALSE;
    }
    else  // do periodic update...
    {
        if (ButtonHit())
        {
            if (ButtonCode() == '*')  GoToNextScreen(SCN_MISC_UTILITY);  // Cancel
            if (ButtonCode() == '#')  
            {
                LCD_PosXY(8, 12);
                LCD_BlockClear(120, 30);  // Erase existing text
                LCD_SetFont(PROP_8_NORM);
                LCD_PosXY(8, 32);
                LCD_PutText("Config restored OK.");
                DefaultConfigData();  
                done = TRUE;
            }
        }
        
        if (done && m_ElapsedTime_ms >= 1500)  GoToNextScreen(SCN_HOME);
    }
}


void  ScreenFunc_OscFreqControls(bool isNewScreen)
{
    static char *potLabel[] = { "OSC1", "OSC2", "OSC3", "OSC4", "OSC5", "OSC6" };
    bool   doRefresh[6];
    char   textBuf[20];
    int    pot, idx, setting;
    uint16 xpos, ypos;  // display coords
    
    if (isNewScreen)  // Render static display info
    {
        LCD_SetFont(MONO_8_NORM);
        LCD_Mode(SET_PIXELS);
        
        for (pot = 0; pot < 6; pot++)  
        {
            xpos = (pot % 3) * 43 + 6;
            ypos = (pot < 3) ? 12 : 34;
            LCD_PosXY(xpos, ypos);
            LCD_PutText(potLabel[pot]);
            xpos = (pot % 3) * 43 + 1;
            ypos = (pot < 3) ? 20 : 42;
            LCD_PosXY(xpos, ypos);
            LCD_BlockFill(40, 11);
	    doRefresh[pot] = TRUE;
        }
        DisplayMenuOption(8,  56, '*', "Exit");
        DisplayMenuOption(48, 56, 'B', "Back");
        DisplayMenuOption(88, 56, '#', "Next");
        PotFlagsClear();
    }
    else  // do periodic update...
    {
        if (ButtonHit())
        {
            if (ButtonCode() == '*') GoToNextScreen(SCN_HOME);
            if (ButtonCode() == '#') GoToNextScreen(SCN_CTRL_MIXER_LEVELS);
            if (ButtonCode() == 'B') GoToNextScreen(SCN_CTRL_OTHER_PARAMS);
        }
        
        for (pot = 0; pot < 6; pot++)
        {
            if (PotMoved(pot))
            {
                setting = (int) PotReading(pot) / 21;  // 0..12  
                if (setting > 11) setting = 11;  // 0..11
                g_Patch.OscFreqMult[pot] = (uint8) setting;
                doRefresh[pot] = TRUE;
            }
            else doRefresh[pot] = FALSE;
        }
    }
    
    for (pot = 0; pot < 6; pot++)  
    {
        if (doRefresh[pot])
        {
            idx = g_Patch.OscFreqMult[pot];  // range 0..11 (1 of 12 options)
            if (idx == 2)  sprintf(textBuf, "%5.3f", g_FreqMultConst[idx]);  // 1.333 x fo
            else if (idx == 3)  sprintf(textBuf, "%4.2f", g_FreqMultConst[idx]);  // 1.50 x fo
            else  sprintf(textBuf, "%3.1f", g_FreqMultConst[idx]);
            xpos = (pot % 3) * 43 + 3;
            ypos = (pot < 3) ? 22 : 44;
            LCD_PosXY(xpos, ypos);
            LCD_Mode(SET_PIXELS);  // Erase existing data
            LCD_BlockFill(36, 8);
            LCD_Mode(CLEAR_PIXELS);  // Write new data
            DisplayTextCenteredInField(xpos, ypos, textBuf, 6);
        }
    }
}


void  ScreenFunc_MixerLevelControls(bool isNewScreen)
{
    static char *potLabel[] = { "OSC1", "OSC2", "OSC3", "OSC4", "OSC5", "OSC6" };
    bool   doRefresh[6], doRefreshAll;
    char   textBuf[20];
    int    pot, setting;
    uint16 xpos, ypos;  // display coords 
    
    if (isNewScreen)  // Render static display info
    {
        LCD_SetFont(MONO_8_NORM);
        LCD_Mode(SET_PIXELS);
        
        for (pot = 0; pot < 6; pot++)  
        {
            xpos = (pot % 3) * 43 + 6;
            ypos = (pot < 3) ? 12 : 34;
            LCD_PosXY(xpos, ypos);
            LCD_PutText(potLabel[pot]);
            xpos = (pot % 3) * 43 + 1;
            ypos = (pot < 3) ? 20 : 42;
            LCD_PosXY(xpos, ypos);
            LCD_BlockFill(40, 11);
        }
        DisplayMenuOption(8,  56, '*', "Exit");
        DisplayMenuOption(48, 56, 'B', "Back");
        DisplayMenuOption(88, 56, '#', "Next");
        PotFlagsClear();
        doRefreshAll = TRUE;
    }
    else  // do periodic update...
    {
        if (ButtonHit())
        {
            if (ButtonCode() == '*') GoToNextScreen(SCN_HOME);
            if (ButtonCode() == '#') GoToNextScreen(SCN_CTRL_OSC_DETUNE);
            if (ButtonCode() == 'B') GoToNextScreen(SCN_CTRL_OSC_FREQ);
            if (ButtonCode() == 'C')  // Clear settings (all 0)
            {
                for (pot = 0;  pot < 6;  pot++)  { g_Patch.MixerInputStep[pot] = 0; }
                doRefreshAll = TRUE;
            }
            if (ButtonCode() == 'D')  // Default settings (all 14)
            {
                for (pot = 0;  pot < 6;  pot++)  { g_Patch.MixerInputStep[pot] = 14; }
                doRefreshAll = TRUE;
            }
        }
        
        memset(doRefresh, FALSE, 6);
        
        for (pot = 0; pot < 6; pot++)
        {
            if (PotMoved(pot))
            {
                setting = (int) PotReading(pot) / 15;  // 0..17
                if (setting > 16)  setting = 16;  // 0..16
                g_Patch.MixerInputStep[pot] = setting;
                doRefresh[pot] = TRUE;
            }
            else doRefresh[pot] = FALSE;
        }
    }
    
    // Update variable data displayed, if changed or isNewScreen
    for (pot = 0; pot < 6; pot++)  
    {
        if (doRefreshAll || doRefresh[pot])
        {
            // Display MixerInputStep param (0..15) 
            sprintf(textBuf, "%d", (int) g_Patch.MixerInputStep[pot]);
            xpos = (pot % 3) * 43 + 3;
            ypos = (pot < 3) ? 22 : 44;
            LCD_PosXY(xpos, ypos);
            LCD_Mode(SET_PIXELS);  // Erase existing data
            LCD_BlockFill(36, 8);
            LCD_Mode(CLEAR_PIXELS);  // Write new data
            DisplayTextCenteredInField(xpos, ypos, textBuf, 6);
        }
    }
}


void  ScreenFunc_OscDetuneControls(bool isNewScreen)
{
    static char *potLabel[] = { "OSC1", "OSC2", "OSC3", "OSC4", "OSC5", "OSC6" };
    bool   doRefresh[6], doRefreshAll;
    char   textBuf[20];
    int    pot, setting, ivalue;
    uint16 xpos, ypos;  // display coords 
    
    if (isNewScreen)  // Render static display info
    {
        LCD_SetFont(MONO_8_NORM);
        LCD_Mode(SET_PIXELS);
        
        for (pot = 0; pot < 6; pot++)  
        {
            xpos = (pot % 3) * 43 + 6;
            ypos = (pot < 3) ? 12 : 34;
            LCD_PosXY(xpos, ypos);
            LCD_PutText(potLabel[pot]);
            xpos = (pot % 3) * 43 + 1;
            ypos = (pot < 3) ? 20 : 42;
            LCD_PosXY(xpos, ypos);
            LCD_BlockFill(40, 11);
        }
        DisplayMenuOption(8,  56, '*', "Exit");
        DisplayMenuOption(48, 56, 'B', "Back");
        DisplayMenuOption(88, 56, '#', "Next");
        PotFlagsClear();
        doRefreshAll = TRUE;
    }
    else  // do periodic update...
    {
        if (ButtonHit())
        {
            if (ButtonCode() == '*')  GoToNextScreen(SCN_HOME);
            if (ButtonCode() == '#')  GoToNextScreen(SCN_CTRL_OSC_MODN);
            if (ButtonCode() == 'B')  GoToNextScreen(SCN_CTRL_MIXER_LEVELS);
            if (ButtonCode() == 'C')  // Clear settings (all zero)
            {
                for (pot = 0;  pot < 6;  pot++)  { g_Patch.OscDetune[pot] = 0; }
                doRefreshAll = TRUE;
            }
        }
        
        for (pot = 0; pot < 6; pot++)
        {
            if (PotMoved(pot))
            {
                setting = (int) PotReading(pot) - 128;  // bipolar setting -128..+127
                ivalue = (setting * setting * 600) / (127 * 127);  // square-law curve
                if (setting < 0)  ivalue = 0 - ivalue;  // negate
                g_Patch.OscDetune[pot] = (short) ivalue;  // range -600..+600 cents
                doRefresh[pot] = TRUE;
            }
            else doRefresh[pot] = FALSE;
        }
    }
    
    // Update numeric data displayed, if changed or isNewScreen
    for (pot = 0; pot < 6; pot++)  
    {
        if (doRefreshAll || doRefresh[pot])
        {
            sprintf(textBuf, "%+d", (int) g_Patch.OscDetune[pot]);
            xpos = (pot % 3) * 43 + 3;
            ypos = (pot < 3) ? 22 : 44;
            LCD_PosXY(xpos, ypos);
            LCD_Mode(SET_PIXELS);  // Erase existing data
            LCD_BlockFill(36, 8);
            LCD_Mode(CLEAR_PIXELS);  // Write new data
            DisplayTextCenteredInField(xpos, ypos, textBuf, 6);
        }
    }
}


void  ScreenFunc_OscModnControls(bool isNewScreen)
{
    static char *potLabel[] = { "OSC1", "OSC2", "OSC3", "OSC4", "OSC5", "OSC6" };
    static char *optName[] = 
        { "X", "CONT+", "CONT-", "ENV2", "MODN", "EXPR+", "EXPR-", "LFO", "VELO+", "VELO-" };
    bool   doRefresh[6], doRefreshAll;
    int    pot, idx, setting;
    uint16 xpos, ypos;  // display coords
    
    if (isNewScreen)  // Render static display info
    {
        LCD_SetFont(MONO_8_NORM);
        LCD_Mode(SET_PIXELS);
        
        for (pot = 0; pot < 6; pot++)  
        {
            xpos = (pot % 3) * 43 + 6;
            ypos = (pot < 3) ? 12 : 34;
            LCD_PosXY(xpos, ypos);
            LCD_PutText(potLabel[pot]);
            xpos = (pot % 3) * 43 + 1;
            ypos = (pot < 3) ? 20 : 42;
            LCD_PosXY(xpos, ypos);
            LCD_BlockFill(40, 11);
	    doRefresh[pot] = TRUE;
        }
        DisplayMenuOption(8,  56, '*', "Exit");
        DisplayMenuOption(48, 56, 'B', "Back");
        DisplayMenuOption(88, 56, '#', "Next");
        PotFlagsClear();
        doRefreshAll = TRUE;
    }
    else  // do periodic update...
    {
        if (ButtonHit())
        {
            if (ButtonCode() == '*') GoToNextScreen(SCN_HOME);
            if (ButtonCode() == '#') GoToNextScreen(SCN_CTRL_CONTOUR_PARAMS);
            if (ButtonCode() == 'B') GoToNextScreen(SCN_CTRL_OSC_DETUNE);
            if (ButtonCode() == 'C')  // Clear settings (all zero)
            {
                for (pot = 0;  pot < 6;  pot++)  
                    { g_Patch.OscAmpldModSource[pot] = 0; }
                doRefreshAll = TRUE;
            }
        }
        
        for (pot = 0; pot < 6; pot++)
        {
            if (PotMoved(pot))
            {
                setting = (int) PotReading(pot) / 26;  // 1 of 10 options 
                g_Patch.OscAmpldModSource[pot] = (uint8) setting;  // 0..9
                doRefresh[pot] = TRUE;
            }
            else doRefresh[pot] = FALSE;
        }
    }

    // Update numeric data displayed, if changed or isNewScreen
    for (pot = 0; pot < 6; pot++)  
    {
        if (doRefreshAll || doRefresh[pot])
        {
            LCD_SetFont(PROP_8_NORM);
            xpos = (pot % 3) * 43 + 3;
            ypos = (pot < 3) ? 22 : 44;
            LCD_PosXY(xpos, ypos);
            LCD_Mode(SET_PIXELS);  // Erase field
            LCD_BlockFill(36, 8);
            LCD_Mode(CLEAR_PIXELS);
            idx = g_Patch.OscAmpldModSource[pot];  // 0..9
            DisplayTextCenteredInField(xpos, ypos, optName[idx], 6);
        }
    }
}


void  ScreenFunc_ContourEnvControls(bool isNewScreen)
{
    static char *potLabel[] = { "Start %", "Delay", "Ramp", "Hold %", "EG2 Dec", "EG2 Sus" };
    static bool  doRefresh[6];
    char   textBuf[20];
    int    pot, setting;
    uint16 xpos, ypos;  // display coords 
    
    if (isNewScreen)  // Render static display info
    {
        LCD_SetFont(PROP_8_NORM);
        LCD_Mode(SET_PIXELS);
        
        for (pot = 0;  pot < 6;  pot++)
        {
            xpos = (pot % 3) * 43 + 2;
            ypos = (pot < 3) ? 12 : 34;
            LCD_PosXY(xpos, ypos);
            LCD_PutText(potLabel[pot]);
            xpos = (pot % 3) * 43 + 1;
            ypos = (pot < 3) ? 20 : 42;
            LCD_PosXY(xpos, ypos);
            LCD_BlockFill(40, 11);
	    doRefresh[pot] = TRUE;
        }
        DisplayMenuOption(8,  56, '*', "Exit");
        DisplayMenuOption(48, 56, 'B', "Back");
        DisplayMenuOption(88, 56, '#', "Next");
        PotFlagsClear();
    }
    else  // do periodic update...
    {
        if (ButtonHit())
        {
            if (ButtonCode() == '*') GoToNextScreen(SCN_HOME);
            if (ButtonCode() == '#') GoToNextScreen(SCN_CTRL_ENVELOPE_PARAMS);
            if (ButtonCode() == 'B') GoToNextScreen(SCN_CTRL_OSC_MODN);
        }
        
        if (PotMoved(0))  // Start Level (0..1000)
        {
            setting = (int) PotReading(0);  // unipolar setting 0..255
            setting = (setting * 100) / 255;  // 0..100 % (linear)
            setting = (setting / 5) * 5;  // quantize, step size = 5
            g_Patch.ContourStartLevel = (uint16) setting;
            doRefresh[0] = TRUE;
        }
        if (PotMoved(1))  // Delay Time (ms)
        {
            setting = (int) PotReading(1) / 16;  // 0..15
            setting = timeValueQuantized[setting];  // one of 16 values
            g_Patch.ContourDelayTime = (uint16) setting;  // range 0..5000
            doRefresh[1] = TRUE;
        }
        if (PotMoved(2))  // Ramp Time (ms)
        {
            setting = (int) PotReading(2) / 16;  // 0..15
            setting = timeValueQuantized[setting];  // one of 16 values
            if (setting < 5) setting = 5;
            g_Patch.ContourRampTime = (uint16) setting;  // range 5..5000
            doRefresh[2] = TRUE;
        }
        if (PotMoved(3))  // Hold Level (0..1000)
        {
            setting = (int) PotReading(3);  // unipolar setting 0..255
            setting = (setting * 100) / 255;  // 0..100 % (linear)
            setting = (setting / 5) * 5;  // quantize, step size = 5
            g_Patch.ContourHoldLevel = (uint16) setting;
            doRefresh[3] = TRUE;
        }
        if (PotMoved(4))  // ENV2 Decay/Release Time (ms)
        {
            setting = (int) PotReading(4) / 16;  // 0..15
            setting = timeValueQuantized[setting];  // one of 16 values
            if (setting < 5) setting = 5;
            g_Patch.Env2DecayTime = (uint16) setting;  // range 5..5000
            doRefresh[4] = TRUE;
        }
        if (PotMoved(5))  // ENV2 Sustain Level (%)
        {
            setting = (int) PotReading(5) / 16;  // 0..15
            setting = percentQuantized[setting];
            g_Patch.Env2SustainLevel = (uint16) setting;  // 0..100 %
            doRefresh[5] = TRUE;
        }
    }
    
    for (pot = 0;  pot < 6;  pot++)    // update parameter display
    {
        if (doRefresh[pot])
        {
            if (pot == 0) setting = (int) g_Patch.ContourStartLevel;
            if (pot == 1) setting = (int) g_Patch.ContourDelayTime;
            if (pot == 2) setting = (int) g_Patch.ContourRampTime;
            if (pot == 3) setting = (int) g_Patch.ContourHoldLevel;
            if (pot == 4) setting = (int) g_Patch.Env2DecayTime;
            if (pot == 5) setting = (int) g_Patch.Env2SustainLevel;
            
            sprintf(textBuf, "%d", setting);
            xpos = (pot % 3) * 43 + 3;
            ypos = (pot < 3) ? 22 : 44;
            LCD_PosXY(xpos, ypos);
            LCD_Mode(SET_PIXELS);  // Erase existing data
            LCD_BlockFill(36, 8);
            LCD_Mode(CLEAR_PIXELS);  // Write new data
            DisplayTextCenteredInField(xpos, ypos, textBuf, 6);
            doRefresh[pot] = FALSE;
        }
    }
}


void  ScreenFunc_EnvelopeControls(bool isNewScreen)
{
    static char *potLabel[] = { "Attack", "Hold", "Decay", "Sust %", "Release", "Vel.Mod" };
    static bool   doRefresh[6];
    char   textBuf[20];
    int    pot, setting;
    uint16 xpos, ypos;  // display coords 
    
    if (isNewScreen)  // Render static display info
    {
        LCD_SetFont(PROP_8_NORM);
        LCD_Mode(SET_PIXELS);
        
        for (pot = 0;  pot < 6;  pot++)
        {
            xpos = (pot % 3) * 43 + 2;
            ypos = (pot < 3) ? 12 : 34;
            LCD_PosXY(xpos, ypos);
            if (pot != 5)  LCD_PutText(potLabel[pot]);  // todo: Vel. Mod (pot = 5)
            xpos = (pot % 3) * 43 + 1;
            ypos = (pot < 3) ? 20 : 42;
            LCD_PosXY(xpos, ypos);
            LCD_BlockFill(40, 11);
	    doRefresh[pot] = TRUE;
        }
        DisplayMenuOption(8,  56, '*', "Exit");
        DisplayMenuOption(48, 56, 'B', "Back");
        DisplayMenuOption(88, 56, '#', "Next");
        PotFlagsClear();
    }
    else  // do periodic update...
    {
        if (ButtonHit())
        {
            if (ButtonCode() == '*') GoToNextScreen(SCN_HOME);
            if (ButtonCode() == '#') GoToNextScreen(SCN_CTRL_OTHER_PARAMS);
            if (ButtonCode() == 'B') GoToNextScreen(SCN_CTRL_CONTOUR_PARAMS);
        }
        
        memset(doRefresh, FALSE, 6);
        
        if (PotMoved(0))  // Attack Time (ms)
        {
            setting = (int) PotReading(0) / 16;  // 0..15
            setting = timeValueQuantized[setting];  // one of 16 values
            if (setting < 5) setting = 5;
            g_Patch.EnvAttackTime = (uint16) setting;  // range 5..5000
            doRefresh[0] = TRUE;
        }
        if (PotMoved(1))  // Peak Hold Time (ms)
        {
            setting = (int) PotReading(1) / 16;  // 0..15
            setting = timeValueQuantized[setting];  // one of 16 values
            g_Patch.EnvHoldTime = (uint16) setting;  // range 0..5000
            doRefresh[1] = TRUE;
            doRefresh[2] = TRUE;
        }
        if (PotMoved(2))  // Decay Time (ms)
        {
            setting = (int) PotReading(2) / 16;  // 0..15
            setting = timeValueQuantized[setting];  // one of 16 values
            if (setting < 5) setting = 5;
            // Allow Decay time to be changed only if Peak-Hold segment enabled...
            if (g_Patch.EnvHoldTime != 0)
                g_Patch.EnvDecayTime = (uint16) setting;  // range 5..5000
            doRefresh[2] = TRUE;
        }
        if (PotMoved(3))  // Sustain Level (0..100 %) 
        {
            setting = (int) PotReading(3) / 16;  // 0..15
            setting = percentQuantized[setting]; 
            g_Patch.EnvSustainLevel = (uint16) setting;
            doRefresh[3] = TRUE;
        }
        if (PotMoved(4))  // Release Time (ms)
        {
            setting = (int) PotReading(4) / 16;  // 0..15
            setting = timeValueQuantized[setting];  // one of 16 values
            if (setting < 5) setting = 5;
            g_Patch.EnvReleaseTime = (uint16) setting;  // range 5..5000
            doRefresh[4] = TRUE;
        }
        if (PotMoved(5))  // Velocity Modulation of Attack rate  *** todo ***
        {
            setting = (int) PotReading(5);  // unipolar setting 0..255
            setting = (setting * 100) / 255;  // 0..100 (linear)
            setting = (setting / 5) * 5;  // quantize, step size = 5
            g_Patch.EnvVelocityMod = (uint16) setting;  // 0..100 %
//          doRefresh[5] = TRUE;
        }
    }
 
    for (pot = 0;  pot < 6;  pot++)  // todo: Vel. Mod (pot = 5)
    {
        if (doRefresh[pot])    // Refresh displayed data
        {
            if (pot == 0) setting = (int) g_Patch.EnvAttackTime;
            if (pot == 1) setting = (int) g_Patch.EnvHoldTime;
            if (pot == 2) setting = (int) g_Patch.EnvDecayTime;
            if (pot == 3) setting = (int) g_Patch.EnvSustainLevel;
            if (pot == 4) setting = (int) g_Patch.EnvReleaseTime;
            if (pot == 5) setting = (int) g_Patch.EnvVelocityMod;

            sprintf(textBuf, "%d", setting);
            if ((pot == 1 || pot == 2) && g_Patch.EnvHoldTime == 0)  
                strcpy(textBuf, "X");
            if (pot == 5)  strcpy(textBuf, "--");  // todo: pot 5 ?
            xpos = (pot % 3) * 43 + 3;
            ypos = (pot < 3) ? 22 : 44;
            LCD_PosXY(xpos, ypos);
            LCD_Mode(SET_PIXELS);  // Erase existing data
            LCD_BlockFill(36, 8);
            LCD_Mode(CLEAR_PIXELS);  // Write new data
            DisplayTextCenteredInField(xpos, ypos, textBuf, 6);  
            doRefresh[pot] = FALSE;
        }
    }
}


void  ScreenFunc_OtherControls(bool isNewScreen)
{
    static char *potLabel[] = 
            { "LFO Hz", "Ramp ms", "Osc FM", "LFO AM", "Mix gain", "Amp ctrl" };
    static char *optName[] = { "100%", "50%", "ENV1", "EXPRN" };
    static uint8  optMixerGain_x10[] = { 5, 7, 10, 15, 20, 25, 30, 50, 70, 100 };
    static uint8  LFOfreqStep[] = { 5, 7, 10, 15, 20, 25, 30, 40, 50, 60, 70, 80,
                                    100, 150, 200, 250 };  // 16 steps
    static bool  doRefresh[6];
    char   textBuf[20];
    int    pot, setting;
    uint16 xpos, ypos;  // display coords 
    
    if (isNewScreen)  // Render static display info
    {
        LCD_SetFont(PROP_8_NORM);
        LCD_Mode(SET_PIXELS);
        
        for (pot = 0; pot < 6; pot++)  
        {
            xpos = (pot % 3) * 43 + 2;
            ypos = (pot < 3) ? 12 : 34;
            LCD_PosXY(xpos, ypos);
            LCD_PutText(potLabel[pot]);
            xpos = (pot % 3) * 43 + 1;
            ypos = (pot < 3) ? 20 : 42;
            LCD_PosXY(xpos, ypos);
            LCD_BlockFill(40, 11);
            doRefresh[pot] = TRUE;
        }
        DisplayMenuOption(8,  56, '*', "Exit");
        DisplayMenuOption(48, 56, 'B', "Back");
        DisplayMenuOption(88, 56, '#', "Next");
        PotFlagsClear();
    }
    else  // do periodic update...
    {
        if (ButtonHit())
        {
            if (ButtonCode() == '*') GoToNextScreen(SCN_HOME);
            if (ButtonCode() == '#') GoToNextScreen(SCN_CTRL_OSC_FREQ);  // wrap
            if (ButtonCode() == 'B') GoToNextScreen(SCN_CTRL_ENVELOPE_PARAMS);
        }
        
        if (PotMoved(0))  // LFO Freq.
        {
            setting = (int) PotReading(0) / 16;  // 0..15
            g_Patch.LFO_Freq_x10 = LFOfreqStep[setting];  // 5..250
            doRefresh[0] = TRUE;
        }
        if (PotMoved(1))  // LFO Delay-Ramp Time (ms)
        {
            setting = (int) PotReading(1) / 16;  // 0..15
            setting = timeValueQuantized[setting];  // one of 16 values
            if (setting < 5) setting = 5;
            g_Patch.LFO_RampTime = (uint16) setting;  // range 5..5000
            doRefresh[1] = TRUE;
        }
        if (PotMoved(2))  // LFO FM Depth (cents = % semitone)
        {
            setting = (int) PotReading(2);
            setting = (setting * 100) / 255;  // 0..100
            setting = (setting / 5) * 5;  // quantize, step size = 5
            g_Patch.LFO_FM_Depth = (uint16) setting;
            doRefresh[2] = TRUE;
        }
        if (PotMoved(3))  // LFO AM Depth (%)
        {
            setting = (int) PotReading(3);
            setting = (setting * 100) / 255;  // 0..100
            setting = (setting / 5) * 5;  // quantize, step size = 5
            g_Patch.LFO_AM_Depth = (uint16) setting;  // range 0..100
            doRefresh[3] = TRUE;
        }
        if (PotMoved(4))  // Mixer output GAIN
        {
            setting = (int) PotReading(4) / 25;  // 0..10
            if (setting > 9)  setting = 9;  // 0..9 (index)
            g_Patch.MixerOutGain_x10 = (uint16) optMixerGain_x10[setting];
            doRefresh[4] = TRUE;
        }
        if (PotMoved(5))  // Output Ampld control source
        {
            setting = (int) PotReading(5) / 32;  // 0..7
            if (g_Config.AudioAmpldCtrlMode == AUDIO_CTRL_BY_PATCH)
                g_Patch.AmpldControlSource = (uint8) setting & 3;  // 0..3
            doRefresh[5] = TRUE;
        }
    }
    
    // Update variable data displayed, if changed or isNewScreen
    for (pot = 0; pot < 6; pot++)  
    {
        if (doRefresh[pot])
        {
            if (pot == 0) 
            {
                setting = (int) g_Patch.LFO_Freq_x10;
                sprintf(textBuf, "%4.1f", (float)setting/10);
            }
            else if (pot == 1) 
            {
                setting = (int) g_Patch.LFO_RampTime;
                sprintf(textBuf, "%d", setting);
            }
            else if (pot == 2) 
            {
                setting = (int) g_Patch.LFO_FM_Depth;
                sprintf(textBuf, "%dc", setting);
            }
            else if (pot == 3) 
            {
                setting = (int) g_Patch.LFO_AM_Depth;
                sprintf(textBuf, "%d %%", setting);
            }
            else if (pot == 4) 
            {
                setting = (int) g_Patch.MixerOutGain_x10;
                sprintf(textBuf, "%4.1f", (float)setting/10);
            }
            else // (pot == 5)
            {
                setting = (int) g_Patch.AmpldControlSource & 3;
                strcpy(textBuf, optName[setting]);
                if (g_Config.AudioAmpldCtrlMode != AUDIO_CTRL_BY_PATCH)
                    strcpy(textBuf, "X");  // Setting overridden
            }
            xpos = (pot % 3) * 43 + 3;
            ypos = (pot < 3) ? 22 : 44;
            LCD_PosXY(xpos, ypos);
            LCD_Mode(SET_PIXELS);  // Erase existing data
            LCD_BlockFill(36, 8);
            LCD_Mode(CLEAR_PIXELS);  // Write new data
            DisplayTextCenteredInField(xpos, ypos, textBuf, 6);
            doRefresh[pot] = FALSE;
        }
    }
}


// This function is for debug use only
//
void  ScreenFunc_ControlPotTest(bool isNewScreen)
{
    static char  *potLabel[] = { "Pot 1", "Pot 2", "Pot 3", "Pot 4", "Pot 5", "Pot 6" };
    static uint16  dummyParam[6];
    bool   doRefresh[6];
    char   textBuf[40];
    int    pot, setting;
    uint16 xpos, ypos;  // display coords 
    
    if (isNewScreen)  // new screen...
    {
        LCD_Mode(SET_PIXELS);
        LCD_SetFont(PROP_8_NORM); 
        
        for (pot = 0; pot < 6; pot++)  
        {
            xpos = (pot % 3) * 43 + 2;
            ypos = (pot < 3) ? 12 : 34;
            LCD_PosXY(xpos, ypos);
            LCD_PutText(potLabel[pot]);
            xpos = (pot % 3) * 43 + 1;
            ypos = (pot < 3) ? 20 : 42;
            LCD_PosXY(xpos, ypos);
            LCD_BlockFill(40, 11);
            doRefresh[pot] = TRUE;
        }
        DisplayMenuOption(8,  56, '*', "Exit");
        DisplayMenuOption(48, 56, 'B', "Back");
        DisplayMenuOption(88, 56, '#', "Next");
        PotFlagsClear();
    }
    else  // check for button hit or any pot position changed
    {
        if (ButtonHit())
        {
            if (ButtonCode() == '*') GoToNextScreen(SCN_HOME);
            if (ButtonCode() == '#') GoToNextScreen(SCN_CTRL_OSC_FREQ);
            if (ButtonCode() == 'B') GoToNextScreen(SCN_CTRL_OTHER_PARAMS);
        }
		
		memset(doRefresh, FALSE, 6);
        
        for (pot = 0;  pot < 3;  pot++)  
        {
            if (PotMoved(pot))  
            {
                setting = (int) PotReading(pot);  // unipolar setting 0..255
                setting = (setting * setting * 1000) / (255 * 255);  // square-law curve
                dummyParam[pot] = timeValueQuantized[setting];  // range 0..1000
                doRefresh[pot] = TRUE;
            }
        }
        
        for (pot = 3;  pot < 6;  pot++)  
        {
            if (PotMoved(pot))  
            {
                setting = (int) PotReading(pot);  // unipolar setting 0..255
                setting = (setting * setting * 5000) / (255 * 255);  // square-law curve
                if (setting < 10)  setting = 10;
                dummyParam[pot] = timeValueQuantized[setting];  // range 10..5000
                doRefresh[pot] = TRUE;
            }
        }
    }
    
    // Update variable data displayed, if changed or isNewScreen
    for (pot = 0;  pot < 6;  pot++)  
    {
        if (doRefresh[pot])
        {
            sprintf(textBuf, "%4d", (int) dummyParam[pot]);
            xpos = (pot % 3) * 43 + 3;
            ypos = (pot < 3) ? 22 : 44;
            LCD_PosXY(xpos, ypos);
            LCD_Mode(SET_PIXELS);  // Erase existing data
            LCD_BlockFill(36, 8);
            LCD_Mode(CLEAR_PIXELS);  // Write new data
            LCD_PosXY(xpos+9, ypos);
            LCD_PutText(textBuf);
        }
    }
}


void  ScreenFunc_SetAudioControlMode(bool isNewScreen)
{
    static  uint8   audioCtrlMode;
    bool    doRefresh = 0;

    if (isNewScreen)   // Render static display info
    {
        LCD_Mode(SET_PIXELS);
        LCD_SetFont(PROP_8_NORM);
        LCD_PosXY(8, 12);
        LCD_PutText("Current setting:");

        LCD_PosXY(0, 42);
        LCD_DrawLineHoriz(128);
        DisplayMenuOption(80, 45, 'C', "Change");
        DisplayMenuOption( 8, 56, '*', "Home");
        DisplayMenuOption(80, 56, '#', "Next");

        audioCtrlMode = g_Config.AudioAmpldCtrlMode;
        doRefresh = 1;
    }
    else  // monitor buttons
    {
        if (ButtonHit())
        {
            if (ButtonCode() == '*')  GoToNextScreen(SCN_HOME);
            if (ButtonCode() == '#')  GoToNextScreen(SCN_SET_REVERB_PARAMS);
            if (ButtonCode() == 'C')  // Change mode
            {
                audioCtrlMode = (audioCtrlMode + 1) & 3;  // 0..3
                g_Config.AudioAmpldCtrlMode = audioCtrlMode;
                StoreConfigData();
                SynthPrepare();
                doRefresh = 1;
            }
        }
    }

    if (doRefresh)
    {
        LCD_SetFont(PROP_8_NORM);
        LCD_PosXY(16, 22);
        LCD_BlockClear(96, 8);  // erase existing text
        if (audioCtrlMode == AUDIO_CTRL_CONST)  LCD_PutText("Fixed (full-scale)");
        else if (audioCtrlMode == AUDIO_CTRL_ENV1_VELO)  LCD_PutText("ENV1 * Velocity");
        else if (audioCtrlMode == AUDIO_CTRL_EXPRESS)  LCD_PutText("Expression (CC)");
        else  LCD_PutText("Using patch param");
        
        LCD_PosXY(16, 32);
        LCD_BlockClear(96, 8);  // erase existing text
        if (audioCtrlMode != AUDIO_CTRL_BY_PATCH)  LCD_PutText("(Override patch)");
    }
}


void  ScreenFunc_SetReverbControls(bool isNewScreen)
{
    static uint8  reverbMix, reverbAtten;
    char   textBuf[20];
    bool   doRefresh = 0;
    
    if (isNewScreen)  // Render static display info
    {
        reverbMix = g_Config.ReverbMix_pc;
        reverbAtten = g_Config.ReverbAtten_pc;
        LCD_PosXY(0, 42);
        LCD_DrawLineHoriz(128);
        DisplayMenuOption( 4, 45, 'A', "Mix+");
        DisplayMenuOption(44, 45, 'B', "Mix-");
        DisplayMenuOption(80, 45, 'C', "Atten+");
        DisplayMenuOption(44, 56, 'D', "Dflt");
        DisplayMenuOption( 4, 56, '*', "Home");
        DisplayMenuOption(80, 56, '#', "Next");     
        doRefresh = 1;
    }
    else   // do periodic update
    {
        if (ButtonHit())
        {
            if (ButtonCode() == '*')  GoToNextScreen(SCN_HOME);
            if (ButtonCode() == '#')  GoToNextScreen(SCN_SET_VIBRATO_MODE);
            if (ButtonCode() == 'A' && reverbMix <= 90)  // max. 95
            {
                reverbMix += 5;
                g_Config.ReverbMix_pc = reverbMix;
                StoreConfigData();
                SynthPrepare();
                doRefresh = 1;
            }
            if (ButtonCode() == 'B' && reverbMix >= 5)  // min. 0 (Off))
            {
                reverbMix -= 5;
                g_Config.ReverbMix_pc = reverbMix;
                StoreConfigData();
                SynthPrepare();
                doRefresh = 1;
            }
            if (ButtonCode() == 'C')  
            {
                reverbAtten += 5;
                if (reverbAtten > 90)  reverbAtten = 50;  // max. 90, min. 50
                g_Config.ReverbAtten_pc = reverbAtten;
                StoreConfigData();
                SynthPrepare();
                doRefresh = 1;
            }
            if (ButtonCode() == 'D')  // Default settings
            {
                reverbMix = 15;
                g_Config.ReverbMix_pc = reverbMix;
                reverbAtten = 70;
                g_Config.ReverbAtten_pc = reverbAtten;
                StoreConfigData();
                SynthPrepare();
                doRefresh = 1;
            }
        }
    }
    
    if (doRefresh)
    {
        sprintf(textBuf, "%2d", (int)reverbMix);
        LCD_PosXY(0, 16);
        LCD_BlockClear(128, 10);  // erase existing text line
        LCD_SetFont(PROP_8_NORM);
        LCD_PutText("Reverb Mix Level: ");
        LCD_SetFont(MONO_8_NORM);
        LCD_PutText(textBuf);
        LCD_PutText("%");

        sprintf(textBuf, "%2d", (int)reverbAtten);
        LCD_PosXY(0, 26);
        LCD_BlockClear(128, 10);  // erase existing text line
        LCD_SetFont(PROP_8_NORM);
        LCD_PutText("Reverb Attenuation: ");
        LCD_SetFont(MONO_8_NORM);
        LCD_PutText(textBuf);
        LCD_PutText("%");
        doRefresh = 0;   // inhibit refresh until next key hit
    }
}


void  ScreenFunc_SetVibratoMode(bool isNewScreen)
{
    static  uint8   vib_mode;
    bool    doRefresh = 0;

    if (isNewScreen)   // Render static display info
    {
        LCD_Mode(SET_PIXELS);
        LCD_SetFont(PROP_8_NORM);
        LCD_PosXY(8, 16);
        LCD_PutText("Control Mode:");

        LCD_PosXY(0, 42);
        LCD_DrawLineHoriz(128);
        DisplayMenuOption(80, 45, 'C', "Change");
        DisplayMenuOption( 8, 56, '*', "Home");
        DisplayMenuOption(80, 56, '#', "Next");

        vib_mode = g_Config.VibratoCtrlMode;
        doRefresh = 1;
    }
    else  // monitor buttons
    {
        if (ButtonHit())
        {
            if (ButtonCode() == '*')  GoToNextScreen(SCN_HOME);
            if (ButtonCode() == '#')  GoToNextScreen(SCN_SET_PITCH_BEND_MODE);
            if (ButtonCode() == 'C')  // Change mode
            {
                if (vib_mode == VIBRATO_DISABLED) vib_mode = VIBRATO_BY_MODN_CC;
                else if (vib_mode == VIBRATO_BY_MODN_CC) vib_mode = VIBRATO_AUTOMATIC;
                else  vib_mode = VIBRATO_DISABLED;

                g_Config.VibratoCtrlMode = vib_mode;
                StoreConfigData();
                SynthPrepare();
                doRefresh = 1;
            }
        }
    }

    if (doRefresh)
    {
        LCD_PosXY(0, 26);
        LCD_BlockClear(128, 10);  // erase existing text line
        LCD_SetFont(PROP_8_NORM);
        LCD_PosXY(16, 26);

        if (vib_mode == VIBRATO_BY_EFFECT_SW)  LCD_PutText("Effect Switch");  // todo (?)
        else if (vib_mode == VIBRATO_BY_MODN_CC)  LCD_PutText("MIDI CC1 message");
        else if (vib_mode == VIBRATO_AUTOMATIC)  LCD_PutText("Automatic");
        else  LCD_PutText("Disabled");

        doRefresh = 0;   // inhibit refresh until next key hit
    }
}


void  ScreenFunc_SetPitchBendMode(bool isNewScreen)
{
    static uint8  bendRangeOption[] = { 1, 2, 3, 4, 6, 8, 10, 12 };
    static short  bendix;  // index into array bendRangeOption[]
    bool   doRefresh = 0;

    if (isNewScreen)  // Render static display info
    {
        LCD_Mode(SET_PIXELS);
        LCD_SetFont(PROP_8_NORM);
        LCD_PosXY(8, 16);
        LCD_PutText("Mode:");
        LCD_PosXY(8, 26);
        LCD_PutText("Range: ");

        LCD_PosXY(0, 42);
        LCD_DrawLineHoriz(128);
        DisplayMenuOption( 8, 45, 'B', "Range");
        DisplayMenuOption(64, 45, 'C', "Control");
        DisplayMenuOption( 8, 56, '*', "Home");
        DisplayMenuOption(80, 56, '#', "Next");

        bendix = 0;
        doRefresh = 1;
    }
    else  // monitor buttons
    {
        if (ButtonHit())
        {
            if (ButtonCode() == '*')  GoToNextScreen(SCN_HOME);
            if (ButtonCode() == '#')  GoToNextScreen(SCN_SET_MIDI_PARAMS);
            if (ButtonCode() == 'B')  // Set Bend range
            {
                bendix = (bendix + 1) & 7;  // 0..7
                g_Config.PitchBendRange = bendRangeOption[bendix];
                StoreConfigData();
                SynthPrepare();
                doRefresh = 1;
            }
            if (ButtonCode() == 'C')  // Set Control mode
            {
                g_Config.PitchBendEnable = !g_Config.PitchBendEnable;  // toggle
                StoreConfigData();
                SynthPrepare();
                doRefresh = 1;
            }
        }
    }

    if (doRefresh)
    {
        LCD_SetFont(PROP_8_NORM);
        LCD_PosXY(56, 16);
        LCD_BlockClear(72, 8);  // erase existing text
        if (g_Config.PitchBendEnable)  LCD_PutText("Enabled");
        else  LCD_PutText("Disabled");
        
        LCD_SetFont(MONO_8_NORM);
        LCD_PosXY(56, 26);
        LCD_BlockClear(72, 8);  // erase existing data
        LCD_PutDecimal(g_Config.PitchBendRange, 1);
        LCD_SetFont(PROP_8_NORM);
        LCD_PutText(" semitone");
        if (g_Config.PitchBendRange > 1) LCD_PutChar('s');  // plural!
    }
}


void  ScreenFunc_MIDI_Settings(bool isNewScreen)
{
    static uint8  midiChannel, midiExpressionCCnum;
    char   textBuf[20];
    bool   doRefresh = 0;
    
    if (isNewScreen)  // Render static display info
    {
        midiChannel = g_Config.MidiChannel;
        midiExpressionCCnum = g_Config.MidiExpressionCCnum;
        LCD_PosXY(0, 42);
        LCD_DrawLineHoriz(128);
        DisplayMenuOption( 4, 45, 'A', "Ch+");
        DisplayMenuOption(44, 45, 'B', "Ch-");
        DisplayMenuOption(84, 45, 'C', "Exprn");
        DisplayMenuOption(44, 56, 'D', "Mode");
        DisplayMenuOption( 4, 56, '*', "Home");
        DisplayMenuOption(84, 56, '#', "Next");   
        doRefresh = 1;
    }
    else   // do periodic update
    {
        if (ButtonHit())
        {
            if (ButtonCode() == '*')  GoToNextScreen(SCN_HOME);
            if (ButtonCode() == '#')  GoToNextScreen(SCN_SET_AUDIO_CTRL_MODE);
            if (ButtonCode() == 'A' && midiChannel < 16)  // max. 16
            {
                midiChannel++;
                g_Config.MidiChannel = midiChannel;
                StoreConfigData();
                doRefresh = 1;
            }
            if (ButtonCode() == 'B' && midiChannel > 1)  // min. 1
            {
                midiChannel--;
                g_Config.MidiChannel = midiChannel;
                StoreConfigData();
                doRefresh = 1;
            }
            if (ButtonCode() == 'C')  // scroll thru options
            {
                if (midiExpressionCCnum == 0)  midiExpressionCCnum = 2;
                else if (midiExpressionCCnum == 2)  midiExpressionCCnum = 7;
                else if (midiExpressionCCnum == 7)  midiExpressionCCnum = 11;
                else if (midiExpressionCCnum == 11)  midiExpressionCCnum = 0;
                else  midiExpressionCCnum = 2;  // default
                g_Config.MidiExpressionCCnum = midiExpressionCCnum;
                StoreConfigData();
                doRefresh = 1;
            }
            if (ButtonCode() == 'D')  // toggle MIDI Omni mode
            {
                if (g_Config.MidiMode == OMNI_OFF_MONO) 
                    g_Config.MidiMode = OMNI_ON_MONO;
                else  g_Config.MidiMode = OMNI_OFF_MONO;
                StoreConfigData();
                doRefresh = 1;
            }
        }
    }

    if (doRefresh)
    {
        LCD_SetFont(MONO_8_NORM);
        LCD_PosXY(16, 12);
        LCD_BlockClear(104, 8);  // erase existing
        LCD_PutText("Mode: ");
        if (g_Config.MidiMode == OMNI_ON_MONO) LCD_PutText("Omni-ON");
        else  LCD_PutText("Omni-OFF");
        
        sprintf(textBuf, "%d", (int)g_Config.MidiChannel);
        LCD_PosXY(16, 22);
        LCD_BlockClear(104, 8);  // erase existing
        LCD_PutText("Channel: ");
        LCD_PutText(textBuf);

        sprintf(textBuf, "CC%d", (int)g_Config.MidiExpressionCCnum);
        LCD_PosXY(16, 32);
        LCD_BlockClear(104, 8);  // erase existing
        LCD_PutText("Expression: ");
        if (g_Config.MidiExpressionCCnum == 0) LCD_PutText("OFF");
        else  LCD_PutText(textBuf);
    }    
}


// Function for debug purposes only
//
void  SerialDiagnosticOutput()
{
    char     textBuf[100];
    fixed_t  pitchBendNorm = 0;  // fraction of an octave (0..+/-1.0)
    fixed_t  freqDevn;  // normalized fixed_pt (12:20)
    int32    oscStep;   // 16:16 bit fixed-pt value
    int32    oscStepMid = (int32) 10 << 16;
    
    putNewLine();
    putstr(textBuf);
    putstr("  pitchBend |  freqDevn  |  oscStep \n");
    putstr("    12:20   |   12:20    |   16:16  \n");
    putstr("  ----------|------------|----------\n");
    
    while (pitchBendNorm <= FloatToFixed(0.25))
    {
        freqDevn = Base2Exp(pitchBendNorm);
        oscStep = MultiplyFixed(oscStepMid, freqDevn);  // expression under test
        
        sprintf(textBuf, "  %8.3f  |  %8.5f  |  %8.5f  \n", 
            FixedToFloat(pitchBendNorm), FixedToFloat(freqDevn), (float)oscStep / 65536);
        
        putstr(textBuf);
        pitchBendNorm += FloatToFixed(0.01);
    }
}

