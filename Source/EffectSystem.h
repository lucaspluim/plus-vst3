#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Effect types available in the visualizer
enum class EffectType
{
    BinaryFlash,    // Pure black/white binary flash (original kick effect)
    Flutter,        // Gradual grayscale flickering (current mids/highs)
    Starfield,      // 3D starfield with lightspeed (current kick effect)
    FrequencyLine,  // Frequency response line for selected frequency range
    // Future effects can be added here:
    // Particles,
    // Waveform,
    // Spectrum,
    // etc.
};

// Frequency range an effect can react to
enum class FrequencyRange
{
    SubBass,        // 20-60 Hz
    Bass,           // 60-250 Hz
    LowMids,        // 250-500 Hz
    Mids,           // 500-2000 Hz
    HighMids,       // 2000-4000 Hz
    Highs,          // 4000-8000 Hz
    VeryHighs,      // 8000-20000 Hz
    KickTransient,  // Special: 50-90 Hz transient detection
    FullSpectrum    // All frequencies
};

// Configuration for an effect instance
struct EffectConfig
{
    EffectType type = EffectType::Flutter;
    FrequencyRange frequencyRange = FrequencyRange::Mids;
    juce::Colour effectColor = juce::Colours::white;  // Color for flashes/stars

    // Effect-specific parameters
    float sensitivity = 1.0f;       // Multiplier for responsiveness
    float threshold = 0.0f;         // Minimum trigger level
    bool smoothing = true;          // Apply temporal smoothing

    EffectConfig() = default;
    EffectConfig(EffectType t, FrequencyRange fr)
        : type(t), frequencyRange(fr) {}
};

// Section identifiers for the three window areas
enum class SectionID
{
    Top,         // Top 50% - currently mids flutter
    BottomLeft,  // Bottom-left 25% - currently starfield
    BottomRight  // Bottom-right 25% - currently highs flutter
};
