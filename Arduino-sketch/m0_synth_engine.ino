/*
 * File:       m0_synth_engine (.ino)
 *
 * Module:     Sigma-6 sound synthesizer implementation
 *
 * Platform:   Adafruit 'ItsyBitsy M0 Express' or compatible (MCU: ATSAMD21G18)
 *
 * Author:     M.J.Bauer, 2025 -- www.mjbauer.biz
 *
 * Licence:    Open Source (Unlicensed) -- free to copy, distribute, modify
 */
#include <SPI.h>
#include "m0_synth_def.h"

// Functions "private" within this source file ...
//
void   AmpldEnvelopeGenerator();
void   TransientEnvelopeGen();
void   ContourGenerator();
void   AudioLevelController();
void   OscAmpldModulation();
void   OscFreqModulation();
void   LowFrequencyOscillator();
void   VibratoRampGenerator();

fixed_t  ReverbDelayLine[REVERB_DELAY_MAX_SIZE];  // fixed-point samples

PatchParamTable_t  g_Patch;     // active patch parameters

static int32    m_OscStepInit[6];         // Osc phase step values at Note-On
static int32    m_OscStepDetune[6];       // Osc phase step values with de-tune applied
static bool     m_OscMuted[6];            // True if osc freq > 0.4 x SAMPLE_RATE_HZ       
static int32    m_LFO_Step;               // LFO "phase step" (fixed-point format)
static fixed_t  m_LFO_output;             // LFO output signal, normalized, bipolar (+/-1.0)
static fixed_t  m_RampOutput;             // Vibrato Ramp output level,  normalized (0..+1)
static fixed_t  m_ExpressionLevel;        // Expression level, normalized, unipolar (0..+1)
static fixed_t  m_ModulationLevel;        // Modulation level, normalized, unipolar (0..+1)
static fixed_t  m_PitchBendFactor;        // Pitch-Bend factor, normalized, bipolar (+/-1.0)

static fixed_t  m_ENV1_Output;            // Envelope Generator #1 output  (0 ~ 1.0)
static fixed_t  m_ENV2_Output;            // Envelope Generator #2 output  (0 ~ 1.0)
static fixed_t  m_ContourOutput;          // Contour EG output, normalized (0 ~ 1.0)
static fixed_t  m_KeyVelocity;            // Note-On Velocity, normalized  (0 ~ 1.0)
static bool     m_TriggerAttack1;         // Signal to put ENV1 into attack
static bool     m_TriggerRelease1;         // Signal to put ENV1 into release
static bool     m_TriggerAttack2;         // Signal to put ENV2 into attack
static bool     m_TriggerRelease2;        // Signal to put ENV2 into release
static bool     m_TriggerContour;         // Signal to start Contour generator
static bool     m_TriggerReset;           // Signal to reset Contour generator
static bool     m_LegatoNoteChange;       // Signal Legato note change to Vibrato func.
static uint8    m_NoteOn;                 // TRUE if Note ON, ie. "gated", else FALSE
static uint8    m_NotePlaying;            // MIDI note number of note playing

static int      m_RvbDelayLen;            // Reverb. delay line length (samples)
static fixed_t  m_RvbDecay;               // Reverb. decay factor
static uint16   m_RvbAtten;               // Reverb. attenuation factor (0..128)
static uint16   m_RvbMix;                 // Reverb. wet/dry mix ratio (0..128)

volatile uint8    v_SynthEnable;          // Signal to enable synth sampling routine
volatile int32    v_OscAngle[6];          // Osc sample pos'n in wave-table [16:16]
volatile int32    v_OscStep[6];           // Osc sample pos'n increment [16:16]
volatile uint16   v_OscAmpldModn[6];      // Osc ampld modulation x1024 (0..1024)
volatile uint16   v_MixerLevel[6];        // Mixer input levels x1000 (0..1000)
volatile uint16   v_MixerOutGain;         // Mixer output gain x100 (x0.8 - x10 %)
volatile fixed_t  v_LimiterLevelPos;      // Audio limiter level (pos. peak, normalized)
volatile fixed_t  v_LimiterLevelNeg;      // Audio limiter level (neg. peak, normalized)
volatile uint16   v_OutputLevel;          // Audio output level x1000 (0..1000)

// Look-up table giving frequencies of notes on the chromatic scale.
// The array covers a 9-octave range beginning with C0 (MIDI note number 12),
// up to C9 (120).  Subtract 12 from MIDI note number to get table index.
// Table index range:  0..108
//
const  float  g_NoteFrequency[] =
{
    // C0      C#0       D0      Eb0       E0       F0      F#0       G0
    16.3516, 17.3239, 18.3540, 19.4455, 20.6017, 21.8268, 23.1247, 24.4997,
    // Ab0      A0      Bb0       B1       C1      C#1       D1      Eb1
    25.9566, 27.5000, 29.1353, 30.8677, 32.7032, 34.6478, 36.7081, 38.8909,
    // E1       F1      F#1       G1      Ab1       A1      Bb1       B1
    41.2034, 43.6535, 46.2493, 48.9994, 51.9131, 55.0000, 58.2705, 61.7354,
    // C2      C#2       D2      Eb2       E2       F2      F#2       G2
    65.4064, 69.2957, 73.4162, 77.7817, 82.4069, 87.3071, 92.4986, 97.9989,
    // Ab2      A2      Bb2       B2       C3      C#3       D3      Eb3
    103.826, 110.000, 116.541, 123.471, 130.813, 138.591, 146.832, 155.563,
    // E3       F3      F#3       G3      Ab3       A3      Bb3       B3
    164.814, 174.614, 184.997, 195.998, 207.652, 220.000, 233.082, 246.942,
    // C4      C#4       D4      Eb4       E4       F4      F#4       G4
    261.626, 277.183, 293.665, 311.127, 329.628, 349.228, 369.994, 391.995,
    // Ab4      A4      Bb4       B4       C5      C#5       D5      Eb5
    415.305, 440.000, 466.164, 493.883, 523.251, 554.365, 587.330, 622.254,
    // E5       F5      F#5       G5      Ab5       A5      Bb5       B5
    659.255, 698.456, 739.989, 783.991, 830.609, 880.000, 932.328, 987.767,
    // C6      C#6       D6      Eb6       E6       F6      F#6       G6
    1046.50, 1108.73, 1174.66, 1244.51, 1318.51, 1396.91, 1479.98, 1567.98,
    // Ab6      A6      Bb6       B6       C7      C#7       D7      Eb7
    1661.22, 1760.00, 1864.66, 1975.53, 2093.00, 2217.46, 2349.32, 2489.02,
    // E7       F7      F#7       G7      Ab7       A7      Bb7       B7
    2637.02, 2793.83, 2959.96, 3135.96, 3322.44, 3520.00, 3729.31, 3951.07,
    // C8      C#8       D8      Eb8       E8       F8      F#8       G8
    4186.01, 4434.92, 4698.64, 4978.04, 5274.04, 5587.66, 5919.92, 6271.92,
    // Ab8      A8      Bb8       B8       C9
    6644.88, 7040.00, 7458.62, 7902.14, 8372.02
};

// Set of 12 fixed values for Osc. Frequency Mutiplier:
const  float  g_FreqMultConst[] = 
        { 0.5, 1.0, 1.333333, 1.5, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0 };

// Values for Audio Ampld Level... 0 + 16 fixed levels on 3dB log scale
const  uint16  g_AmpldLevelLogScale_x1000[] =  
        { 0, 5, 8, 11, 16, 22, 31, 44, 63, 88, 125, 177, 250, 353, 500, 707, 1000 };


/*`````````````````````````````````````````````````````````````````````````````````````````````````
 * Function:     Initialization of "constant" synth data environment.
 *               Called by PresetSelect().
 *               Must be called after a change in any config. parameter.
 */
void  SynthPrepare()
{
    static bool SPI_setupDone;
    float   rvbDecayRatio;

    v_SynthEnable = 0;      // Disable the synth tone-generator

    if (SPI_setupDone)  SPI.endTransaction();  // already begun
    else  // initialize SPI -- once only at power-on/reset
    { 
        SPI.begin();  
        SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));  
        SPI_setupDone = TRUE; 
    }  
    
    m_NoteOn = FALSE;       // No note playing
    m_TriggerRelease1 = 1;  // Reset ENV1
    m_TriggerRelease2 = 1;  // Reset ENV2
    m_ExpressionLevel = 0;  // Mute audio output
    m_KeyVelocity = (IntToFixedPt(1) * 80) / 100;  // in case CV mode selected
    
    // Calculate reverb effect constants...
    m_RvbDelayLen = (int) (REVERB_LOOP_TIME_SEC * SAMPLE_RATE_HZ);  // loop time is 0.04f
    rvbDecayRatio = (float) REVERB_LOOP_TIME_SEC / REVERB_DECAY_TIME_SEC;
    m_RvbDecay = FloatToFixed( powf(0.001f, rvbDecayRatio) );  // = 0.83 (approx)   
    m_RvbAtten = ((uint16)REVERB_ATTENUATION_PC << 7) / 100;  // = 0..127
    m_RvbMix = ((uint16)g_Config.ReverbMix_pc << 7) / 100;  // = 0..127

    v_SynthEnable = 1;      // Let 'er rip, Boris!
}


/*
 * Function:     If a note is already playing, perform a Legato note change;
 *               otherwise initiate a new note.
 *
 * Entry args:   noteNum  = MIDI standard note number, range: 12 ~ 120 (C0..C9),
 *                          e.g. note #60 = C4 = middle-C.
 *               velocity = MIDI velocity value (0..127)
 *
 * Output data:  m_KeyVelocity (usage dependent on synth settings)
 *
 * When a new note is initiated, the function prepares the synth wave-table oscillators
 * to play the given note, then triggers the envelope shaper to start the 'Attack' phase.
 */
void  SynthNoteOn(uint8 noteNum, uint8 velocity)
{
  if (!m_NoteOn)    // Note OFF -- Initiate a new note...
  {
    SynthNoteChange(noteNum);  // Set OSC frequencies, etc

    // A square-law curve is applied to velocity
    m_KeyVelocity = IntToFixedPt((int) velocity) / 128;  // normalized
    m_KeyVelocity = MultiplyFixed(m_KeyVelocity, m_KeyVelocity);  // squared
    m_LegatoNoteChange = 0;    // Not a Legato event
    SynthTriggerAttack();
  }
  else  // Note already playing -- Legato note change
  {
    SynthNoteChange(noteNum);  // Adjust OSC1 and OSC2 frequencies
    m_LegatoNoteChange = 1;    // Signal Note-Change event (for vibrato fn)
  }
}


void  SynthTriggerAttack()
{
  m_TriggerAttack1 = 1;
  m_TriggerAttack2 = 1;
  m_TriggerContour = 1;
  m_NoteOn = TRUE;
}


void  SynthTriggerRelease()
{
  m_TriggerRelease1 = 1;
  m_TriggerRelease2 = 1;
  m_TriggerReset = 1;
  m_NoteOn = FALSE;
}


