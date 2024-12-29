/**
 *   File:    sigma6_synth_panel.h 
 *
 *   Definitions for the Control Panel User Interface of the Sigma-6 Sound Synth.
 * 
 */
#ifndef S6_SYNTH_PANEL_H
#define S6_SYNTH_PANEL_H

#ifdef __32MX440F256H__
#include "pic32mx440_low_level.h"
#else
#include "pic32_low_level.h"
#endif

#ifdef __32MX440F256H__  // assume MAM board (Pinguino-micro)
#define POT_MODULE_CONNECTED  (READ_HW_CFG_JUMPER_P0 == 0)
#else  // assume PIC32MX340 platform
#define POT_MODULE_CONNECTED  (READ_JUMPER_HW_CFG1 == 0)
#endif

#define SCREEN_UPDATE_INTERVAL     (50)     // Time (ms) between active screen updates
#define GUI_INACTIVE_TIMEOUT    (30*1000)   // Time (ms) before revert to quiescent screen
#define SELF_TEST_WAIT_TIME_MS     2000     // Duration of self-test message display (ms)

// An object of this type is needed for each display screen defined.
// An array of structures of this type is held in flash memory (const data area).
// For screens which have no Title Bar, initialize TitleBarText = NULL;
//
typedef struct GUI_screen_descriptor
{
    uint16  screen_ID;             // Screen ID number (0..NUMBER_OF_SCREEN_IDS)
    void    (*ScreenFunc)(bool);   // Function to prepare/update the screen
    char     *TitleBarText;        // Pointer to title string;  NULL if no title bar

} ScreenDescriptor_t;


// ------- Screens defined in the REMI Local User Interface  -----------------
//
enum  Set_of_Screen_ID_numbers   // Any arbitrary order
{
    SCN_STARTUP = 0,
    SCN_SELFTEST_REPORT,
    SCN_HOME,
    SCN_SELECT_PRESET,
    SCN_SAVE_USER_PATCH,
    SCN_DIAGNOSTIC_INFO,
    SCN_MISC_UTILITY,
    SCN_DEFAULT_CONFIG,
    //
    SCN_CTRL_OSC_FREQ,
    SCN_CTRL_MIXER_LEVELS,
    SCN_CTRL_OSC_DETUNE,
    SCN_CTRL_OSC_MODN,
    SCN_CTRL_CONTOUR_PARAMS,
    SCN_CTRL_ENVELOPE_PARAMS,
    SCN_CTRL_OTHER_PARAMS,
    //
    SCN_SET_AUDIO_CTRL_MODE,
    SCN_SET_REVERB_PARAMS,
    SCN_SET_VIBRATO_MODE,
    SCN_SET_PITCH_BEND_MODE,
    SCN_SET_MIDI_PARAMS
};


// --------  Control Panel functions  --------
//
int     GetNumberOfScreensDefined();
uint16  GetCurrentScreenID();
uint16  GetPreviousScreenID();
void    GoToNextScreen(uint16 nextScreenID);

void    ControlPanelService();
void    ButtonInputService();
void    ControlPotService();

bool    ScreenSwitchOccurred(void);
int     ScreenDescIndexFind(uint16 searchID);
void    DisplayMenuOption(uint16 x, uint16 y, char symbol, char *text);
void    DisplayTextCenteredInField(uint16 x, uint16 y, char *str, uint8 nplaces);

#endif // S6_SYNTH_PANEL_H
