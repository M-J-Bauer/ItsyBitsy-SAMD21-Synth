/**
 *   File:    m0_synth_def.h 
 *
 *   Data declarations for SAMD21 'ItsyBitsy M0' sound synthesizer.
 */
#ifndef M0_SYNTH_DEF_H
#define M0_SYNTH_DEF_H

#include "common_def.h"

#define USE_SPI_DAC_FOR_AUDIO    TRUE    // Set FALSE to use MCU on-chip DAC (10 bits)
#define SPI_DAC_CS                  2    // DAC CS/ pin ID
#define MODE_JUMPER                 7    // Mode jumper (JP1) input pin
#define ISR_TESTPOINT              13    // ISR test-point pin
#define CHAN_SWITCH_S1             12    // MIDI channel-select switch S1 (bit 0)
#define CHAN_SWITCH_S2             11    // MIDI channel-select switch S2 (bit 1)
#define CHAN_SWITCH_S3             10    // MIDI channel-select switch S3 (bit 2)
#define CHAN_SWITCH_S4              9    // MIDI channel-select switch S4 (bit 3)

#define WAVE_TABLE_SIZE          2048    // nunber of samples
#define SAMPLE_RATE_HZ          32000    // typically 32,000 or 40,000 Hz
#define MAX_OSC_FREQ_HZ         12000    // must be < 0.4 x SAMPLE_RATE_HZ

#define REVERB_DELAY_MAX_SIZE    2000    // samples 
#define REVERB_LOOP_TIME_SEC     0.04    // seconds (max. 0.05 sec.)
#define REVERB_DECAY_TIME_SEC     1.5    // seconds

#define FIXED_MIN_LEVEL    (1)                   // Minimum non-zero signal level (0.00001)
#define FIXED_MAX_LEVEL  (IntToFixedPt(1) - 1)   // Full-scale normalized signal level (0.99999)
#define FIXED_PT_HALF    (IntToFixedPt(1) / 2)   // constant = 0.5 in fixed_t format
#define MAX_CLIPPING_LEVEL   ((IntToFixedPt(1) * 97) / 100)   // constant = 0.97

// Possible values for config parameter: g_Config.AudioAmpldCtrlMode
// If non-zero, this setting overrides the patch parameter: g_Patch.AmpldControlSource
#define AUDIO_CTRL_BY_PATCH         0    // Audio output control by active patch param
#define AUDIO_CTRL_CONST            1    // Audio output control by fixed level (max)
#define AUDIO_CTRL_ENV1_VELO        2    // Audio output control by ENV1 * Velocity
#define AUDIO_CTRL_EXPRESS          3    // Audio output control by Expression (CC2,7,11)

// Possible values for config parameter: g_Config.VibratoCtrlMode
#define VIBRATO_DISABLED            0    // Vibrato disabled
#define VIBRATO_BY_EFFECT_SW        1    // Vibrato controlled by effect switch (TBD))
#define VIBRATO_BY_MODN_CC          2    // Vibrato controlled by Modulation (CC1)
#define VIBRATO_AUTOMATIC           3    // Vibrato automatic, delay + ramp, all osc.

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

// Possible values for patch parameter: g_Patch.AmpldControlSource
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
#define SYSTEM_MSG_EOX       0xF7    // system msg terminator
#define SYS_EXCL_REMI_ID     0x73    // arbitrary pick... hope it's free!
#define CC_MODULATION        1       // Control change High byte
#define CC_BREATH_PRESSURE   2       //    ..     ..     ..
#define CC_CHANNEL_VOLUME    7       //    ..     ..     ..
#define CC_EXPRESSION        11      //    ..     ..     ..
#define MIDI_MSG_MAX_LENGTH  16      // not in MIDI specification!

typedef struct table_of_configuration_params
{
    uint8   MidiMode;                 // MIDI IN mode (Omni on/off, mono)
    uint8   MidiChannel;              // MIDI IN channel, 1..15, dflt 1
    uint8   MidiExpressionCCnum;      // MIDI IN breath/pressure CC number, dflt 2
    uint8   AudioAmpldCtrlMode;       // Override patch param AmpldControlSource
    uint8   VibratoCtrlMode;          // Vibrato Control Mode, dflt: 0 (Off)
    uint8   PitchBendEnable;          // Pitch Bend Enable (0: disabled)
    uint8   PitchBendRange;           // Pitch Bend range, semitones (1..12)
    uint8   ReverbAtten_pc;           // Reverberation attenuator gain (1..100 %)
    uint8   ReverbMix_pc;             // Reverberation wet/dry mix (1..100 %)

} ConfigParams_t;

extern  ConfigParams_t  g_Config;     // structure holding configuration params

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

// Data structure for active patch (g_Patch) and preset patches in flash PM.
// Note: Vibrato control mode is NOT a PATCH parameter; it is a config. param.
//
typedef  struct  synth_patch_param_table
{
    char   PresetName[24];        // Preset (patch) name, up to 22 chars
    uint16 OscFreqMult[6];        // One of 12 options (encoded 0..11)
    uint16 OscAmpldModSource[6];  // One of 10 options (encoded 0..9)
    short  OscDetune[6];          // Unit = cents (range 0..+/-600)
    uint16 MixerInputStep[6];     // Mixer Input Level (encoded 0..16)
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

// Functions defined in main source file ...
//
int    GetNumberOfPresets(void);
void   PresetSelect(uint8 preset);
void   MidiInputService();
void   ProcessMidiMessage(uint8 *midiMessage, short msgLength);
void   ProcessControlChange(uint8 *midiMessage);
void   ProcessMidiSystemExclusive(uint8 *midiMessage, short msgLength);
int    MIDI_GetMessageLength(uint8 statusByte);
void   DefaultConfigData(void);

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

#endif // M0_SYNTH_DEF_H