/*
 * Function:     Set the pitch of a note to be initiated, or change pitch of the note
 *               in progress, without affecting the amplitude envelopes (i.e. no re-attack).
 *               This function may be used where a "legato" effect is required.
 *               (See also: SynthNoteOn() function.)
 *
 * Entry args:   noteNum = MIDI standard note number. (Note #60 = C4 = middle-C.)
 *               The synth supports note numbers in the range: 12 (C0) to 120 (C9).
 */
void  SynthNoteChange(uint8 noteNum)
{
  float   oscFreq, freqMult;
  int32   tableSize, oscStep;  // 16:16 bit fixed point
  int     osc;

  // Ensure note number is within synth range (12 ~ 108)
  noteNum &= 0x7F;
  if (noteNum > 120)  noteNum -= 12;   // too high
  if (noteNum < 12)   noteNum += 12;   // too low
  m_NotePlaying = noteNum;
  
  for (osc = 0;  osc < 6;  osc++)  // Update 6 oscillators...
  {
    // Convert MIDI note number to frequency (Hz) and apply OscFreqMult param.
    freqMult = g_FreqMultConst[g_Patch.OscFreqMult[osc]];  // float
    oscFreq = g_NoteFrequency[noteNum-12] * freqMult;
    if (oscFreq > MAX_OSC_FREQ_HZ)  m_OscMuted[osc] = TRUE;
    else  m_OscMuted[osc] = FALSE;

    // Initialize oscillator "phase step" for use in audio ISR
    tableSize = (int32) WAVE_TABLE_SIZE << 16;  // convert to 16:16 fixed-pt
    oscStep = (int32) ((tableSize * oscFreq) / SAMPLE_RATE_HZ);
    m_OscStepInit[osc] = oscStep;
  }
}


// This function is provided for CV control mode to set oscillator pitch.
//
void  SynthSetOscFrequency(float fundamental_Hz)
{
  float   oscFreq, freqMult;
  int32   tableSize, oscStep;  // 16:16 bit fixed point
  int     osc;

  for (osc = 0;  osc < 6;  osc++)  // Set freq in 6 oscillators...
  {
    freqMult = g_FreqMultConst[g_Patch.OscFreqMult[osc]];  // float
    oscFreq = fundamental_Hz * freqMult;
    if (oscFreq > MAX_OSC_FREQ_HZ)  m_OscMuted[osc] = TRUE;
    else  m_OscMuted[osc] = FALSE;

    // Initialize oscillator "phase step" for use in audio ISR
    tableSize = (int32) WAVE_TABLE_SIZE << 16;  // convert to 16:16 fixed-pt
    oscStep = (int32) ((tableSize * oscFreq) / SAMPLE_RATE_HZ);
    m_OscStepInit[osc] = oscStep;
  }
}


/*
 * Function:     End the note playing, if it matches the given note number.
 *
 * Entry args:   noteNum = MIDI standard note number of note to be ended.
 *
 * The function puts envelope shapers into the 'Release' phase. The note will be
 * terminated by the synth process (B/G task) when the release time expires, or if
 * a new note is initiated prior.
 */
void  SynthNoteOff(uint8 noteNum)
{
    noteNum &= 0x7F;
    if (noteNum > 120)  noteNum -= 12;   // too high
    if (noteNum < 12)   noteNum += 12;   // too low
    if (noteNum == m_NotePlaying) SynthTriggerRelease();
}


/*
 * Function:     Set the "Expression Level" according to a given data value.
 *               Sourced from MIDI Control Change message (CC# = 02, 07 or 11).
 *
 * Entry args:   data14 = MIDI expression/pressure value (14 bits, unsigned).
 *
 * Output:       (fixed_t) m_ExpressionLevel = normalized level (0..+1.0),
 *                         capped at 0.990 (99%FS)
 *
 * Note:   A square-law is applied to the data value to approximate an exponential
 *         response curve. The output expression level is fixed point, normalized.
 */
void   SynthExpression(unsigned data14)
{
    uint32  ulval;
    fixed_t level;
    fixed_t levelMax = (IntToFixedPt(1) * 99) / 100;  // = 0.99

    ulval = ((uint32) data14 * data14) / 16384;  // apply square law
    ulval = ulval << 6;   // scale to 20 bits (fractional part)
    level = (fixed_t) ulval;  // convert to fixed-point fraction
    if (level > levelMax) level = levelMax;  // limit at 0.99

    m_ExpressionLevel = level;
}


/*
 * Function:     Modify pitch of note in progress according to Pitch-Bend data value.
 *
 * Entry args:   bipolarPosn = signed integer representing Pitch Bend lever position,
 *                        in the range +/-8000 (14 bits).  Centre pos'n is 0.
 *
 * Output data:  m_PitchBendFactor, which is processed by the real-time synth function
 *               OscFreqModulation() while a note is in progress.
 */
void   SynthPitchBend(int bipolarPosn)
{
    // Scale lever position (arg) according to 'PitchBendRange' config param.
    // PitchBendRange may be up to 12 semitones (ie. 1 octave maximum).
    int  posnScaled = (bipolarPosn * g_Config.PitchBendRange) / 12;  // +/-8K max.
    
    // Convert to 20-bit *signed* fixed-point fraction  (13 + 7 = 20 bits)
    m_PitchBendFactor = (fixed_t) (posnScaled << 7);
}


// Function included for diagnostic info
//
fixed_t  GetPitchBendFactor()
{
    return  m_PitchBendFactor;
}


/*
 * Function:     Control synth effect(s) according to MIDI Modulation data (CC#1);
 *               e.g. vibrato depth, noise level, filter freq., etc.
 *               The effect(s) to be controlled is a function of the synth patch^.
 *
 * Entry args:   data14 = unsigned integer representing Modulation Lever position;
 *                        range 0..16383 (= 2^14 - 1).
 *
 * Output:       m_ModulationLevel, normalized fixed-pt number in the range 0..+1.0.
 *
 */
void   SynthModulation(unsigned data14)
{
    if (data14 < (16 * 1024))
        m_ModulationLevel = (fixed_t) ((uint32) data14 << 6);
    else   m_ModulationLevel = FIXED_MAX_LEVEL;
}


/*`````````````````````````````````````````````````````````````````````````````````````````````````
 * Function:  SynthProcess()
 *
 * Overview:  Periodic background task called at 1ms intervals which performs most of the
 *            real-time sound synthesis computations, except those which need to be executed
 *            at the PCM audio sampling rate; these are done by the Timer/Counter ISR.
 *
 * This task implements the envelope shapers, oscillator pitch bend and vibrato (LFO), mixer
 * input level control, audio output amplitude control, etc.
 *
 * Some processing is done at 1ms intervals (1000Hz), while other tasks are done at longer
 * intervals, e.g. 5ms (200Hz) where timing is not so critical and/or more intensive
 * computation, e.g. floating point arithmetic, is needed.
 */
void  SynthProcess()
{
    static  int  count5ms;

    AmpldEnvelopeGenerator();
    TransientEnvelopeGen();
    ContourGenerator();
    LowFrequencyOscillator();
    AudioLevelController();

    if (++count5ms >= 5)
    {
        count5ms = 0;
        VibratoRampGenerator();
        OscFreqModulation();
        OscAmpldModulation();
    }
}


/*
 * Function:  AmpldEnvelopeGenerator()
 *
 * Overview:  Amplitude envelope generator (ENV1)
 *            Routine called by the Synth Process at 1ms intervals.
 *
 * Output:    (fixed_t) m_ENV1_Output = Envelope output level, normalized (0 ~ +1.00)
 *
 */
void  AmpldEnvelopeGenerator()
{
    static  uint8    EnvSegment;      // Envelope segment (aka "phase")
    static  uint32   envPhaseTimer;   // Time elapsed in envelope phase (ms)
    static  fixed_t  sustainLevel;    // Envelope sustain level, norm. (0 ~ 1.000)
    static  fixed_t  timeConstant;    // 20% of decay or release time (ms)
    static  fixed_t  ampldDelta;      // Step change in Env Ampld in 1ms
    static  fixed_t  ampldMaximum;    // Peak value of Envelope Ampld

    if (m_TriggerAttack1)
    {
        m_TriggerAttack1 = 0;
        m_TriggerRelease1 = 0;
        envPhaseTimer = 0;
        sustainLevel = IntToFixedPt((int) g_Patch.EnvSustainLevel) / 100;
        ampldMaximum = FIXED_MAX_LEVEL;  // for Peak-Hold phase
        if (g_Patch.EnvHoldTime == 0)  ampldMaximum = sustainLevel;  // No Peak-Hold phase
        ampldDelta = ampldMaximum / g_Patch.EnvAttackTime;  // step change in 1ms
        EnvSegment = ENV_ATTACK;
    }

    if (m_TriggerRelease1)
    {
        m_TriggerRelease1 = 0;
        timeConstant = g_Patch.EnvReleaseTime / 5;
        envPhaseTimer = 0;
        EnvSegment = ENV_RELEASE;
    }

    switch (EnvSegment)
    {
    case ENV_IDLE:          // Idle - zero output level
    {
        m_ENV1_Output = 0;
        break;
    }
    case ENV_ATTACK:        // Attack - linear ramp up to peak, or to the sustain level
    {
        if (m_ENV1_Output < ampldMaximum) m_ENV1_Output += ampldDelta;
        if (++envPhaseTimer >= g_Patch.EnvAttackTime)  // attack time ended
        {
            m_ENV1_Output = ampldMaximum;
            envPhaseTimer = 0;
            if (g_Patch.EnvHoldTime == 0)  EnvSegment = ENV_SUSTAIN; // skip peak and decay
            else  EnvSegment = ENV_PEAK_HOLD;  // run all phases
        }
        break;
    }
    case ENV_PEAK_HOLD:     // Peak Hold - constant output level (0.99999)
    {
        if (++envPhaseTimer >= g_Patch.EnvHoldTime)  // Peak-hold time ended
        {
            timeConstant = g_Patch.EnvDecayTime / 5;  // for Decay phase
            envPhaseTimer = 0;
            EnvSegment = ENV_DECAY;
        }
        break;
    }
    case ENV_DECAY:         // Decay - exponential ramp down to sustain level
    {
        ampldDelta = (m_ENV1_Output - sustainLevel) / timeConstant;  // step in 1ms
        if (ampldDelta == 0)  ampldDelta = FIXED_MIN_LEVEL;
        if (m_ENV1_Output >= (sustainLevel + ampldDelta))  m_ENV1_Output -= ampldDelta;
        // Allow 10 x time-constant for decay phase to complete
        if (++envPhaseTimer >= (g_Patch.EnvDecayTime * 2))  EnvSegment = ENV_SUSTAIN;
        break;
    }
    case ENV_SUSTAIN:       // Sustain constant level -- waiting for m_TriggerRelease1
    {
        break;
    }
    case ENV_RELEASE:       // Release - exponential ramp down to zero level
    {
        // timeConstant and envPhaseTimer are set by the trigger condition, above.
        ampldDelta = m_ENV1_Output / timeConstant;
        if (ampldDelta == 0)  ampldDelta = FIXED_MIN_LEVEL;
        if (m_ENV1_Output >= ampldDelta)  m_ENV1_Output -= ampldDelta;
        // Allow 10 x time-constant for release phase to complete
        if (++envPhaseTimer >= (g_Patch.EnvReleaseTime * 2))  EnvSegment = ENV_IDLE;
        break;
    }
    }  // end switch
}


