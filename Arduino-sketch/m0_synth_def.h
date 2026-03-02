/**
 *   File:    m0_synth_def.h 
 *
 *   Data definitions & declarations for 'Sigma-6' sound synthesizers.
 *   This header file is common to all SAMD21-based 'Sigma-6' voice modules,
 *   but must be customized to suit a particular synth variant as follows:
 *   ``````````````````````````````````````````````````````````````````````````
 *   Mono-synth voice module using Adafruit M0 Express board:  
 *           In Arduino IDE, select board: 'Adafruit ItsyBitsy M0 Express'
 *           Set MCU_PINS_D2_D4_REVERSED to FALSE
 *           Set BUILD_FOR_POLY_VOICE to FALSE
 *           Set EEPROM_IS_INSTALLED to TRUE or FALSE as applicable
 *
 *   Mono-synth voice module using RobotDyn M0 Mini board:
 *           In Arduino IDE, select board: 'Arduino Zero (Native USB)'
 *           Set MCU_PINS_D2_D4_REVERSED to TRUE
 *           Set BUILD_FOR_POLY_VOICE to FALSE
 *           Set EEPROM_IS_INSTALLED to TRUE or FALSE as applicable
 *
 *   POLY-synth voice module using RobotDyn M0 Mini board:  
 *           In Arduino IDE, select board: 'Arduino Zero (Native USB)'
 *           Set MCU_PINS_D2_D4_REVERSED to FALSE
 *           Set BUILD_FOR_POLY_VOICE to TRUE
 *           Set EEPROM_IS_INSTALLED to FALSE
 */
#ifndef M0_SYNTH_DEF_H
#define M0_SYNTH_DEF_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

// Firmware build options...............
#define MCU_PINS_D2_D4_REVERSED    TRUE   // See notes above
#define BUILD_FOR_POLY_VOICE       FALSE  // FALSE => Build for Sigma-6 Mono-synth
#define EEPROM_IS_INSTALLED        TRUE   // FALSE => EEPROM not installed

#define BUILD_FOR_EWI_CONTROLLER   FALSE  // FALSE => Build for keyboard or sequencer
#define APPLY_VELOCITY_EXPL_CURVE  FALSE  // TRUE => Apply "exponential" ampld curve
#define APPLY_EXPRESSN_EXPL_CURVE  FALSE  // TRUE => Apply "exponential" ampld curve
#define USE_SPI_DAC_FOR_AUDIO      TRUE   // FALSE => Use MCU on-chip DAC (pin A0)
#define INCLUDE_DEBUG_CODE         FALSE  // FALSE => Normal build for release

//#define HOME_SCREEN_SYNTH_DESCR  "Voice Module"  // 12 chars max.
//#define HOME_SCREEN_SYNTH_DESCR  "Mono Voice"  // 12 chars max.
#define HOME_SCREEN_SYNTH_DESCR  "Mono-synth"  // 12 chars max.

// Do not modify code below this line...
//===========================================================================================

typedef signed long  fixed_t;     // 32-bit fixed point (20-bit fraction)
typedef void (* pfnvoid)(void);   // pointer to void function

#ifndef BOOL
typedef unsigned char  BOOL;
#endif
#ifndef FALSE
#define FALSE   (0)
#define TRUE    (!FALSE)
#endif

// MCU I/O pin assignments......
#define CHAN_SWITCH_S1        12    // MIDI channel-select switch S1 (bit 0)
#define CHAN_SWITCH_S2        11    // MIDI channel-select switch S2 (bit 1)
#define CHAN_SWITCH_S3        10    // MIDI channel-select switch S3 (bit 2)
#define CHAN_SWITCH_S4         9    // MIDI channel-select switch S4 (bit 3)
#define TESTPOINT1            13    // Scope test-point pin (ISR)
#define TESTPOINT2             5    // Scope test-point pin (GATE)
#define BUTTON_A_PIN           3    // Button [A] input (active low)

#if (!BUILD_FOR_POLY_VOICE)  // assume Sigma-6 Mono VM with CV inputs...
#define CV_MODE_JUMPER         7    // CV Mode jumper (JP1) input pin
#define GATE_INPUT            19    // GATE input (digital, active High)
#endif

