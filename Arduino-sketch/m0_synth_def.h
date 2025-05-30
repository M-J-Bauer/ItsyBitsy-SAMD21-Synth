/**
 *   File:    m0_synth_def.h 
 *
 *   Data definitions & declarations for SAMD21 'ItsyBitsy M0' sound synthesizer.
 */
#ifndef M0_SYNTH_DEF_H
#define M0_SYNTH_DEF_H

#include "common_def.h"

#define FIRMWARE_VERSION  "2.0"

#define USE_SAMD21_M0_MINI_MCU     FALSE  // TRUE => Robotdyn SAMD21 M0-MINI board

// Other firmware build options -- set according to user preference...
#define EEPROM_IS_INSTALLED        TRUE   // FALSE => EEPROM not installed!
#define DEFAULT_MASTER_TUNING      0      // Master Tune parameter (-100 ~ +100 cents)
#define APPLY_VELOCITY_EXPL_CURVE  FALSE  // TRUE => Apply "exponential" ampld curve
#define LEGATO_ENABLED_ALWAYS      FALSE  // TRUE => Legato Mode always enabled
#define USE_SPI_DAC_FOR_AUDIO      TRUE   // FALSE => Use MCU on-chip DAC (pin A0)

//=======================================================================================
#if (USE_SAMD21_M0_MINI_MCU)  // Robotdyn MCU board...
#define HOME_SCREEN_SYNTH_DESCR  "Voice Module"  // 12 chars max.
#else  // assume Adafruit ItsyBitsy M0 Express
#define HOME_SCREEN_SYNTH_DESCR  "ItsyBitsy M0"  // 12 chars max.
#endif

#define CV_MODE_JUMPER              7    // CV Mode jumper (JP1) input pin
#define TESTPOINT1                 13    // Scope test-point pin
#define TESTPOINT2                  5    // Scope test-point pin
#define CHAN_SWITCH_S1             12    // MIDI channel-select switch S1 (bit 0)
#define CHAN_SWITCH_S2             11    // MIDI channel-select switch S2 (bit 1)
#define CHAN_SWITCH_S3             10    // MIDI channel-select switch S3 (bit 2)
#define CHAN_SWITCH_S4              9    // MIDI channel-select switch S4 (bit 3)
#define BUTTON_A_PIN                3    // Button [A] input (active low)
#define GATE_INPUT                 19    // GATE input (digital, active High)

#if (USE_SAMD21_M0_MINI_MCU)  // Pins D2 and D4 are reversed on RobotDyn MCU board!
#define BUTTON_B_PIN                2    // Button [B] input (active low)
#define SPI_DAC_CS                  4    // DAC CS/ pin ID
#else
#define BUTTON_B_PIN                4
#define SPI_DAC_CS                  2 
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
  uint8   AudioAmpldCtrlMode;       // Override patch param AmpldControlSource
  uint8   VibratoCtrlMode;          // Vibrato Control Mode, dflt: 0 (Off)
  uint8   PitchBendMode;            // Pitch Bend Control Mode (0: disabled)
  uint8   PitchBendRange;           // Pitch Bend range, semitones (1..12)
  uint8   ReverbMix_pc;             // Reverb. wet/dry mix (0..100 %)
  uint8   PresetLastSelected;       // Preset Last Selected (0..127)
  uint8   Pitch_CV_BaseNote;        // Lowest note in Pitch CV range (MIDI #)
  bool    Pitch_CV_Quantize;        // Quantize CV pitch to nearest semitone
  bool    CV_ModeAutoSwitch;        // CV Control Mode enabled by GATE+ signal
  bool    CV3_is_Velocity;          // CV3 input controls Velocity (with ENV1)
  short   CV1_FullScale_mV;         // CV1 input calibration constant (mV)
  short   MasterTuneOffset;         // Pitch fine-tuning (+/-100 cents)
  uint32  EEpromCheckWord;          // Data integrity check (*last entry*)

} ConfigParams_t;

extern  ConfigParams_t  g_Config;     // structure holding configuration params