/*
 * Function:  TransientEnvelopeGen()
 *
 * Overview:  Transient envelope generator == ENV2.
 *            ENV2 output can be used to amplitude-modulate any of the 6 oscillators.
 *            Routine called by the Synth Process at 1ms intervals.
 *
 * Output:    (fixed_t) m_ENV2_Output = Envelope #2 output level, norm. (0 ~ 0.9999)
 *
 */
void   TransientEnvelopeGen()
{
    static  uint8    EnvSegment;      // Envelope segment (aka "phase")
    static  uint32   envPhaseTimer;   // Time elapsed in envelope phase (ms)
    static  fixed_t  sustainLevel;    // Envelope sustain level (0 ~ 1.000)
    static  fixed_t  timeConstant;    // 20% of decay or release time (ms)
    static  fixed_t  ampldDelta;      // Step change in Env Ampld in 1ms
    static  fixed_t  ampldMaximum;    // Peak value of Envelope Ampld

    if (m_TriggerAttack2)
    {
        m_TriggerAttack2 = 0;
        m_TriggerRelease2 = 0;
        envPhaseTimer = 0;
        sustainLevel = IntToFixedPt((int) g_Patch.Env2SustainLevel) / 100; 
        ampldMaximum = FIXED_MAX_LEVEL;  // for Peak-Hold phase
        ampldDelta = ampldMaximum / 10;  // ENV2 attack time = 10ms (fixed)
        EnvSegment = ENV_ATTACK;
    }

    if (m_TriggerRelease2)
    {
        m_TriggerRelease2 = 0;
        timeConstant = g_Patch.Env2DecayTime / 5;  // Release time == Decay time
        envPhaseTimer = 0;
        EnvSegment = ENV_RELEASE;
    }

    switch (EnvSegment)
    {
    case ENV_IDLE:          // Idle - zero output level
    {
        m_ENV2_Output = 0;
        break;
    }
    case ENV_ATTACK:        // Attack - linear ramp up to peak
    {
        if (m_ENV2_Output < ampldMaximum) m_ENV2_Output += ampldDelta;
        if (++envPhaseTimer >= 10)  // attack time (10ms) ended
        {
            m_ENV2_Output = ampldMaximum;
            envPhaseTimer = 0;
            EnvSegment = ENV_PEAK_HOLD;
        }
        break;
    }
    case ENV_PEAK_HOLD:     // Peak Hold - constant output level (0.9999)
    {
        if (++envPhaseTimer >= 20)  // ENV2 Peak-Hold time = 20ms (fixed)
        {
            timeConstant = g_Patch.Env2DecayTime / 5;  // for Decay phase
            envPhaseTimer = 0;
            EnvSegment = ENV_DECAY;
        }
        break;
    }
    case ENV_DECAY:         // Decay - exponential ramp down to sustain level
    {
        ampldDelta = (m_ENV2_Output - sustainLevel) / timeConstant;  // step in 1ms
        if (ampldDelta == 0)  ampldDelta = FIXED_MIN_LEVEL;
        if (m_ENV2_Output >= (sustainLevel + ampldDelta))  m_ENV2_Output -= ampldDelta;
        // Allow 10 x time-constant for decay phase to complete
        if (++envPhaseTimer >= (g_Patch.Env2DecayTime * 2))  EnvSegment = ENV_SUSTAIN;
        break;
    }
    case ENV_SUSTAIN:       // Sustain constant level -- waiting for m_TriggerRelease
    {
        break;
    }
    case ENV_RELEASE:       // Release - exponential ramp down to zero level
    {
        // timeConstant and envPhaseTimer are set by the trigger condition, above.
        ampldDelta = m_ENV2_Output / timeConstant;
        if (ampldDelta == 0)  ampldDelta = FIXED_MIN_LEVEL;
        if (m_ENV2_Output >= ampldDelta)  m_ENV2_Output -= ampldDelta;
        // Allow 10 x time-constant for release phase to complete
        if (++envPhaseTimer >= (g_Patch.Env2DecayTime * 2))  EnvSegment = ENV_IDLE;
        break;
    }
    }  // end switch
}


/*
 * Function:  ContourGenerator()
 *
 * Overview:  Routine called by the Synth Process at 1ms intervals.
 *            All segments of the Contour shape are linear time-varying.
 *
 * Output:    (fixed_t) m_ContourOutput = output signal, normalized (0..+1.00)
 */
void  ContourGenerator()
{
    static  short    contourSegment;  // Contour envelope segment (aka phase)
    static  uint32   segmentTimer;    // Time elapsed in active phase (ms)
    static  fixed_t  outputDelta;     // Step change in output level per millisecond
    static  fixed_t  startLevel;      // Output level at start of contour (0..+1.0)
    static  fixed_t  holdLevel;       // Output level maintained at end of ramp (0..+1.0)

    if (m_TriggerContour)  // Note-On event
    {
        m_TriggerContour = 0;
        startLevel = IntToFixedPt(g_Patch.ContourStartLevel) / 100;
        holdLevel = IntToFixedPt(g_Patch.ContourHoldLevel) / 100;
        m_ContourOutput = startLevel;
        outputDelta = (holdLevel - startLevel) / g_Patch.ContourRampTime;
        segmentTimer = 0;
        contourSegment = CONTOUR_DELAY;
    }
    
    if (m_TriggerReset)  // Note-Off event
    {
        m_TriggerReset = 0;
        segmentTimer = 0;
        contourSegment = CONTOUR_IDLE;
    }

    switch (contourSegment)
    {
    case CONTOUR_IDLE:  // Waiting for trigger signal
    {
        break;
    }
    case CONTOUR_DELAY:  // Delay before ramp up/down segment
    {
        if (++segmentTimer >= g_Patch.ContourDelayTime)  // Delay segment ended
        {
            segmentTimer = 0;
            contourSegment = CONTOUR_RAMP;
        }
        break;
    }
    case CONTOUR_RAMP:  // Linear ramp up/down from Start to Hold level
    {
        if (++segmentTimer >= g_Patch.ContourRampTime)  // Ramp segment ended
            contourSegment = CONTOUR_HOLD;
        else  m_ContourOutput += outputDelta;
        break;
    }
    case CONTOUR_HOLD:  // Hold constant level - waiting for Note-Off event to reset
    {
        m_ContourOutput = holdLevel;
        break;
    }
    };  // end switch
}


/*
 * Function:  AudioLevelController()
 *
 * Overview:  This routine is called by the Synth Process at 1ms intervals.
 *            The output level is controlled (i.e. varied) by one of a choice of options
 *            as determined usually by the patch parameter g_Patch.OutputAmpldCtrl, but
 *            which may be overridden by the config param g_Config.AudioAmpldCtrlMode.
 *
 * Output:    (fixed_t) v_OutputLevel : normalized output level (range 0..+1.000)
 *            The output variable is used by the audio ISR to control the audio ampld,
 *            except for the reverberated signal which may continue to sound.
 */
void   AudioLevelController()
{
    static  fixed_t  outputAmpld;       // Audio output level, normalized
    static  fixed_t  smoothExprnLevel;  // Expression level, normalized, smoothed
    volatile  fixed_t  outputLevel;     // immune to corruption by ISR
    fixed_t  exprnLevel;
    uint8   controlSource = g_Patch.AmpControlMode;  // default
    
    // Check for global (config) override of patch parameter 
    if (g_Config.AudioAmpldCtrlMode == AUDIO_CTRL_CONST) 
        controlSource = AMPLD_CTRL_CONST_MAX;
    else if (g_Config.AudioAmpldCtrlMode == AUDIO_CTRL_ENV1_VELO) 
        controlSource = AMPLD_CTRL_ENV1_VELO;
    else if (g_Config.AudioAmpldCtrlMode == AUDIO_CTRL_EXPRESS) 
        controlSource = AMPLD_CTRL_EXPRESS;
    else  controlSource = g_Patch.AmpControlMode;

    if (controlSource == AMPLD_CTRL_CONST_LOW)  // mode 1
    {
        if (m_NoteOn)  outputAmpld = FIXED_MAX_LEVEL / 2;
        else  outputAmpld = 0;  // Mute when note terminated
    } 
    else if (controlSource == AMPLD_CTRL_ENV1_VELO)  // mode 2
    {
        if (g_CVcontrolMode && g_Config.CV3_is_Velocity)
          m_KeyVelocity = m_ExpressionLevel;  // CV3 (EXPRN) input controls ampld
        outputAmpld = MultiplyFixed(m_ENV1_Output, m_KeyVelocity);
    }
    else if (controlSource == AMPLD_CTRL_EXPRESS)  // mode 3
    {
//      if (m_ENV1_Output)  exprnLevel = m_ExpressionLevel;
//      else  exprnLevel = 0;  // Mute when ENV1 release phase ends (option 1), or...
        exprnLevel = m_ExpressionLevel;  // ... let MIDI controller determine the level
        
        // Apply IIR smoothing filter to eliminate abrupt step changes (K = 1/16)
        smoothExprnLevel -= smoothExprnLevel >> 4;  // divide by 16
        smoothExprnLevel += exprnLevel >> 4;
        outputAmpld = smoothExprnLevel;
    }
    else  // controlSource == AMPLD_CTRL_CONST_MAX   // mode 0
    {
//      if (m_NoteOn)  outputAmpld = FIXED_MAX_LEVEL;  
//      else  outputAmpld = 0;  // Mute when note terminated (option 1), or...
        outputAmpld = FIXED_MAX_LEVEL;  // ... sound the note indefinitely
    }

    if (outputAmpld > FIXED_MAX_LEVEL)  outputAmpld = FIXED_MAX_LEVEL;

    outputLevel = FractionPart(outputAmpld, 10);  // unit = 1/1024, range 0..1023
    v_OutputLevel = outputLevel;
      
    // Convert limiter level (%) to fixed-point normalized value for ISR
    if (g_Patch.LimiterLevelPc != 0)   // Limiter enabled...
        v_LimiterLevelPos = IntToFixedPt(g_Patch.LimiterLevelPc) / 100;
    else  // Limiter disabled...
        v_LimiterLevelPos = MAX_CLIPPING_LEVEL;  // maximum allowed level
		
    v_LimiterLevelNeg = 0 - v_LimiterLevelPos;
}


/*
 * Function:     Synth LFO implementation.
 *
 * Called by SynthProcess() at 1ms intervals, this function generates a sinusoidal
 * waveform in real time.  LFO frequency is a patch parameter, g_Patch.LFO_Freq_x10,
 * an unsigned 8-bit value = LFO freq * 10 Hz, range 1..250 => 0.1 to 25 Hz.
 *
 * See also OscFreqModulation() function which updates the input variable m_LFO_Step.
 * Effective sample rate (Fs) is 1000 Hz.
 *
 * Input data:   m_LFO_Step = LFO "phase step" which determines the LFO frequency
 * Output data:  m_LFO_output = normalized bipolar fixed-point value (0..+|-1.0)
 */
void   LowFrequencyOscillator()
{
    static  int32  oscAngleLFO;  // 24:8 bit fixed-point format
    int  waveIdx;

    waveIdx = oscAngleLFO >> 8;  // integer part of oscAngleLFO
    m_LFO_output = (fixed_t) g_sine_wave[waveIdx] << 5;  // normalized sample
    oscAngleLFO += m_LFO_Step;
    if (oscAngleLFO >= (WAVE_TABLE_SIZE << 8))  
        oscAngleLFO -= (WAVE_TABLE_SIZE << 8);
}


