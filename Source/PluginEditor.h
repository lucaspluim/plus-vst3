#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "EffectSystem.h"

class AudioVisualizerEditor : public juce::AudioProcessorEditor,
                               public juce::FileDragAndDropTarget,
                               public juce::DragAndDropContainer,
                               public juce::DragAndDropTarget,
                               public juce::Timer
{
public:
    AudioVisualizerEditor (AudioVisualizerProcessor&);
    ~AudioVisualizerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // File drag and drop
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    // Keyboard handling
    bool keyPressed (const juce::KeyPress& key) override;

    // Timer for animation
    void timerCallback() override;

    // Mouse handling for double-click to open effect picker
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void mouseDown(const juce::MouseEvent& event) override;

    // Drag and drop for effects
    bool isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDragEnter(const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDragMove(const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDragExit(const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDropped(const juce::DragAndDropTarget::SourceDetails& details) override;

private:
    AudioVisualizerProcessor& audioProcessor;

    // Visual state
    bool showLoadedMessage = false;
    int loadedMessageTimer = 0;

    juce::String statusMessage = "Drop audio file here or press 'O' to open";

    // Smoothed values for fluid animations - per section
    float smoothedTopValue = 0.0f;
    float smoothedBottomLeftValue = 0.0f;
    float smoothedBottomRightValue = 0.0f;
    static constexpr float visualSmoothingFactor = 0.7f; // Higher = smoother but slower response
    static constexpr float pauseFadeFactor = 0.98f;      // Slow fade when paused

    // Starfield effect - each section has independent state
    struct Star {
        float x, y, z;           // 3D position
        float prevX, prevY, prevZ; // Previous position for drawing lines
    };

    struct StarfieldInstance {
        std::vector<Star> stars;
        float currentSpeed = 2.0f;
        juce::Random random;

        StarfieldInstance()
        {
            stars.reserve(200);
            initStars();
        }

        void initStars();
        void update(float value, bool isBinaryMode);
        void draw(juce::Graphics& g, const juce::Rectangle<int>& bounds, float centerX, float centerY, bool lightMode, juce::Colour starColor);
    };

    // Separate starfield instance for each section
    StarfieldInstance topStarfield;
    StarfieldInstance bottomLeftStarfield;
    StarfieldInstance bottomRightStarfield;

    // Effect system
    EffectConfig topEffect { EffectType::FrequencyLine, FrequencyRange::Mids };
    EffectConfig bottomLeftEffect { EffectType::Starfield, FrequencyRange::KickTransient };
    EffectConfig bottomRightEffect { EffectType::Flutter, FrequencyRange::Highs };

    // Frequency line smoothing buffers (temporal smoothing across frames)
    std::vector<float> topSpectrumSmooth;
    std::vector<float> bottomLeftSpectrumSmooth;
    std::vector<float> bottomRightSpectrumSmooth;

    // Adaptive normalization - smoothed peak values for each panel
    float topSpectrumPeak = 0.0001f;
    float bottomLeftSpectrumPeak = 0.0001f;
    float bottomRightSpectrumPeak = 0.0001f;

    void renderEffect(juce::Graphics& g, const EffectConfig& config,
                     float value, const juce::Rectangle<int>& bounds);

    // Helper to get the frequency value based on FrequencyRange and panel
    float getFrequencyValue(FrequencyRange range, AudioVisualizerProcessor::PanelID panel = AudioVisualizerProcessor::Main);

    // Effect picker UI
    bool effectPickerVisible = false;
    void toggleEffectPicker();

    // UI settings
    bool showDebugValues = true;  // Show frequency debug values
    bool lightMode = false;        // Light mode vs dark mode
    juce::Colour selectedColor = juce::Colours::white;  // Color for next effect

    // Drag and drop state
    SectionID hoveredSection = SectionID::Top;
    bool isDraggingEffect = false;
    juce::Rectangle<int> topSectionBounds;
    juce::Rectangle<int> bottomLeftSectionBounds;
    juce::Rectangle<int> bottomRightSectionBounds;

    // Effect box bounds for drag detection
    juce::Rectangle<int> effectBoxBounds[4];
    juce::Rectangle<int> lightModeToggleBounds;
    juce::Rectangle<int> colorPickerBounds;
    void mouseDrag(const juce::MouseEvent& event) override;

    // Right-click menu
    void showFrequencyMenu(SectionID section);
    void applyEffectToSection(SectionID section, EffectType effect, juce::Colour color);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioVisualizerEditor)
};