#if (MCU_PINS_D2_D4_REVERSED)
#define BUTTON_B_PIN           2
#define SPI_DAC_CS             4    // using Robotdyn M0 Mini pinout
#else
#define BUTTON_B_PIN           4
#define SPI_DAC_CS             2    // using Adafruit M0 pinout
#endif

#define WAVE_TABLE_SIZE          2048    // nunber of samples
#define SAMPLE_RATE_HZ          32000    // typically 32,000 or 40,000 Hz
#define MAX_OSC_FREQ_HZ         12000    // must be < 0.4 x SAMPLE_RATE_HZ

#define REVERB_DELAY_MAX_SIZE    2000    // samples 
#define REVERB_LOOP_TIME_SEC     0.04    // seconds (max. 0.05 sec.)
#define REVERB_DECAY_TIME_SEC     1.5    // seconds
#define REVERB_ATTENUATION_PC      70    // percent (range: 35..95 %)

#define FIXED_MIN_LEVEL           (1)    // Minimum non-zero signal level (0.00001)
#define FIXED_MAX_LEVEL  (IntToFixedPt(1) - 1)   // Full-scale normalized signal level (0.99999)
#define FIXED_PT_HALF    (IntToFixedPt(1) / 2)   // constant = 0.5 in fixed_t format
#define MAX_CLIPPING_LEVEL ((IntToFixedPt(1) * 97) / 100)   // constant = 0.97

// Possible values for config parameter: g_Config.AudioAmpldCtrlMode
// If non-zero, this setting overrides the patch parameter: g_Patch.AmpldControlSource
#define AUDIO_CTRL_BY_PATCH         0    // Audio output control by active patch param
#define AUDIO_CTRL_CONST            1    // Audio output control by fixed level (max)
#define AUDIO_CTRL_ENV1_VELO        2    // Audio output control by ENV1 * Velocity
#define AUDIO_CTRL_EXPRESS          3    // Audio output control by Expression (CC2,7,11)

// Possible values for config parameter: g_Config.VibratoCtrlMode
#define VIBRATO_DISABLED            0    // Vibrato disabled
#define VIBRATO_BY_MODN_CC          1    // Vibrato controlled by MIDI message (CC1)
#define VIBRATO_BY_CV_AUXIN         2    // Vibrato controlled by CV4 (AUX.IN)
#define VIBRATO_AUTOMATIC           3    // Vibrato automatic, delay + ramp, all osc.

// Possible values for config parameter: g_Config.PitchBendMode
#define PITCH_BEND_DISABLED         0    // Pitch Bend disabled
#define PITCH_BEND_BY_MIDI_MSG      1    // Pitch Bend controlled by MIDI message
#define PITCH_BEND_BY_CV1_INPUT     2    // Pitch Bend controlled by CV1 (PITCH)

// Possible values for patch parameters: g_Patch.OscAmpldModSource[6]]
#define OSC_MODN_SOURCE_NONE        0    // Osc Ampld Mod'n disabled (fixed 100%)
#define OSC_MODN_SOURCE_CONT_POS    1    // Osc Ampld Mod'n by Contour EG, normal(+)
#define OSC_MODN_SOURCE_CONT_NEG    2    // Osc Ampld Mod'n by Contour EG, invert(-)
#define OSC_MODN_SOURCE_ENV2        3    // Osc Ampld Mod'n by ENV2 - Transient Gen.
#define OSC_MODN_SOURCE_MODN        4    // Osc Ampld Mod'n by MIDI Modulation (CC1)
#define OSC_MODN_SOURCE_EXPR_POS    5    // Osc Ampld Mod'n by MIDI Exprn, normal(+)
#define OSC_MODN_SOURCE_EXPR_NEG    6    // Osc Ampld Mod'n by MIDI Exprn, invert(-))
#define OSC_MODN_SOURCE_LFO         7    // Osc Ampld Mod'n by LFO (using AM depth)
#define OSC_MODN_SOURCE_VELO_POS    8    // Osc Ampld Mod'n by Velocity, normal(+)
#define OSC_MODN_SOURCE_VELO_NEG    9    // Osc Ampld Mod'n by Velocity, invert(-)