/*
 * Function:     Vibrato Ramp Generator implementation.
 *
 * Called by the SynthProcess() at 5ms intervals, this function generates a linear ramp.
 *
 * The vibrato (LFO) delayed ramp is triggered by a Note-On event. 
 * If a Legato note change occurs, vibrato is stopped (fast ramp down) and the ramp delay
 * is re-started, so that vibrato will ramp up again after the delay.
 *
 * The delay and ramp-up times are both set by the patch parameter, g_Patch.LFO_RampTime,
 * so the delay time value is the same as the ramp-up time.  This works well enough.
 */
void   VibratoRampGenerator()
{
    static  short   rampState = 0;
    static  uint32  rampTimer_ms;
    static  fixed_t rampStep;  // Step chnage in output per 5 ms

    if (g_Patch.LFO_RampTime == 0)  // ramp disabled
    {
      m_RampOutput = FIXED_MAX_LEVEL;
      return;
    }

    // Check for Note-Off or Note-Change event while ramp is progressing
    if (rampState != 3 && (!m_NoteOn || m_LegatoNoteChange))
    {
        rampStep = IntToFixedPt(5) / 100;  // ramp down in 100ms
        rampState = 3;
    }

    if (rampState == 0)  // Idle - waiting for Note-On
    {
        if (m_NoteOn)
        {
            m_RampOutput = 0;
            rampTimer_ms = 0;  // start ramp delay timer
            rampState = 1;
        }
    }
    else if (rampState == 1)  // Delaying before ramp-up begins
    {
        if (rampTimer_ms >= g_Patch.LFO_RampTime)  
        {
            rampStep = IntToFixedPt(5) / (int) g_Patch.LFO_RampTime;
            rampState = 2;
        }
        rampTimer_ms += 5;
    }
    else if (rampState == 2)  // Ramping up - hold at max. level (1.00)
    {
        if (m_RampOutput < FIXED_MAX_LEVEL)  m_RampOutput += rampStep;
        if (m_RampOutput > FIXED_MAX_LEVEL)  m_RampOutput = FIXED_MAX_LEVEL;
    }
    else if (rampState == 3)  // Ramping down fast (fixed 100ms time)
    {
        if (m_RampOutput > 0)  m_RampOutput -= rampStep;
        if (m_RampOutput < 0)  m_RampOutput = 0;
        
        if (m_RampOutput < (IntToFixedPt(1) / 100))  // output is below 0.01
        {
            // If a legato note change has occurred, re-start the ramp delay
            if (m_LegatoNoteChange)  { m_LegatoNoteChange = 0;  rampState = 1; }
            else  rampState = 0;
            rampTimer_ms = 0;
        }
    }
    else  rampState = 0;
}


/*
 * Function:     Oscillator Frequency Modulation  (Pitch-bend or vibrato)
 *
 * Called by SynthProcess() at 5ms intervals, this function modulates the pitch of
 * the wave-table oscillators according to a "deviation factor" (freqDevn) which is
 * continuously updated while a note is in progress. The function also modifies the
 * oscillator frequencies according to the respective de-tune patch parameters.
 * This function also applies the "master tune" configuration param.
 * 
 * The linear m_PitchBendFactor is transformed into a multiplier in the range 0.5 ~ 2.0.
 * Centre (zero) m_PitchBendFactor value gives a multplier value of 1.00.
 *
 * Note:  This function is used for low frequency modulation, up to about 25Hz,
 *        intended for Pitch Bend OR Vibrato (mutually exclusive).
 */
void   OscFreqModulation()
{
    fixed_t detuneNorm;      // osc de-tune factor (0.5 ~ 2.0 octave)
    fixed_t LFO_scaled;      // normalized, bipolar (range 0..+/-1.0)
    fixed_t modnLevel;       // normalized, unipolar (range 0..+1.0)
    fixed_t freqDevn;        // deviation from median freq. (x0.5 .. x2.0)
    int32   oscStep;         // temporary for calc'n (16:16 bit fix-pt)
    int32   oscFreqLFO;      // 24:8 bit fixed-point format (8-bit fraction)
    short   osc, cents;
    
    oscFreqLFO = (((int) g_Patch.LFO_Freq_x10) << 8) / 10;  // 24:8 bit fixed-pt
    m_LFO_Step = (oscFreqLFO * WAVE_TABLE_SIZE) / 1000;  // LFO Fs = 1000Hz  
    
    if (g_Config.VibratoCtrlMode == VIBRATO_BY_MODN_CC)  // Use Mod Lever
        modnLevel = (m_ModulationLevel * g_Config.PitchBendRange) / 12;  // 1 octave max.
    
    if (g_Config.VibratoCtrlMode == VIBRATO_AUTOMATIC)  // Use LFO with ramp generator
        modnLevel = (m_RampOutput * g_Patch.LFO_FM_Depth) / 1200;

    if (g_Config.VibratoCtrlMode == VIBRATO_BY_CV_AUXIN)  // Use LFO without ramp
        modnLevel = (IntToFixedPt(1) * g_Patch.LFO_FM_Depth) / 1200;

    if (g_Config.VibratoCtrlMode != 0)  // Vibrato has priority over pitch bend
    {
        LFO_scaled = MultiplyFixed(m_LFO_output, modnLevel); 
        freqDevn = Base2Exp(LFO_scaled);   // range 0.5 ~ 2.0.
    }
    else if (g_Config.PitchBendMode != 0)  // pitch bend enabled
        freqDevn = Base2Exp(m_PitchBendFactor);  // max. 1 octave
    else  freqDevn = IntToFixedPt(1);  // No FM -- default
   
    for (osc = 0;  osc < 6;  osc++)
    { 
        cents = g_Patch.OscDetune[osc] + g_Config.MasterTuneOffset;  // signed
        detuneNorm = Base2Exp((IntToFixedPt(1) * cents) / 1200);
        m_OscStepDetune[osc] = MultiplyFixed(m_OscStepInit[osc], detuneNorm);
        oscStep = MultiplyFixed(m_OscStepDetune[osc], freqDevn);  // Apply FM
        v_OscStep[osc] = oscStep;  // update osc frequency
    }
}


/*
 * Oscillator Ampld Modulation and Mixer Level Control routine.
 *
 * Called by the SynthProcess() routine at 5ms intervals, this function determines the 
 * amplitude modulation factors (multipliers) for the 6 oscillators, according to their
 * respective control sources (as specified by an array of 6 patch parameters).
 * The actual modulator operation is performed by the audio ISR, using output data.
 *
 * The function also determines the mixer input levels for the 6 oscillators, according 
 * to 6 patch parameters in the array g_Patch.MixerInputlevel[].  
 * The actual mixer operation is performed by the audio ISR, using the output data.
 *
 * Input data:   g_Patch.OscAmpldModSource[osc],  g_Patch.MixerInputlevel[osc],
 *               and  g_Patch.MixerOutputGain
 *
 * Output data:  v_OscAmpldModn[osc],  v_MixerLevel[osc]  (accessed by the audio ISR)
 *               (These are scalar multipliers, range 0..1023)
 */
void  OscAmpldModulation()
{
    short  osc, step;
    
    fixed_t  LFO_scaled = (m_LFO_output * g_Patch.LFO_AM_Depth) / 200;  // FS = +/-0.5
    fixed_t  LFO_AM_bias = IntToFixedPt(1) - IntToFixedPt(g_Patch.LFO_AM_Depth) / 200;
    
    for (osc = 0;  osc < 6;  osc++)
    {
        // Determine Ampld Modulation factor for each oscillator
        if (g_Patch.OscAmpldModSource[osc] == OSC_MODN_SOURCE_CONT_POS)
            v_OscAmpldModn[osc] = m_ContourOutput >> 10;  // 0..1024
        else if (g_Patch.OscAmpldModSource[osc] == OSC_MODN_SOURCE_CONT_NEG)
            v_OscAmpldModn[osc] = 1024 - (m_ContourOutput >> 10);  // 1024..0
        else if (g_Patch.OscAmpldModSource[osc] == OSC_MODN_SOURCE_ENV2)
            v_OscAmpldModn[osc] = m_ENV2_Output >> 10;  // 0..1024
        else if (g_Patch.OscAmpldModSource[osc] == OSC_MODN_SOURCE_MODN)
            v_OscAmpldModn[osc] = m_ModulationLevel >> 10;  // 0..1024
        else if (g_Patch.OscAmpldModSource[osc] == OSC_MODN_SOURCE_EXPR_POS)
            v_OscAmpldModn[osc] = m_ExpressionLevel >> 10;  // 0..1024
        else if (g_Patch.OscAmpldModSource[osc] == OSC_MODN_SOURCE_EXPR_NEG)
            v_OscAmpldModn[osc] = 1024 - (m_ExpressionLevel >> 10);  // 1024..0
        else if (g_Patch.OscAmpldModSource[osc] == OSC_MODN_SOURCE_LFO)
            v_OscAmpldModn[osc] = (LFO_scaled + LFO_AM_bias) >> 10;  // 0..1024
        else if (g_Patch.OscAmpldModSource[osc] == OSC_MODN_SOURCE_VELO_POS)
            v_OscAmpldModn[osc] = m_KeyVelocity >> 10;  // 0..1024
        else if (g_Patch.OscAmpldModSource[osc] == OSC_MODN_SOURCE_VELO_NEG)
            v_OscAmpldModn[osc] = 1024 - (m_KeyVelocity >> 10);  // 1024..0
        else  
            v_OscAmpldModn[osc] = 1000;  // Fixed, maximum level
        
        if (v_OscAmpldModn[osc] > 1000)  v_OscAmpldModn[osc] = 1000;  // limit to 1000
        
        // Update mixer input level for each oscillator...
        if (m_OscMuted[osc])  v_MixerLevel[osc] = 0;  
        else  
        {
            step = g_Patch.MixerInputStep[osc];  // 0..16
            v_MixerLevel[osc] = g_AmpldLevelLogScale_x1000[step];  // 0..1000
        }
    }
    
    // Set Mixer Output Gain control according to patch param.
    v_MixerOutGain = (g_Patch.MixerOutGain_x10 << 7) / 10;  // 0..1280
}


/*`````````````````````````````````````````````````````````````````````````````````````````````````
 * Function:     Timer-Counter-3 interrupt service routine (Audio ISR)
 *
 * The ISR performs audio DSP synthesis computations which need to be executed at the
 * sample rate, defined by SAMPLE_RATE_HZ (typ. 32 or 40 kHz).
 *
 * Signal (sample) computations use 32-bit [12:20] fixed-point arithmetic, except
 * that wave-table samples are stored as 16-bit signed integers. Wave-table samples are
 * converted to normalized fixed-point (20-bit fraction) by shifting left 5 bit places.
 *
 * The Wave-table Oscillator algorithm uses lower precision  [16:16] fixed-point variables
 * for phase angle to avoid arithmetic overflow which would occur using the [12:20] format.
 */
