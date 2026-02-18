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

    // Mouse handling
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;

    // Drag and drop for effects (from picker)
    bool isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDragEnter(const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDragMove(const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDragExit(const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDropped(const juce::DragAndDropTarget::SourceDetails& details) override;

private:
    AudioVisualizerProcessor& audioProcessor;

    bool showLoadedMessage = false;
    int  loadedMessageTimer = 0;
    juce::String statusMessage = "Drop audio file here or press 'O' to open";

    static constexpr float visualSmoothingFactor = 0.7f;
    static constexpr float pauseFadeFactor       = 0.98f;

    // -------------------------------------------------------------------------
    // Effect instances (implementations in separate .cpp files)
    // -------------------------------------------------------------------------
    struct Star { float x, y, z, prevX, prevY, prevZ; };

    struct StarfieldInstance {
        std::vector<Star> stars;
        float currentSpeed = 2.0f;
        juce::Random random;
        StarfieldInstance() { stars.reserve(200); initStars(); }
        void initStars();
        void update(float value, bool isBinaryMode);
        void draw(juce::Graphics& g, const juce::Rectangle<int>& bounds,
                  float cx, float cy, bool lightMode, juce::Colour color);
    };

    struct RotatingCubeInstance {
        float rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f;
        float speedX = 0.4f, speedY = 0.7f, speedZ = 0.2f;
        float scale  = 1.0f;
        void update(float value);
        void draw(juce::Graphics& g, const juce::Rectangle<int>& bounds,
                  bool lightMode, juce::Colour color);
    };

    // -------------------------------------------------------------------------
    // Panel — all per-panel audio + visual state
    // -------------------------------------------------------------------------
    struct Panel {
        int id = -1;
        EffectConfig config;
        StarfieldInstance starfield;
        RotatingCubeInstance cube;
        float smoothedValue = 0.0f;
        float spectrumPeak  = 0.0001f;
        std::vector<float> spectrumSmooth;
        juce::Rectangle<int> bounds;                             // updated each frame
        AudioVisualizerProcessor::PanelID procID = AudioVisualizerProcessor::Main;
        juce::Colour bgColor       = juce::Colours::black;
        bool         hasBgOverride = false;
    };

    std::vector<std::unique_ptr<Panel>> panels;
    int nextPanelId = 0;

    Panel* findPanel(int id) const;
    int    panelAtPos(juce::Point<int> pos) const;
    int    createPanel(EffectConfig cfg, AudioVisualizerProcessor::PanelID procID);

    void renderPanel(juce::Graphics& g, Panel& p, float rawValue);
    void renderFrequencyLine(juce::Graphics& g, Panel& p);
    float getFrequencyValue(FrequencyRange range, AudioVisualizerProcessor::PanelID panel);

    // -------------------------------------------------------------------------
    // Binary split tree — defines panel layout
    // -------------------------------------------------------------------------
    struct LayoutNode {
        enum class Split { V, H };  // V = top/bottom split, H = left/right split
        bool isLeaf  = true;
        int  panelId = -1;
        Split split  = Split::V;
        float ratio  = 0.5f;        // fraction of space given to 'first' child
        std::unique_ptr<LayoutNode> first, second;
    };

    std::unique_ptr<LayoutNode> layoutRoot;

    static std::unique_ptr<LayoutNode> makeLeaf(int panelId);
    static std::unique_ptr<LayoutNode> makeSplit(LayoutNode::Split s,
                                                  std::unique_ptr<LayoutNode> a,
                                                  std::unique_ptr<LayoutNode> b,
                                                  float ratio = 0.5f);

    void computeBounds(LayoutNode* node, juce::Rectangle<int> area);
    int  countLeaves(const LayoutNode* node) const;
    bool containsPanel(const LayoutNode* node, int id) const;

    static LayoutNode* findLeaf(LayoutNode* node, int panelId);

    std::unique_ptr<LayoutNode>
        removeNode(std::unique_ptr<LayoutNode> node, int panelId);

    std::unique_ptr<LayoutNode>
        insertSplit(std::unique_ptr<LayoutNode> node,
                    int targetId, int newId,
                    LayoutNode::Split dir, bool newFirst);

    void splitPanel(int targetId, LayoutNode::Split dir, bool newFirst);
    void closePanel(int panelId);
    void swapPanels(int a, int b);

    // -------------------------------------------------------------------------
    // Panel drag (rearranging panels by click-and-hold)
    // -------------------------------------------------------------------------
    int              pdDragId   = -1;     // panel being dragged; -1 = none started
    bool             pdActive   = false;  // drag animation is live
    juce::Point<int> pdStartPos;
    juce::int64      pdStartMs  = 0;
    juce::Point<int> pdCurPos;

    static constexpr int kDragDelayMs       = 300;
    static constexpr int kDragMinPx         = 4;
    static constexpr int kClickGroupWindowMs = 400;  // group rapid clicks into one action

    // Click-group tracking for picker toggle
    int         panelClickCount   = 0;
    juce::int64 panelClickGroupMs = 0;

    struct DropZone {
        enum class Act { Top, Bottom, Left, Right, Swap };
        juce::Rectangle<int> bounds;
        int  targetId = -1;
        Act  act      = Act::Swap;
    };

    std::vector<DropZone> dz;
    int hoveredDz = -1;

    void buildDropZones();
    void updateHoverDz(juce::Point<int> pos);
    void execDrop(int dzIdx);

    // -------------------------------------------------------------------------
    // Effect picker overlay
    // -------------------------------------------------------------------------
    bool effectPickerVisible = false;
    void toggleEffectPicker();

    bool lightMode       = false;
    bool showDebugValues = true;
    juce::Colour selectedColor = juce::Colours::white;

    bool isDraggingEffect   = false;
    int  effectHoverPanelId = -1;

    juce::Rectangle<int> effectBoxBounds[5];
    juce::Rectangle<int> lightModeToggleBounds;
    juce::Rectangle<int> colorPickerBounds;

    // Background color picker (footer of effects menu)
    juce::Colour         selectedBgColor     = juce::Colours::black;
    bool                 bgColorApplyAll     = false;
    juce::Rectangle<int> bgColorPickerBounds;
    juce::Rectangle<int> bgColorToggleBounds;

    // Background color drag (hold-to-drag swatch onto a panel)
    bool             bgDragActive    = false;
    juce::Point<int> bgDragStartPos;
    juce::int64      bgDragStartMs   = 0;
    juce::Point<int> bgDragCurPos;
    int              bgHoverPanelId  = -1;

    int effectListScrollOffset = 0;   // px scrolled into the list
    int effectListAreaH        = 300; // visible list height (set each paint)
    void mouseWheelMove(const juce::MouseEvent& event,
                        const juce::MouseWheelDetails& wheel) override;

    void showPanelMenu(int panelId);
    void applyEffectToPanel(int panelId, EffectType effect, juce::Colour color);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioVisualizerEditor)
};
