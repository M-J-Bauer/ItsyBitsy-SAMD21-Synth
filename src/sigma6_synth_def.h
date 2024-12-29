/**
 *   File:    sigma6_synth_def.h 
 *
 *   Data declarations for REMI mk2 sound synthesizer module.
 */
#ifndef SIGMA6_SYNTH_DEF_H
#define SIGMA6_SYNTH_DEF_H

#ifdef __32MX440F256H__
#include "pic32mx440_low_level.h"
#else
#include "pic32_low_level.h"
#endif

#define WAVE_TABLE_SIZE          2048    // samples
#define SAMPLE_RATE_HZ          40000    // For wave-table Oscillators
#define MIDI_EXPRN_ADJUST_PC      125    // Pressure/expression compensation factor (%)

#define REVERB_DELAY_MAX_SIZE    2000    // samples 
#define REVERB_LOOP_TIME_SEC     0.04    // seconds (max. 0.05 sec.)
#define REVERB_DECAY_TIME_SEC     1.5    // seconds

#define FIXED_MIN_LEVEL  (1)                     // Minimum normalized signal level (0.00001)
#define FIXED_MAX_LEVEL  (IntToFixedPt(1) - 1)   // Full-scale normalized signal level (0.9999)
#define FIXED_PT_HALF    (IntToFixedPt(1) / 2)   // Constant 0.5 in fixed_t format

// Possible values for config parameter: g_Config.AudioAmpldCtrlMode
// If non-zero, this setting overrides the patch parameter: g_Patch.AmpldControlSource
#define AUDIO_CTRL_BY_PATCH         0    // Audio output control by active patch param
#define AUDIO_CTRL_CONST            1    // Audio output control by fixed level (FS)
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
    uint16 OscAmpldModSource[6];  // One of 8 options (encoded 0..7)
    short  OscDetune[6];          // Unit = cents (range 0..+/-600)
    uint16 MixerInputStep[6];     // Unit = 1/10 (range 0..16, log scale)
    uint16 EnvAttackTime;         // 5..5000+ ms
    uint16 EnvHoldTime;           // 0..5000+ ms (if zero, skip Decay)
    uint16 EnvDecayTime;          // 5..5000+ ms
    uint16 EnvSustainLevel;       // Unit = 1/100 (range 0..100 %)
    uint16 EnvReleaseTime;        // 5..5000+ ms
    uint16 EnvVelocityMod;        // Attack modified by key velocity (%)
    uint16 ContourStartLevel;     // Unit = 1/100 (range 0..100 %)
    uint16 ContourDelayTime;      // 0..5000+ ms
    uint16 ContourRampTime;       // 5..5000+ ms
    uint16 ContourHoldLevel;      // Unit = 1/100 (range 0..100 %)
    uint16 Env2DecayTime;         // 5..5000+ ms
    uint16 Env2SustainLevel;      // Unit = 1/100 (range 0..100 %)
    uint16 LFO_Freq_x10;          // LFO frequency x10 (range 5..250)
    uint16 LFO_RampTime;          // 5..5000+ ms
    uint16 LFO_FM_Depth;          // Unit = 1/100 semitone (cents, max. 600)
    uint16 LFO_AM_Depth;          // Unit = 1/100 (0..100 %FS)
    uint16 MixerOutGain_x10;      // Unit = 1/10  (value = gain x10, 0..100)
    uint16 AmpldControlSource;    // One of 4 options (encoded 0..3)

} PatchParamTable_t;


extern  const   PatchParamTable_t  g_PresetPatch[];
extern  const   int16   g_sine_wave[];
extern  const   uint16  g_base2exp[];
extern  const   float   g_FreqMultConst[];
extern  const   uint16  g_MixerInputLevel[];

extern  PatchParamTable_t  g_Patch;   // Active patch data

int    GetNumberOfPresets(void);   // fn defined in sigma6_synth_data.c

// Functions defined in "remi_synth2_engine.c" available to external modules:
//
void   SynthPrepare();
void   SynthNoteOn(uint8 note, uint8 vel);
void   SynthNoteChange(uint8 note);
void   SynthNoteOff(uint8 note);
void   SynthPitchBend(int data14);
void   SynthExpression(unsigned data14);
void   SynthModulation(unsigned data14);
void   SynthProcess();

bool     isNoteOn();
bool     isSynthActive();
fixed_t  GetExpressionLevel(void);
fixed_t  GetModulationLevel(void);
fixed_t  GetPitchBendFactor(void);
fixed_t  Base2Exp(fixed_t xval);

#endif // SIGMA6_SYNTH_DEF_H