void  TC3_Handler(void)
{
    static   int   rvbIndex;        // index into ReverbDelayLine[]
    static   fixed_t  reverbPrev;   // previous output from reverb delay line
    int      osc;                   // oscillator number (0..5)
    int      idx;                   // index into wave-table
    fixed_t  oscSample;             // wave-table sample (normalized fixed_pt)
    fixed_t  mixerOut = 0;          // output from mixer
    fixed_t  attenOut = 0;          // output from variable-gain attenuator
    fixed_t  reverbOut;             // output from reverb delay line
    fixed_t  reverbLPF;             // output from reverb filter
    fixed_t  finalOutput = 0;       // output to audio DAC
    uint16   spiDACdata;            // SPI DAC register data

    digitalWrite(TESTPOINT1, HIGH);  // pin pulses high during ISR execution

    if (v_SynthEnable)
    {
        for (osc = 0;  osc < 6;  osc++)
        {
            // Wave-table oscillator algorithm
            idx = v_OscAngle[osc] >> 16;  // integer part of v_OscAngle
            oscSample = (fixed_t) g_sine_wave[idx] << 5;  // normalize
            v_OscAngle[osc] += v_OscStep[osc];
            if (v_OscAngle[osc] >= (WAVE_TABLE_SIZE << 16))
                v_OscAngle[osc] -= (WAVE_TABLE_SIZE << 16);
            
            // Apply oscillator amplitude modulation
            oscSample = (oscSample * v_OscAmpldModn[osc]) >> 10; // scalar multiply
            
            // Feed oscSample into mixer, scaled by the respective input setting
            mixerOut += (oscSample * v_MixerLevel[osc]) >> 10;  // scalar multiply
        }

        // Apply Mixer Gain parameter to optimize output level 
        mixerOut = (mixerOut * v_MixerOutGain) >> 7;  // (mixerOut * v_MixerOutGain) / 128
        
        // Apply Ampld Limiter
        if (mixerOut > v_LimiterLevelPos)  mixerOut = v_LimiterLevelPos;
        if (mixerOut < v_LimiterLevelNeg)  mixerOut = v_LimiterLevelNeg;

        // Output attenuator -- Apply envelope, velocity, expression, etc.
        attenOut = (mixerOut * v_OutputLevel) >> 10;  // scalar multiply
        
        // Reverberation effect (Courtesy of Dan Mitchell, ref. "BasicSynth")
        if (m_RvbMix)
        {
            reverbOut = MultiplyFixed(ReverbDelayLine[rvbIndex], m_RvbDecay);
            reverbLPF = (reverbOut + reverbPrev) >> 1;  // simple low-pass filter
            reverbPrev = reverbOut;
            ReverbDelayLine[rvbIndex] = ((attenOut * m_RvbAtten) >> 7) + reverbLPF;
            if (++rvbIndex >= m_RvbDelayLen)  rvbIndex = 0;  // wrap
            // Add reverb output to dry signal according to reverb mix setting...
            finalOutput = (attenOut * (128 - m_RvbMix)) >> 7;  // Dry portion
            finalOutput += (reverbOut * m_RvbMix) >> 7;   // Wet portion
        }
        else  finalOutput = attenOut;
    }
 
#if USE_SPI_DAC_FOR_AUDIO  
    spiDACdata = (uint16)(2048 + (int)(finalOutput >> 9));  // 12 LS bits
    digitalWrite(SPI_DAC_CS, LOW);
    SPI.transfer16(spiDACdata | 0x3000 );
    digitalWrite(SPI_DAC_CS, HIGH);
#else  
    analogWrite(A0, 512 + (int)(finalOutput >> 11));  // use on-chip DAC (10 bits)
#endif

    digitalWrite(TESTPOINT1, LOW);
    TC3->COUNT16.INTFLAG.bit.MC0 = 1;  // clear the IRQ
}


/*`````````````````````````````````````````````````````````````````````````````````````````````````
 * Function:    Base-2 exponential transfer function using look-up table with interpolation.
 *              Resolution (precision) is better than +/-0.0001
 *
 * Entry arg:   (fixed_t) xval = fixed-point real number, range -1.0 to +1.0
 *
 * Returned:    (fixed_t) yval = 2 ** xval;  range 0.5000 to 2.0000
 *
 */
fixed_t  Base2Exp(fixed_t xval)
{
    int   ixval;        // 13-bit integer representing x-axis coordinate
    int   idx;          // 10 MS bits of ixval = array index into LUT, g_base2exp[]
    int   irem3;        // 3 LS bits of ixval for interpolation
    int32 ydelta;       // change in y value between 2 adjacent points in LUT
    int32 yval;         // y value (from LUT) with interpolation

    if (xval < IntToFixedPt(-1) || xval > IntToFixedPt(1))  xval = 0;

    // Convert real xval (x-coord) to positive 13-bit integer in the range 0 ~ 8K
    ixval = FractionPart((xval + IntToFixedPt(1)) / 2, 13); 
    idx = ixval >> 3;  
    irem3 = ixval & 7; 

    if (xval == IntToFixedPt(1))
        yval = 2 << 14;  // maximum value in 18:14 bit format
    else
    {
        yval = (int32) g_base2exp[idx];
        ydelta = (((int32) g_base2exp[idx+1] - yval) * irem3) / 8;
        yval = yval + ydelta;
    }

    return  (fixed_t)(yval << 6);   // convert to 12:20 fixed-pt format
}