// Possible values for patch parameter: g_Patch.AmpControlMode
#define AMPLD_CTRL_CONST_MAX        0    // Output ampld is constant (max. level)
#define AMPLD_CTRL_CONST_LOW        1    // Output ampld is constant (lower level)
#define AMPLD_CTRL_ENV1_VELO        2    // Output ampld control by ENV1 * Velocity
#define AMPLD_CTRL_EXPRESS          3    // Output ampld control by Expression (CC2,7,11)

#define OMNI_ON_POLY      1   // MIDI device responds in Poly mode on all channels
#define OMNI_ON_MONO      2   // MIDI device responds in Mono mode on all channels
#define OMNI_OFF_POLY     3   // MIDI device responds in Poly mode on base channel only
#define OMNI_OFF_MONO     4   // MIDI device responds in Mono mode on base channel only

#define NOTE_OFF_CMD         0x80    // 3-byte message
#define NOTE_ON_CMD          0x90    // 3-byte message
#define POLY_KEY_PRESS_CMD   0xA0    // 3-byte message
#define CONTROL_CHANGE_CMD   0xB0    // 3-byte message
#define PROGRAM_CHANGE_CMD   0xC0    // 2-byte message
#define CHAN_PRESSURE_CMD    0xD0    // 2-byte message
#define PITCH_BEND_CMD       0xE0    // 3-byte message
#define SYS_EXCLUSIVE_MSG    0xF0    // variable length message
#define SYSTEM_MSG_EOX       0xF7    // system-ex msg terminator
#define SYS_EXCL_REMI_ID     0x73    // arbitrary pick... hope it's free!
#define CC_MODULATION        1       // Control change High byte
#define CC_BREATH_PRESSURE   2       //    ..     ..     ..
#define CC_CHANNEL_VOLUME    7       //    ..     ..     ..
#define CC_EXPRESSION        11      //    ..     ..     ..
#define MIDI_MSG_MAX_LENGTH  16      // not in MIDI specification!

enum  Envelope_Gen_Phases  // aka "segments"
{
  ENV_IDLE = 0,      // Idle - Envelope off - zero output level
  ENV_ATTACK,        // Attack - linear ramp up to peak
  ENV_PEAK_HOLD,     // Peak Hold - constant output at max. level (.999)
  ENV_DECAY,         // Decay - exponential ramp down to sustain level
  ENV_SUSTAIN,       // Sustain - constant output at preset level
  ENV_RELEASE,       // Release - exponential ramp down to zero level
};

enum  Contour_Gen_Phases  // aka "segments"
{
  CONTOUR_IDLE = 0,  // Idle - maintain start or hold level
  CONTOUR_DELAY,     // Delay after note on, before ramp
  CONTOUR_RAMP,      // Ramp progressing (linear)
  CONTOUR_HOLD       // Hold at constant level indefinitely
};

typedef struct table_of_configuration_params
{
  uint8_t MidiChannel;              // Used only if EEPROM and OLED are OK
  uint8_t AudioAmpldCtrlMode;       // Override patch param AmpldControlSource
  uint8_t VibratoCtrlMode;          // Vibrato Control Mode, dflt: 0 (Off)
  uint8_t PitchBendMode;            // Pitch Bend Control Mode (0: disabled)
  uint8_t PitchBendRange;           // Pitch Bend range, semitones (1..12)
  uint8_t ReverbMix_pc;             // Reverb. wet/dry mix (0..100 %)
  uint8_t PresetLastSelected;       // Preset Last Selected (0..127)
  uint8_t Pitch_CV_BaseNote;        // Lowest note in Pitch CV range (MIDI #)
  bool    Pitch_CV_Quantize;        // Quantize CV pitch to nearest semitone
  bool    CV_ModeAutoSwitch;        // CV Control Mode enabled by GATE+ signal
  bool    CV3_is_Velocity;          // CV3 input controls Velocity (with ENV1)
  bool    MultiTriggerEnab;         // True to enable Multi-trigger keying mode
  short   CV1_FullScale_mV;         // CV1 input calibration constant (mV)
  short   FineTuning_cents;         // Pitch fine-tuning (signed, +/-100 cents)
  uint32_t  EEpromCheckWord;        // Data integrity check (*last entry*)

} ConfigParams_t;

extern  ConfigParams_t  g_Config;     // structure holding configuration params