// Data structure for active patch (g_Patch) and preset patches in flash PM.
// Note: Vibrato control mode is NOT a PATCH parameter; it is a config. param.
//
typedef  struct  synth_patch_param_table
{
  char   PresetName[24];        // Preset (patch) name, up to 22 chars
  uint16 OscFreqMult[6];        // One of 12 options (encoded 0..11)
  uint16 OscAmpldModSource[6];  // One of 10 options (encoded 0..9)
  short  OscDetune[6];          // Unit = cents (range 0..+/-600)
  uint16 MixerInputStep[6];     // Mixer Input Levels (encoded 0..16)
  ////
  uint16 EnvAttackTime;         // 5..5000+ ms
  uint16 EnvHoldTime;           // 0..5000+ ms (if zero, skip Decay)
  uint16 EnvDecayTime;          // 5..5000+ ms
  uint16 EnvSustainLevel;       // Unit = 1/100 (range 0..100 %)
  uint16 EnvReleaseTime;        // 5..5000+ ms
  uint16 AmpControlMode;        // One of 4 options (encoded 0..3)
  ////
  uint16 ContourStartLevel;     // Unit = 1/100 (range 0..100 %)
  uint16 ContourDelayTime;      // 0..5000+ ms
  uint16 ContourRampTime;       // 5..5000+ ms
  uint16 ContourHoldLevel;      // Unit = 1/100 (range 0..100 %)
  uint16 Env2DecayTime;         // 5..5000+ ms
  uint16 Env2SustainLevel;      // Unit = 1/100 (range 0..100 %)
  ////
  uint16 LFO_Freq_x10;          // LFO frequency x10 (range 5..250)
  uint16 LFO_RampTime;          // 5..5000+ ms
  uint16 LFO_FM_Depth;          // Unit = 1/100 semitone (cents, max. 600)
  uint16 LFO_AM_Depth;          // Unit = 1/100 (0..100 %FS)
  uint16 MixerOutGain_x10;      // Unit = 1/10  (value = gain x10, 0..100)
  uint16 LimiterLevelPc;        // Audio limiter level (%), 0: Disabled

} PatchParamTable_t;

extern  const   PatchParamTable_t  g_PresetPatch[];
extern  PatchParamTable_t  g_Patch;   // Active patch data

extern  const   short   g_sine_wave[];
extern  const   uint16  g_base2exp[];
extern  const   float   g_FreqMultConst[];
extern  const   uint16  g_MixerInputLevel[];

extern  uint8  g_MidiChannel;        // 1..16  (16 = broadcast, omni)
extern  uint8  g_MidiMode;           // OMNI_ON_MONO or OMNI_OFF_MONO
extern  uint8  g_GateState;          // GATE signal state (de-bounced)
extern  bool   g_DisplayEnabled;     // True if OLED is enabled
extern  bool   g_CVcontrolMode;      // True if CV pitch control enabled
extern  bool   g_MidiRxSignal;       // Signal MIDI message received
extern  bool   g_EEpromFaulty;       // True if EEPROM error or not fitted
extern  uint8  g_LegatoMode;         // Switch ON or OFF using MIDI CC68 msg
extern  int    g_DebugData;

extern  const  float  g_NoteFrequency[];

// Functions defined in main source file ...
//
int    GetNumberOfPresets(void);
void   PresetSelect(uint8 preset);
void   MidiInputService();
void   ProcessMidiMessage(uint8 *midiMessage, short msgLength);
void   ProcessControlChange(uint8 *midiMessage);
void   ProcessMidiSystemExclusive(uint8 *midiMessage, short msgLength);
int    MIDI_GetMessageLength(uint8 statusByte);
void   CVinputService();
void   DefaultConfigData(void);
uint8  FetchConfigData(void);
void   StoreConfigData(void);

// Functions defined in "m0_synth_engine.c" ...
//
void   SynthPrepare();
void   SynthNoteOn(uint8 note, uint8 vel);
void   SynthNoteChange(uint8 note);
void   SynthNoteOff(uint8 note);
void   SynthPitchBend(int data14);
void   SynthExpression(unsigned data14);
void   SynthModulation(unsigned data14);
void   SynthProcess();
void   SynthSetOscFrequency(float freq_Hz);
void   SynthTriggerAttack();
void   SynthTriggerRelease();


#endif // M0_SYNTH_DEF_H