// Lookup table to transform linear variable to base-2 exponential.
// Index value range 0..1024 (integer) represents linear axis range -1.0 ~ +1.0.
// Lookup value range is 0.5 to 2.0 (fixed point).  Centre (zero) value is 1.00.
//
// <!>  g_base2exp[] values are in 18:14 bit fixed-point format.
//      Shift left 6 bit places to convert to 12:20 fixed-point.
//      ````````````````````````````````````````````````````````
// For higher precision, where required, use the function: Base2Exp()
//
const  uint16  g_base2exp[] =
{
    0x2000, 0x200B, 0x2016, 0x2021, 0x202C, 0x2037, 0x2042, 0x204E,
    0x2059, 0x2064, 0x206F, 0x207A, 0x2086, 0x2091, 0x209C, 0x20A8,
    0x20B3, 0x20BE, 0x20CA, 0x20D5, 0x20E0, 0x20EC, 0x20F7, 0x2103,
    0x210E, 0x211A, 0x2125, 0x2130, 0x213C, 0x2148, 0x2153, 0x215F,
    0x216A, 0x2176, 0x2181, 0x218D, 0x2199, 0x21A4, 0x21B0, 0x21BC,
    0x21C7, 0x21D3, 0x21DF, 0x21EB, 0x21F6, 0x2202, 0x220E, 0x221A,
    0x2226, 0x2231, 0x223D, 0x2249, 0x2255, 0x2261, 0x226D, 0x2279,
    0x2285, 0x2291, 0x229D, 0x22A9, 0x22B5, 0x22C1, 0x22CD, 0x22D9,
    0x22E5, 0x22F1, 0x22FD, 0x2309, 0x2315, 0x2322, 0x232E, 0x233A,
    0x2346, 0x2352, 0x235F, 0x236B, 0x2377, 0x2384, 0x2390, 0x239C,
    0x23A9, 0x23B5, 0x23C1, 0x23CE, 0x23DA, 0x23E7, 0x23F3, 0x23FF,
    0x240C, 0x2418, 0x2425, 0x2432, 0x243E, 0x244B, 0x2457, 0x2464,
    0x2470, 0x247D, 0x248A, 0x2496, 0x24A3, 0x24B0, 0x24BD, 0x24C9,
    0x24D6, 0x24E3, 0x24F0, 0x24FC, 0x2509, 0x2516, 0x2523, 0x2530,
    0x253D, 0x254A, 0x2557, 0x2564, 0x2570, 0x257D, 0x258A, 0x2598,
    0x25A5, 0x25B2, 0x25BF, 0x25CC, 0x25D9, 0x25E6, 0x25F3, 0x2600,
    0x260D, 0x261B, 0x2628, 0x2635, 0x2642, 0x2650, 0x265D, 0x266A,
    0x2678, 0x2685, 0x2692, 0x26A0, 0x26AD, 0x26BA, 0x26C8, 0x26D5,
    0x26E3, 0x26F0, 0x26FE, 0x270B, 0x2719, 0x2726, 0x2734, 0x2742,
    0x274F, 0x275D, 0x276A, 0x2778, 0x2786, 0x2794, 0x27A1, 0x27AF,
    0x27BD, 0x27CB, 0x27D8, 0x27E6, 0x27F4, 0x2802, 0x2810, 0x281E,
    0x282C, 0x283A, 0x2847, 0x2855, 0x2863, 0x2871, 0x287F, 0x288E,
    0x289C, 0x28AA, 0x28B8, 0x28C6, 0x28D4, 0x28E2, 0x28F0, 0x28FF,
    0x290D, 0x291B, 0x2929, 0x2938, 0x2946, 0x2954, 0x2962, 0x2971,
    0x297F, 0x298E, 0x299C, 0x29AA, 0x29B9, 0x29C7, 0x29D6, 0x29E4,
    0x29F3, 0x2A01, 0x2A10, 0x2A1F, 0x2A2D, 0x2A3C, 0x2A4A, 0x2A59,
    0x2A68, 0x2A77, 0x2A85, 0x2A94, 0x2AA3, 0x2AB2, 0x2AC0, 0x2ACF,
    0x2ADE, 0x2AED, 0x2AFC, 0x2B0B, 0x2B1A, 0x2B29, 0x2B38, 0x2B47,
    0x2B56, 0x2B65, 0x2B74, 0x2B83, 0x2B92, 0x2BA1, 0x2BB0, 0x2BBF,
    0x2BCE, 0x2BDE, 0x2BED, 0x2BFC, 0x2C0B, 0x2C1B, 0x2C2A, 0x2C39,
    0x2C48, 0x2C58, 0x2C67, 0x2C77, 0x2C86, 0x2C95, 0x2CA5, 0x2CB4,
    0x2CC4, 0x2CD3, 0x2CE3, 0x2CF3, 0x2D02, 0x2D12, 0x2D21, 0x2D31,
    0x2D41, 0x2D50, 0x2D60, 0x2D70, 0x2D80, 0x2D8F, 0x2D9F, 0x2DAF,
    0x2DBF, 0x2DCF, 0x2DDF, 0x2DEF, 0x2DFE, 0x2E0E, 0x2E1E, 0x2E2E,
    0x2E3E, 0x2E4E, 0x2E5F, 0x2E6F, 0x2E7F, 0x2E8F, 0x2E9F, 0x2EAF,
    0x2EBF, 0x2ED0, 0x2EE0, 0x2EF0, 0x2F00, 0x2F11, 0x2F21, 0x2F31,
    0x2F42, 0x2F52, 0x2F62, 0x2F73, 0x2F83, 0x2F94, 0x2FA4, 0x2FB5,
    0x2FC5, 0x2FD6, 0x2FE7, 0x2FF7, 0x3008, 0x3018, 0x3029, 0x303A,
    0x304B, 0x305B, 0x306C, 0x307D, 0x308E, 0x309F, 0x30AF, 0x30C0,
    0x30D1, 0x30E2, 0x30F3, 0x3104, 0x3115, 0x3126, 0x3137, 0x3148,
    0x3159, 0x316A, 0x317C, 0x318D, 0x319E, 0x31AF, 0x31C0, 0x31D2,
    0x31E3, 0x31F4, 0x3205, 0x3217, 0x3228, 0x323A, 0x324B, 0x325C,
    0x326E, 0x327F, 0x3291, 0x32A2, 0x32B4, 0x32C6, 0x32D7, 0x32E9,
    0x32FB, 0x330C, 0x331E, 0x3330, 0x3341, 0x3353, 0x3365, 0x3377,
    0x3389, 0x339B, 0x33AC, 0x33BE, 0x33D0, 0x33E2, 0x33F4, 0x3406,
    0x3418, 0x342A, 0x343C, 0x344F, 0x3461, 0x3473, 0x3485, 0x3497,
    0x34AA, 0x34BC, 0x34CE, 0x34E0, 0x34F3, 0x3505, 0x3517, 0x352A,
    0x353C, 0x354F, 0x3561, 0x3574, 0x3586, 0x3599, 0x35AB, 0x35BE,
    0x35D1, 0x35E3, 0x35F6, 0x3609, 0x361C, 0x362E, 0x3641, 0x3654,
    0x3667, 0x367A, 0x368D, 0x369F, 0x36B2, 0x36C5, 0x36D8, 0x36EB,
    0x36FE, 0x3712, 0x3725, 0x3738, 0x374B, 0x375E, 0x3771, 0x3784,
    0x3798, 0x37AB, 0x37BE, 0x37D2, 0x37E5, 0x37F8, 0x380C, 0x381F,
    0x3833, 0x3846, 0x385A, 0x386D, 0x3881, 0x3894, 0x38A8, 0x38BC,
    0x38CF, 0x38E3, 0x38F7, 0x390B, 0x391E, 0x3932, 0x3946, 0x395A,
    0x396E, 0x3982, 0x3996, 0x39AA, 0x39BE, 0x39D2, 0x39E6, 0x39FA,
    0x3A0E, 0x3A22, 0x3A36, 0x3A4A, 0x3A5F, 0x3A73, 0x3A87, 0x3A9B,
    0x3AB0, 0x3AC4, 0x3AD8, 0x3AED, 0x3B01, 0x3B16, 0x3B2A, 0x3B3F,
    0x3B53, 0x3B68, 0x3B7C, 0x3B91, 0x3BA6, 0x3BBA, 0x3BCF, 0x3BE4,
    0x3BF9, 0x3C0D, 0x3C22, 0x3C37, 0x3C4C, 0x3C61, 0x3C76, 0x3C8B,
    0x3CA0, 0x3CB5, 0x3CCA, 0x3CDF, 0x3CF4, 0x3D09, 0x3D1E, 0x3D34,
    0x3D49, 0x3D5E, 0x3D73, 0x3D89, 0x3D9E, 0x3DB3, 0x3DC9, 0x3DDE,
    0x3DF4, 0x3E09, 0x3E1F, 0x3E34, 0x3E4A, 0x3E5F, 0x3E75, 0x3E8B,
    0x3EA0, 0x3EB6, 0x3ECC, 0x3EE2, 0x3EF7, 0x3F0D, 0x3F23, 0x3F39,
    0x3F4F, 0x3F65, 0x3F7B, 0x3F91, 0x3FA7, 0x3FBD, 0x3FD3, 0x3FE9,
    0x4000, 0x4016, 0x402C, 0x4042, 0x4058, 0x406F, 0x4085, 0x409C,
    0x40B2, 0x40C8, 0x40DF, 0x40F5, 0x410C, 0x4122, 0x4139, 0x4150,
    0x4166, 0x417D, 0x4194, 0x41AA, 0x41C1, 0x41D8, 0x41EF, 0x4206,
    0x421D, 0x4234, 0x424A, 0x4261, 0x4278, 0x4290, 0x42A7, 0x42BE,
    0x42D5, 0x42EC, 0x4303, 0x431B, 0x4332, 0x4349, 0x4360, 0x4378,
    0x438F, 0x43A7, 0x43BE, 0x43D6, 0x43ED, 0x4405, 0x441C, 0x4434,
    0x444C, 0x4463, 0x447B, 0x4493, 0x44AA, 0x44C2, 0x44DA, 0x44F2,
    0x450A, 0x4522, 0x453A, 0x4552, 0x456A, 0x4582, 0x459A, 0x45B2,
    0x45CA, 0x45E3, 0x45FB, 0x4613, 0x462B, 0x4644, 0x465C, 0x4675,
    0x468D, 0x46A5, 0x46BE, 0x46D6, 0x46EF, 0x4708, 0x4720, 0x4739,
    0x4752, 0x476A, 0x4783, 0x479C, 0x47B5, 0x47CE, 0x47E7, 0x47FF,
    0x4818, 0x4831, 0x484A, 0x4864, 0x487D, 0x4896, 0x48AF, 0x48C8,
    0x48E1, 0x48FB, 0x4914, 0x492D, 0x4947, 0x4960, 0x497A, 0x4993,
    0x49AD, 0x49C6, 0x49E0, 0x49F9, 0x4A13, 0x4A2D, 0x4A46, 0x4A60,
    0x4A7A, 0x4A94, 0x4AAE, 0x4AC8, 0x4AE1, 0x4AFB, 0x4B15, 0x4B30,
    0x4B4A, 0x4B64, 0x4B7E, 0x4B98, 0x4BB2, 0x4BCC, 0x4BE7, 0x4C01,
    0x4C1B, 0x4C36, 0x4C50, 0x4C6B, 0x4C85, 0x4CA0, 0x4CBA, 0x4CD5,
    0x4CF0, 0x4D0A, 0x4D25, 0x4D40, 0x4D5B, 0x4D75, 0x4D90, 0x4DAB,
    0x4DC6, 0x4DE1, 0x4DFC, 0x4E17, 0x4E32, 0x4E4D, 0x4E69, 0x4E84,
    0x4E9F, 0x4EBA, 0x4ED5, 0x4EF1, 0x4F0C, 0x4F28, 0x4F43, 0x4F5F,
    0x4F7A, 0x4F96, 0x4FB1, 0x4FCD, 0x4FE9, 0x5004, 0x5020, 0x503C,
    0x5058, 0x5074, 0x508F, 0x50AB, 0x50C7, 0x50E3, 0x50FF, 0x511C,
    0x5138, 0x5154, 0x5170, 0x518C, 0x51A9, 0x51C5, 0x51E1, 0x51FE,
    0x521A, 0x5237, 0x5253, 0x5270, 0x528C, 0x52A9, 0x52C5, 0x52E2,
    0x52FF, 0x531C, 0x5339, 0x5355, 0x5372, 0x538F, 0x53AC, 0x53C9,
    0x53E6, 0x5403, 0x5421, 0x543E, 0x545B, 0x5478, 0x5495, 0x54B3,
    0x54D0, 0x54EE, 0x550B, 0x5529, 0x5546, 0x5564, 0x5581, 0x559F,
    0x55BD, 0x55DA, 0x55F8, 0x5616, 0x5634, 0x5652, 0x5670, 0x568E,
    0x56AC, 0x56CA, 0x56E8, 0x5706, 0x5724, 0x5742, 0x5761, 0x577F,
    0x579D, 0x57BC, 0x57DA, 0x57F9, 0x5817, 0x5836, 0x5854, 0x5873,
    0x5891, 0x58B0, 0x58CF, 0x58EE, 0x590D, 0x592B, 0x594A, 0x5969,
    0x5988, 0x59A7, 0x59C7, 0x59E6, 0x5A05, 0x5A24, 0x5A43, 0x5A63,
    0x5A82, 0x5AA1, 0x5AC1, 0x5AE0, 0x5B00, 0x5B1F, 0x5B3F, 0x5B5F,
    0x5B7E, 0x5B9E, 0x5BBE, 0x5BDE, 0x5BFD, 0x5C1D, 0x5C3D, 0x5C5D,
    0x5C7D, 0x5C9D, 0x5CBE, 0x5CDE, 0x5CFE, 0x5D1E, 0x5D3E, 0x5D5F,
    0x5D7F, 0x5DA0, 0x5DC0, 0x5DE1, 0x5E01, 0x5E22, 0x5E42, 0x5E63,
    0x5E84, 0x5EA5, 0x5EC5, 0x5EE6, 0x5F07, 0x5F28, 0x5F49, 0x5F6A,
    0x5F8B, 0x5FAC, 0x5FCE, 0x5FEF, 0x6010, 0x6031, 0x6053, 0x6074,
    0x6096, 0x60B7, 0x60D9, 0x60FA, 0x611C, 0x613E, 0x615F, 0x6181,
    0x61A3, 0x61C5, 0x61E7, 0x6209, 0x622B, 0x624D, 0x626F, 0x6291,
    0x62B3, 0x62D5, 0x62F8, 0x631A, 0x633C, 0x635F, 0x6381, 0x63A4,
    0x63C6, 0x63E9, 0x640B, 0x642E, 0x6451, 0x6474, 0x6497, 0x64B9,
    0x64DC, 0x64FF, 0x6522, 0x6545, 0x6569, 0x658C, 0x65AF, 0x65D2,
    0x65F6, 0x6619, 0x663C, 0x6660, 0x6683, 0x66A7, 0x66CA, 0x66EE,
    0x6712, 0x6736, 0x6759, 0x677D, 0x67A1, 0x67C5, 0x67E9, 0x680D,
    0x6831, 0x6855, 0x6879, 0x689E, 0x68C2, 0x68E6, 0x690B, 0x692F,
    0x6954, 0x6978, 0x699D, 0x69C1, 0x69E6, 0x6A0B, 0x6A2F, 0x6A54,
    0x6A79, 0x6A9E, 0x6AC3, 0x6AE8, 0x6B0D, 0x6B32, 0x6B57, 0x6B7D,
    0x6BA2, 0x6BC7, 0x6BED, 0x6C12, 0x6C38, 0x6C5D, 0x6C83, 0x6CA8,
    0x6CCE, 0x6CF4, 0x6D1A, 0x6D3F, 0x6D65, 0x6D8B, 0x6DB1, 0x6DD7,
    0x6DFD, 0x6E24, 0x6E4A, 0x6E70, 0x6E96, 0x6EBD, 0x6EE3, 0x6F09,
    0x6F30, 0x6F57, 0x6F7D, 0x6FA4, 0x6FCB, 0x6FF1, 0x7018, 0x703F,
    0x7066, 0x708D, 0x70B4, 0x70DB, 0x7102, 0x7129, 0x7151, 0x7178,
    0x719F, 0x71C7, 0x71EE, 0x7216, 0x723D, 0x7265, 0x728D, 0x72B4,
    0x72DC, 0x7304, 0x732C, 0x7354, 0x737C, 0x73A4, 0x73CC, 0x73F4,
    0x741C, 0x7444, 0x746D, 0x7495, 0x74BE, 0x74E6, 0x750F, 0x7537,
    0x7560, 0x7589, 0x75B1, 0x75DA, 0x7603, 0x762C, 0x7655, 0x767E,
    0x76A7, 0x76D0, 0x76F9, 0x7723, 0x774C, 0x7775, 0x779F, 0x77C8,
    0x77F2, 0x781B, 0x7845, 0x786F, 0x7899, 0x78C2, 0x78EC, 0x7916,
    0x7940, 0x796A, 0x7994, 0x79BF, 0x79E9, 0x7A13, 0x7A3D, 0x7A68,
    0x7A92, 0x7ABD, 0x7AE7, 0x7B12, 0x7B3D, 0x7B67, 0x7B92, 0x7BBD,
    0x7BE8, 0x7C13, 0x7C3E, 0x7C69, 0x7C94, 0x7CBF, 0x7CEB, 0x7D16,
    0x7D41, 0x7D6D, 0x7D98, 0x7DC4, 0x7DEF, 0x7E1B, 0x7E47, 0x7E73,
    0x7E9F, 0x7ECA, 0x7EF6, 0x7F22, 0x7F4F, 0x7F7B, 0x7FA7, 0x7FD3,
    0x8000
};