// Data structure for active patch (g_Patch) and preset patches in flash PM.
// Note: Vibrato control mode is NOT a PATCH parameter; it is a config. param.
//
typedef  struct  synth_patch_param_table
{
  char     PresetName[24];        // Preset (patch) name, up to 22 chars
  uint16_t OscFreqMult[6];        // One of 12 options (encoded 0..11)
  uint16_t OscAmpldModSource[6];  // One of 10 options (encoded 0..9)
  short    OscDetune[6];          // Unit = cents (range 0..+/-600)
  uint16_t MixerInputStep[6];     // Mixer Input Levels (encoded 0..16)
  ////
  uint16_t EnvAttackTime;         // 5..5000+ ms
  uint16_t EnvHoldTime;           // 0..5000+ ms (if zero, skip Decay)
  uint16_t EnvDecayTime;          // 5..5000+ ms
  uint16_t EnvSustainLevel;       // Unit = 1/100 (range 0..100 %)
  uint16_t EnvReleaseTime;        // 5..5000+ ms
  uint16_t AmpControlMode;        // One of 4 options (encoded 0..3)
  ////
  uint16_t ContourStartLevel;     // Unit = 1/100 (range 0..100 %)
  uint16_t ContourDelayTime;      // 0..5000+ ms
  uint16_t ContourRampTime;       // 5..5000+ ms
  uint16_t ContourHoldLevel;      // Unit = 1/100 (range 0..100 %)
  uint16_t Env2DecayTime;         // 5..5000+ ms
  uint16_t Env2SustainLevel;      // Unit = 1/100 (range 0..100 %)
  ////
  uint16_t LFO_Freq_x10;          // LFO frequency x10 (range 5..250)
  uint16_t LFO_RampTime;          // 5..5000+ ms
  uint16_t LFO_FM_Depth;          // Unit = 1/100 semitone (cents, max. 600)
  uint16_t LFO_AM_Depth;          // Unit = 1/100 (0..100 %FS)
  uint16_t MixerOutGain_x10;      // Unit = 1/10  (value = gain x10, 0..100)
  uint16_t LimiterLevelPc;        // Audio limiter level (%), 0: Disabled

} PatchParamTable_t;

extern  const   PatchParamTable_t  g_PresetPatch[];
extern  PatchParamTable_t  g_Patch;   // Active patch data

extern  const   short     g_sine_wave[];
extern  const   uint16_t  g_base2exp[];
extern  const   float     g_FreqMultConst[];
extern  const   uint16_t  g_MixerInputLevel[];

extern  uint8_t  g_MidiChannel;        // 1..16  (16 = broadcast, omni)
extern  uint8_t  g_MidiMode;           // OMNI_ON_MONO or OMNI_OFF_MONO
extern  uint8_t  g_GateState;          // GATE signal state (de-bounced)
extern  bool     g_DisplayEnabled;     // True if OLED is enabled
extern  bool     g_CVcontrolMode;      // True if CV pitch control enabled
extern  bool     g_MidiRxSignal;       // Signal MIDI message received
extern  bool     g_EEpromFault;        // True if EEPROM error on start-up
extern  int      g_DebugData;

extern  const  float  g_NoteFrequency[];

// Functions defined in main source file ...
//
int    GetNumberOfPresets(void);
void   PresetSelect(uint8_t preset);
void   MidiInputService();
void   ProcessMidiMessage(uint8_t *midiMessage, short msgLength);
void   ProcessControlChange(uint8_t *midiMessage);
void   ProcessMidiSystemExclusive(uint8_t *midiMessage, short msgLength);
int    MIDI_GetMessageLength(uint8_t statusByte);
void   CVinputService();
void   DefaultConfigData(void);
uint8_t  FetchConfigData(void);
void   StoreConfigData(void);

// Functions defined in "m0_synth_engine.c" ...
//
void   SynthPrepare();
void   SynthNoteOn(uint8_t note, uint8_t vel);
void   SynthNoteChange(uint8_t note);
void   SynthNoteOff(uint8_t note);
void   SynthPitchBend(int data14);
void   SynthExpression(unsigned data14);
void   SynthModulation(unsigned data14);
void   SynthProcess();
void   SynthSetOscFrequency(float freq_Hz);
void   SynthSetReverbMix(uint8_t rvbmix_pc);
void   SynthTriggerAttack();
void   SynthTriggerRelease();
void   SynthLFO_PhaseSync();


#endif // M0_SYNTH_DEF_H