/*```````````````````````````````````````````````````````````````````````````````````````
 * Wave-table definition ...
 * Table name: g_sine_wave
 * Size: 2048 samples
 * Peak value:  +/-32000
 */
const  short  g_sine_wave[] =
{
         0,     97,    196,    293,    392,    490,    588,    686,    785,    882,
       981,   1079,   1177,   1275,   1373,   1471,   1569,   1667,   1765,   1863,
      1961,   2059,   2157,   2255,   2353,   2451,   2548,   2647,   2745,   2842,
      2940,   3038,   3135,   3233,   3331,   3428,   3526,   3624,   3721,   3819,
      3916,   4013,   4111,   4208,   4305,   4403,   4500,   4597,   4694,   4791,
      4888,   4986,   5083,   5179,   5276,   5373,   5469,   5566,   5663,   5759,
      5856,   5953,   6049,   6145,   6242,   6338,   6434,   6531,   6626,   6722,
      6818,   6915,   7010,   7106,   7202,   7297,   7393,   7488,   7583,   7679,
      7774,   7870,   7964,   8059,   8155,   8250,   8344,   8439,   8534,   8628,
      8722,   8817,   8912,   9005,   9100,   9194,   9288,   9381,   9475,   9569,
      9663,   9756,   9850,   9943,  10037,  10129,  10223,  10316,  10409,  10501,
     10594,  10687,  10779,  10872,  10963,  11056,  11148,  11240,  11332,  11423,
     11515,  11607,  11699,  11790,  11880,  11972,  12063,  12154,  12245,  12335,
     12425,  12516,  12606,  12697,  12787,  12876,  12966,  13056,  13146,  13235,
     13325,  13414,  13502,  13591,  13680,  13769,  13858,  13946,  14035,  14123,
     14210,  14298,  14386,  14474,  14561,  14649,  14736,  14823,  14910,  14997,
     15083,  15169,  15256,  15342,  15428,  15514,  15600,  15686,  15771,  15857,
     15942,  16027,  16112,  16197,  16281,  16366,  16450,  16534,  16618,  16702,
     16786,  16869,  16953,  17036,  17119,  17202,  17284,  17367,  17449,  17531,
     17613,  17695,  17777,  17858,  17940,  18021,  18102,  18183,  18263,  18344,
     18424,  18504,  18584,  18665,  18744,  18824,  18903,  18982,  19061,  19139,
     19218,  19296,  19375,  19453,  19531,  19608,  19686,  19763,  19840,  19917,
     19994,  20071,  20147,  20223,  20299,  20375,  20451,  20526,  20601,  20676,
     20750,  20826,  20900,  20974,  21048,  21122,  21196,  21269,  21342,  21416,
     21488,  21561,  21633,  21706,  21778,  21849,  21921,  21993,  22064,  22134,
     22206,  22276,  22346,  22416,  22487,  22556,  22625,  22695,  22764,  22833,
     22902,  22970,  23039,  23106,  23174,  23242,  23309,  23376,  23443,  23510,
     23577,  23643,  23708,  23775,  23840,  23906,  23970,  24036,  24100,  24165,
     24229,  24293,  24357,  24420,  24484,  24546,  24610,  24672,  24735,  24796,
     24859,  24920,  24982,  25043,  25104,  25165,  25225,  25286,  25345,  25406,
     25465,  25524,  25583,  25642,  25701,  25759,  25817,  25875,  25933,  25991,
     26047,  26104,  26161,  26217,  26274,  26330,  26385,  26441,  26496,  26550,
     26605,  26660,  26713,  26768,  26822,  26875,  26928,  26981,  27034,  27085,
     27138,  27190,  27242,  27292,  27344,  27395,  27446,  27496,  27546,  27596,
     27645,  27695,  27744,  27792,  27841,  27889,  27937,  27985,  28033,  28080,
     28126,  28173,  28219,  28266,  28312,  28357,  28403,  28448,  28493,  28537,
     28582,  28625,  28669,  28712,  28755,  28798,  28841,  28883,  28926,  28967,
     29009,  29050,  29091,  29132,  29172,  29213,  29252,  29292,  29332,  29371,
     29410,  29449,  29487,  29525,  29562,  29600,  29637,  29673,  29710,  29747,
     29783,  29819,  29854,  29889,  29924,  29958,  29993,  30027,  30061,  30094,
     30127,  30161,  30193,  30225,  30257,  30290,  30321,  30352,  30383,  30414,
     30444,  30474,  30503,  30534,  30563,  30591,  30621,  30649,  30676,  30705,
     30732,  30759,  30786,  30813,  30839,  30865,  30891,  30916,  30941,  30966,
     30991,  31015,  31040,  31063,  31086,  31109,  31132,  31155,  31177,  31199,
     31220,  31242,  31263,  31284,  31304,  31325,  31344,  31364,  31383,  31402,
     31421,  31439,  31458,  31475,  31493,  31510,  31527,  31543,  31560,  31576,
     31591,  31607,  31623,  31637,  31652,  31666,  31680,  31694,  31707,  31720,
     31733,  31746,  31757,  31769,  31781,  31792,  31803,  31814,  31824,  31834,
     31844,  31853,  31863,  31872,  31880,  31888,  31896,  31904,  31912,  31918,
     31925,  31932,  31938,  31944,  31950,  31955,  31959,  31964,  31968,  31972,
     31976,  31980,  31983,  31986,  31989,  31991,  31993,  31995,  31996,  31997,
     31998,  31998,  31999,  31998,  31998,  31997,  31996,  31995,  31993,  31991,
     31989,  31986,  31983,  31980,  31976,  31972,  31968,  31964,  31959,  31955,
     31950,  31944,  31938,  31932,  31925,  31918,  31912,  31904,  31896,  31888,
     31880,  31872,  31863,  31853,  31844,  31834,  31824,  31814,  31803,  31792,
     31781,  31769,  31757,  31746,  31733,  31720,  31707,  31694,  31680,  31666,
     31652,  31637,  31623,  31607,  31591,  31576,  31560,  31543,  31527,  31510,
     31493,  31475,  31458,  31439,  31421,  31402,  31383,  31364,  31344,  31325,
     31304,  31284,  31263,  31242,  31220,  31199,  31177,  31155,  31132,  31109,
     31086,  31063,  31040,  31015,  30991,  30966,  30941,  30916,  30891,  30865,
     30839,  30813,  30786,  30759,  30732,  30705,  30676,  30649,  30621,  30591,
     30563,  30534,  30503,  30474,  30444,  30414,  30383,  30352,  30321,  30290,
     30257,  30225,  30193,  30161,  30127,  30094,  30061,  30027,  29993,  29958,
     29924,  29889,  29854,  29819,  29783,  29747,  29710,  29673,  29637,  29600,
     29562,  29525,  29487,  29449,  29410,  29371,  29332,  29292,  29252,  29213,
     29172,  29132,  29091,  29050,  29009,  28967,  28926,  28883,  28841,  28798,
     28755,  28712,  28669,  28625,  28582,  28537,  28493,  28448,  28403,  28357,
     28312,  28266,  28219,  28173,  28126,  28080,  28033,  27985,  27937,  27889,
     27841,  27792,  27744,  27695,  27645,  27596,  27546,  27496,  27446,  27395,
     27344,  27292,  27242,  27190,  27138,  27085,  27034,  26981,  26928,  26875,
     26822,  26768,  26713,  26660,  26605,  26550,  26496,  26441,  26385,  26330,
     26274,  26217,  26161,  26104,  26047,  25991,  25933,  25875,  25817,  25759,
     25701,  25642,  25583,  25524,  25465,  25406,  25345,  25286,  25225,  25165,
     25104,  25043,  24982,  24920,  24859,  24796,  24735,  24672,  24610,  24546,
     24484,  24420,  24357,  24293,  24229,  24165,  24100,  24036,  23970,  23906,
     23840,  23775,  23708,  23643,  23577,  23510,  23443,  23376,  23309,  23242,
     23174,  23106,  23039,  22970,  22902,  22833,  22764,  22695,  22625,  22556,
     22487,  22416,  22346,  22276,  22206,  22134,  22064,  21993,  21921,  21849,
     21778,  21706,  21633,  21561,  21488,  21416,  21342,  21269,  21196,  21122,
     21048,  20974,  20900,  20826,  20750,  20676,  20601,  20526,  20451,  20375,
     20299,  20223,  20147,  20071,  19994,  19917,  19840,  19763,  19686,  19608,
     19531,  19453,  19375,  19296,  19218,  19139,  19061,  18982,  18903,  18824,
     18744,  18665,  18584,  18504,  18424,  18344,  18263,  18183,  18102,  18021,
     17940,  17858,  17777,  17695,  17613,  17531,  17449,  17367,  17284,  17202,
     17119,  17036,  16953,  16869,  16786,  16702,  16618,  16534,  16450,  16366,
     16281,  16197,  16112,  16027,  15942,  15857,  15771,  15686,  15600,  15514,
     15428,  15342,  15256,  15169,  15083,  14997,  14910,  14823,  14736,  14649,
     14561,  14474,  14386,  14298,  14210,  14123,  14035,  13946,  13858,  13769,
     13680,  13591,  13502,  13414,  13325,  13235,  13146,  13056,  12966,  12876,
     12787,  12697,  12606,  12516,  12425,  12335,  12245,  12154,  12063,  11972,
     11880,  11790,  11699,  11607,  11515,  11423,  11332,  11240,  11148,  11056,
     10963,  10872,  10779,  10687,  10594,  10501,  10409,  10316,  10223,  10129,
     10037,   9943,   9850,   9756,   9663,   9569,   9475,   9381,   9288,   9194,
      9100,   9005,   8912,   8817,   8722,   8628,   8534,   8439,   8344,   8250,
      8155,   8059,   7964,   7870,   7774,   7679,   7583,   7488,   7393,   7297,
      7202,   7106,   7010,   6915,   6818,   6722,   6626,   6531,   6434,   6338,
      6242,   6145,   6049,   5953,   5856,   5759,   5663,   5566,   5469,   5373,
      5276,   5179,   5083,   4986,   4888,   4791,   4694,   4597,   4500,   4403,
      4305,   4208,   4111,   4013,   3916,   3819,   3721,   3624,   3526,   3428,
      3331,   3233,   3135,   3038,   2940,   2842,   2745,   2647,   2548,   2451,
      2353,   2255,   2157,   2059,   1961,   1863,   1765,   1667,   1569,   1471,
      1373,   1275,   1177,   1079,    981,    882,    785,    686,    588,    490,
       392,    293,    196,     97,      0,    -97,   -196,   -293,   -392,   -490,
      -588,   -686,   -785,   -882,   -981,  -1079,  -1177,  -1275,  -1373,  -1471,
     -1569,  -1667,  -1765,  -1863,  -1961,  -2059,  -2157,  -2255,  -2353,  -2451,
     -2548,  -2647,  -2745,  -2842,  -2940,  -3038,  -3135,  -3233,  -3331,  -3428,
     -3526,  -3624,  -3721,  -3819,  -3916,  -4013,  -4111,  -4208,  -4305,  -4403,
     -4500,  -4597,  -4694,  -4791,  -4888,  -4986,  -5083,  -5179,  -5276,  -5373,
     -5469,  -5566,  -5663,  -5759,  -5856,  -5953,  -6049,  -6145,  -6242,  -6338,
     -6434,  -6531,  -6626,  -6722,  -6818,  -6915,  -7010,  -7106,  -7202,  -7297,
     -7393,  -7488,  -7583,  -7679,  -7774,  -7870,  -7964,  -8059,  -8155,  -8250,
     -8344,  -8439,  -8534,  -8628,  -8722,  -8817,  -8912,  -9005,  -9100,  -9194,
     -9288,  -9381,  -9475,  -9569,  -9663,  -9756,  -9850,  -9943, -10037, -10129,
    -10223, -10316, -10409, -10501, -10594, -10687, -10779, -10872, -10963, -11056,
    -11148, -11240, -11332, -11423, -11515, -11607, -11699, -11790, -11880, -11972,
    -12063, -12154, -12245, -12335, -12425, -12516, -12606, -12697, -12787, -12876,
    -12966, -13056, -13146, -13235, -13325, -13414, -13502, -13591, -13680, -13769,
    -13858, -13946, -14035, -14123, -14210, -14298, -14386, -14474, -14561, -14649,
    -14736, -14823, -14910, -14997, -15083, -15169, -15256, -15342, -15428, -15514,
    -15600, -15686, -15771, -15857, -15942, -16027, -16112, -16197, -16281, -16366,
    -16450, -16534, -16618, -16702, -16786, -16869, -16953, -17036, -17119, -17202,
    -17284, -17367, -17449, -17531, -17613, -17695, -17777, -17858, -17940, -18021,
    -18102, -18183, -18263, -18344, -18424, -18504, -18584, -18665, -18744, -18824,
    -18903, -18982, -19061, -19139, -19218, -19296, -19375, -19453, -19531, -19608,
    -19686, -19763, -19840, -19917, -19994, -20071, -20147, -20223, -20299, -20375,
    -20451, -20526, -20601, -20676, -20750, -20826, -20900, -20974, -21048, -21122,
    -21196, -21269, -21342, -21416, -21488, -21561, -21633, -21706, -21778, -21849,
    -21921, -21993, -22064, -22134, -22206, -22276, -22346, -22416, -22487, -22556,
    -22625, -22695, -22764, -22833, -22902, -22970, -23039, -23106, -23174, -23242,
    -23309, -23376, -23443, -23510, -23577, -23643, -23708, -23775, -23840, -23906,
    -23970, -24036, -24100, -24165, -24229, -24293, -24357, -24420, -24484, -24546,
    -24610, -24672, -24735, -24796, -24859, -24920, -24982, -25043, -25104, -25165,
    -25225, -25286, -25345, -25406, -25465, -25524, -25583, -25642, -25701, -25759,
    -25817, -25875, -25933, -25991, -26047, -26104, -26161, -26217, -26274, -26330,
    -26385, -26441, -26496, -26550, -26605, -26660, -26713, -26768, -26822, -26875,
    -26928, -26981, -27034, -27085, -27138, -27190, -27242, -27292, -27344, -27395,
    -27446, -27496, -27546, -27596, -27645, -27695, -27744, -27792, -27841, -27889,
    -27937, -27985, -28033, -28080, -28126, -28173, -28219, -28266, -28312, -28357,
    -28403, -28448, -28493, -28537, -28582, -28625, -28669, -28712, -28755, -28798,
    -28841, -28883, -28926, -28967, -29009, -29050, -29091, -29132, -29172, -29213,
    -29252, -29292, -29332, -29371, -29410, -29449, -29487, -29525, -29562, -29600,
    -29637, -29673, -29710, -29747, -29783, -29819, -29854, -29889, -29924, -29958,
    -29993, -30027, -30061, -30094, -30127, -30161, -30193, -30225, -30257, -30290,
    -30321, -30352, -30383, -30414, -30444, -30474, -30503, -30534, -30563, -30591,
    -30621, -30649, -30676, -30705, -30732, -30759, -30786, -30813, -30839, -30865,
    -30891, -30916, -30941, -30966, -30991, -31015, -31040, -31063, -31086, -31109,
    -31132, -31155, -31177, -31199, -31220, -31242, -31263, -31284, -31304, -31325,
    -31344, -31364, -31383, -31402, -31421, -31439, -31458, -31475, -31493, -31510,
    -31527, -31543, -31560, -31576, -31591, -31607, -31623, -31637, -31652, -31666,
    -31680, -31694, -31707, -31720, -31733, -31746, -31757, -31769, -31781, -31792,
    -31803, -31814, -31824, -31834, -31844, -31853, -31863, -31872, -31880, -31888,
    -31896, -31904, -31912, -31918, -31925, -31932, -31938, -31944, -31950, -31955,
    -31959, -31964, -31968, -31972, -31976, -31980, -31983, -31986, -31989, -31991,
    -31993, -31995, -31996, -31997, -31998, -31998, -31999, -31998, -31998, -31997,
    -31996, -31995, -31993, -31991, -31989, -31986, -31983, -31980, -31976, -31972,
    -31968, -31964, -31959, -31955, -31950, -31944, -31938, -31932, -31925, -31918,
    -31912, -31904, -31896, -31888, -31880, -31872, -31863, -31853, -31844, -31834,
    -31824, -31814, -31803, -31792, -31781, -31769, -31757, -31746, -31733, -31720,
    -31707, -31694, -31680, -31666, -31652, -31637, -31623, -31607, -31591, -31576,
    -31560, -31543, -31527, -31510, -31493, -31475, -31458, -31439, -31421, -31402,
    -31383, -31364, -31344, -31325, -31304, -31284, -31263, -31242, -31220, -31199,
    -31177, -31155, -31132, -31109, -31086, -31063, -31040, -31015, -30991, -30966,
    -30941, -30916, -30891, -30865, -30839, -30813, -30786, -30759, -30732, -30705,
    -30676, -30649, -30621, -30591, -30563, -30534, -30503, -30474, -30444, -30414,
    -30383, -30352, -30321, -30290, -30257, -30225, -30193, -30161, -30127, -30094,
    -30061, -30027, -29993, -29958, -29924, -29889, -29854, -29819, -29783, -29747,
    -29710, -29673, -29637, -29600, -29562, -29525, -29487, -29449, -29410, -29371,
    -29332, -29292, -29252, -29213, -29172, -29132, -29091, -29050, -29009, -28967,
    -28926, -28883, -28841, -28798, -28755, -28712, -28669, -28625, -28582, -28537,
    -28493, -28448, -28403, -28357, -28312, -28266, -28219, -28173, -28126, -28080,
    -28033, -27985, -27937, -27889, -27841, -27792, -27744, -27695, -27645, -27596,
    -27546, -27496, -27446, -27395, -27344, -27292, -27242, -27190, -27138, -27085,
    -27034, -26981, -26928, -26875, -26822, -26768, -26713, -26660, -26605, -26550,
    -26496, -26441, -26385, -26330, -26274, -26217, -26161, -26104, -26047, -25991,
    -25933, -25875, -25817, -25759, -25701, -25642, -25583, -25524, -25465, -25406,
    -25345, -25286, -25225, -25165, -25104, -25043, -24982, -24920, -24859, -24796,
    -24735, -24672, -24610, -24546, -24484, -24420, -24357, -24293, -24229, -24165,
    -24100, -24036, -23970, -23906, -23840, -23775, -23708, -23643, -23577, -23510,
    -23443, -23376, -23309, -23242, -23174, -23106, -23039, -22970, -22902, -22833,
    -22764, -22695, -22625, -22556, -22487, -22416, -22346, -22276, -22206, -22134,
    -22064, -21993, -21921, -21849, -21778, -21706, -21633, -21561, -21488, -21416,
    -21342, -21269, -21196, -21122, -21048, -20974, -20900, -20826, -20750, -20676,
    -20601, -20526, -20451, -20375, -20299, -20223, -20147, -20071, -19994, -19917,
    -19840, -19763, -19686, -19608, -19531, -19453, -19375, -19296, -19218, -19139,
    -19061, -18982, -18903, -18824, -18744, -18665, -18584, -18504, -18424, -18344,
    -18263, -18183, -18102, -18021, -17940, -17858, -17777, -17695, -17613, -17531,
    -17449, -17367, -17284, -17202, -17119, -17036, -16953, -16869, -16786, -16702,
    -16618, -16534, -16450, -16366, -16281, -16197, -16112, -16027, -15942, -15857,
    -15771, -15686, -15600, -15514, -15428, -15342, -15256, -15169, -15083, -14997,
    -14910, -14823, -14736, -14649, -14561, -14474, -14386, -14298, -14210, -14123,
    -14035, -13946, -13858, -13769, -13680, -13591, -13502, -13414, -13325, -13235,
    -13146, -13056, -12966, -12876, -12787, -12697, -12606, -12516, -12425, -12335,
    -12245, -12154, -12063, -11972, -11880, -11790, -11699, -11607, -11515, -11423,
    -11332, -11240, -11148, -11056, -10963, -10872, -10779, -10687, -10594, -10501,
    -10409, -10316, -10223, -10129, -10037,  -9943,  -9850,  -9756,  -9663,  -9569,
     -9475,  -9381,  -9288,  -9194,  -9100,  -9005,  -8912,  -8817,  -8722,  -8628,
     -8534,  -8439,  -8344,  -8250,  -8155,  -8059,  -7964,  -7870,  -7774,  -7679,
     -7583,  -7488,  -7393,  -7297,  -7202,  -7106,  -7010,  -6915,  -6818,  -6722,
     -6626,  -6531,  -6434,  -6338,  -6242,  -6145,  -6049,  -5953,  -5856,  -5759,
     -5663,  -5566,  -5469,  -5373,  -5276,  -5179,  -5083,  -4986,  -4888,  -4791,
     -4694,  -4597,  -4500,  -4403,  -4305,  -4208,  -4111,  -4013,  -3916,  -3819,
     -3721,  -3624,  -3526,  -3428,  -3331,  -3233,  -3135,  -3038,  -2940,  -2842,
     -2745,  -2647,  -2548,  -2451,  -2353,  -2255,  -2157,  -2059,  -1961,  -1863,
     -1765,  -1667,  -1569,  -1471,  -1373,  -1275,  -1177,  -1079,   -981,   -882,
      -785,   -686,   -588,   -490,   -392,   -293,   -196,    -97
};

// end of file
