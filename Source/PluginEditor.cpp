#include "PluginProcessor.h"
#include "PluginEditor.h"

// =============================================================================
// Constructor / Destructor
// =============================================================================

AudioVisualizerEditor::AudioVisualizerEditor (AudioVisualizerProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (300, 400);
    setWantsKeyboardFocus(true);
    setResizable(true, false);

    // Default layout:
    //   Top half     — Flutter / Highs
    //   Bottom left  — RotatingCube / Mids
    //   Bottom right — VSplit:
    //       top      — Starfield / KickTransient
    //       bottom   — FrequencyLine / FullSpectrum
    int topId = createPanel({ EffectType::Flutter,       FrequencyRange::Highs         },
                              AudioVisualizerProcessor::Top);
    int blId  = createPanel({ EffectType::RotatingCube,  FrequencyRange::Mids          },
                              AudioVisualizerProcessor::BottomLeft);
    int brTopId = createPanel({ EffectType::Starfield,   FrequencyRange::KickTransient },
                               AudioVisualizerProcessor::BottomRight);
    int brBotId = createPanel({ EffectType::FrequencyLine, FrequencyRange::FullSpectrum },
                               AudioVisualizerProcessor::Main);

    layoutRoot = makeSplit(LayoutNode::Split::V,
                           makeLeaf(topId),
                           makeSplit(LayoutNode::Split::H,
                                     makeLeaf(blId),
                                     makeSplit(LayoutNode::Split::V,
                                               makeLeaf(brTopId),
                                               makeLeaf(brBotId))));

    juce::Timer::callAfterDelay(100, [this]()
    {
        if (auto* top = getTopLevelComponent())
            if (auto* dw = dynamic_cast<juce::DocumentWindow*>(top))
                dw->setUsingNativeTitleBar(true);
    });

    startTimerHz(60);
}

AudioVisualizerEditor::~AudioVisualizerEditor() {}

// =============================================================================
// Panel management
// =============================================================================

int AudioVisualizerEditor::createPanel(EffectConfig cfg,
                                        AudioVisualizerProcessor::PanelID procID)
{
    auto panel = std::make_unique<Panel>();
    panel->id     = nextPanelId++;
    panel->config  = cfg;
    panel->procID  = procID;
    int id = panel->id;
    panels.push_back(std::move(panel));
    return id;
}

AudioVisualizerEditor::Panel* AudioVisualizerEditor::findPanel(int id) const
{
    for (auto& p : panels)
        if (p->id == id) return p.get();
    return nullptr;
}

int AudioVisualizerEditor::panelAtPos(juce::Point<int> pos) const
{
    for (auto& p : panels)
        if (p->bounds.contains(pos)) return p->id;
    return -1;
}

void AudioVisualizerEditor::splitPanel(int targetId,
                                        LayoutNode::Split dir,
                                        bool newFirst)
{
    if (countLeaves(layoutRoot.get()) >= 4) return;

    // Pick the next processor channel in order
    static const AudioVisualizerProcessor::PanelID kProcOrder[] = {
        AudioVisualizerProcessor::Top,
        AudioVisualizerProcessor::BottomLeft,
        AudioVisualizerProcessor::BottomRight,
        AudioVisualizerProcessor::Main
    };
    int idx = juce::jlimit(0, 3, (int)panels.size());
    auto procID = kProcOrder[idx];

    int newId = createPanel({ EffectType::Flutter, FrequencyRange::Mids }, procID);
    layoutRoot = insertSplit(std::move(layoutRoot), targetId, newId, dir, newFirst);
}

void AudioVisualizerEditor::closePanel(int panelId)
{
    if (countLeaves(layoutRoot.get()) <= 1) return;

    layoutRoot = removeNode(std::move(layoutRoot), panelId);

    panels.erase(std::remove_if(panels.begin(), panels.end(),
        [panelId](const auto& p) { return p->id == panelId; }),
        panels.end());
}

void AudioVisualizerEditor::swapPanels(int a, int b)
{
    auto* na = findLeaf(layoutRoot.get(), a);
    auto* nb = findLeaf(layoutRoot.get(), b);
    if (na && nb)
    {
        na->panelId = b;
        nb->panelId = a;
    }
}

// =============================================================================
// Layout tree helpers
// =============================================================================

std::unique_ptr<AudioVisualizerEditor::LayoutNode>
AudioVisualizerEditor::makeLeaf(int panelId)
{
    auto n = std::make_unique<LayoutNode>();
    n->isLeaf  = true;
    n->panelId = panelId;
    return n;
}

std::unique_ptr<AudioVisualizerEditor::LayoutNode>
AudioVisualizerEditor::makeSplit(LayoutNode::Split s,
                                   std::unique_ptr<LayoutNode> a,
                                   std::unique_ptr<LayoutNode> b,
                                   float ratio)
{
    auto n = std::make_unique<LayoutNode>();
    n->isLeaf  = false;
    n->split   = s;
    n->ratio   = ratio;
    n->first   = std::move(a);
    n->second  = std::move(b);
    return n;
}

void AudioVisualizerEditor::computeBounds(LayoutNode* node, juce::Rectangle<int> area)
{
    if (!node) return;

    if (node->isLeaf)
    {
        if (auto* p = findPanel(node->panelId))
            p->bounds = area;
        return;
    }

    if (node->split == LayoutNode::Split::V)
    {
        int splitY = area.getY() + (int)(area.getHeight() * node->ratio);
        computeBounds(node->first.get(),  area.withBottom(splitY));
        computeBounds(node->second.get(), area.withTop(splitY));
    }
    else
    {
        int splitX = area.getX() + (int)(area.getWidth() * node->ratio);
        computeBounds(node->first.get(),  area.withRight(splitX));
        computeBounds(node->second.get(), area.withLeft(splitX));
    }
}

int AudioVisualizerEditor::countLeaves(const LayoutNode* node) const
{
    if (!node) return 0;
    if (node->isLeaf) return 1;
    return countLeaves(node->first.get()) + countLeaves(node->second.get());
}

bool AudioVisualizerEditor::containsPanel(const LayoutNode* node, int id) const
{
    if (!node) return false;
    if (node->isLeaf) return node->panelId == id;
    return containsPanel(node->first.get(), id) || containsPanel(node->second.get(), id);
}

AudioVisualizerEditor::LayoutNode*
AudioVisualizerEditor::findLeaf(LayoutNode* node, int panelId)
{
    if (!node) return nullptr;
    if (node->isLeaf) return (node->panelId == panelId) ? node : nullptr;
    auto* result = findLeaf(node->first.get(), panelId);
    return result ? result : findLeaf(node->second.get(), panelId);
}

std::unique_ptr<AudioVisualizerEditor::LayoutNode>
AudioVisualizerEditor::removeNode(std::unique_ptr<LayoutNode> node, int panelId)
{
    if (!node) return nullptr;

    if (node->isLeaf)
        return (node->panelId == panelId) ? nullptr : std::move(node);

    if (containsPanel(node->first.get(), panelId))
    {
        auto newFirst = removeNode(std::move(node->first), panelId);
        if (!newFirst) return std::move(node->second);
        node->first = std::move(newFirst);
    }
    else
    {
        auto newSecond = removeNode(std::move(node->second), panelId);
        if (!newSecond) return std::move(node->first);
        node->second = std::move(newSecond);
    }
    return std::move(node);
}

std::unique_ptr<AudioVisualizerEditor::LayoutNode>
AudioVisualizerEditor::insertSplit(std::unique_ptr<LayoutNode> node,
                                    int targetId, int newId,
                                    LayoutNode::Split dir, bool newFirst)
{
    if (!node) return nullptr;

    if (node->isLeaf && node->panelId == targetId)
    {
        auto existing = makeLeaf(targetId);
        auto added    = makeLeaf(newId);
        if (newFirst)
            return makeSplit(dir, std::move(added),    std::move(existing));
        else
            return makeSplit(dir, std::move(existing), std::move(added));
    }

    if (!node->isLeaf)
    {
        if (containsPanel(node->first.get(), targetId))
            node->first = insertSplit(std::move(node->first), targetId, newId, dir, newFirst);
        else
            node->second = insertSplit(std::move(node->second), targetId, newId, dir, newFirst);
    }
    return std::move(node);
}

// =============================================================================
// Drop zone helpers
// =============================================================================

void AudioVisualizerEditor::buildDropZones()
{
    dz.clear();
    bool canSplit = (countLeaves(layoutRoot.get()) < 4);

    for (auto& panel : panels)
    {
        if (panel->id == pdDragId) continue;

        auto b  = panel->bounds;
        int  id = panel->id;
        int  w  = b.getWidth();
        int  h  = b.getHeight();

        // Center zone — swap
        dz.push_back({ b.reduced(w / 4, h / 4), id, DropZone::Act::Swap });

        if (canSplit)
        {
            dz.push_back({ b.withHeight(h / 4),                              id, DropZone::Act::Top    });
            dz.push_back({ b.withTop(b.getBottom() - h / 4),                id, DropZone::Act::Bottom });
            dz.push_back({ b.withWidth(w / 4),                              id, DropZone::Act::Left   });
            dz.push_back({ b.withLeft(b.getRight() - w / 4),                id, DropZone::Act::Right  });
        }
    }
}

void AudioVisualizerEditor::updateHoverDz(juce::Point<int> pos)
{
    hoveredDz = -1;
    for (int i = 0; i < (int)dz.size(); ++i)
    {
        if (dz[i].bounds.contains(pos))
        {
            hoveredDz = i;
            break;
        }
    }
}

void AudioVisualizerEditor::execDrop(int dzIdx)
{
    if (dzIdx < 0 || dzIdx >= (int)dz.size()) return;

    const auto& zone = dz[dzIdx];
    int srcId  = pdDragId;
    int tgtId  = zone.targetId;

    if (zone.act == DropZone::Act::Swap)
    {
        swapPanels(srcId, tgtId);
        return;
    }

    // Directional drop: remove source, then split target with source
    layoutRoot = removeNode(std::move(layoutRoot), srcId);

    LayoutNode::Split dir;
    bool newFirst;
    switch (zone.act)
    {
        case DropZone::Act::Top:    dir = LayoutNode::Split::V; newFirst = true;  break;
        case DropZone::Act::Bottom: dir = LayoutNode::Split::V; newFirst = false; break;
        case DropZone::Act::Left:   dir = LayoutNode::Split::H; newFirst = true;  break;
        case DropZone::Act::Right:  dir = LayoutNode::Split::H; newFirst = false; break;
        default: return;
    }

    layoutRoot = insertSplit(std::move(layoutRoot), tgtId, srcId, dir, newFirst);
}

// =============================================================================
// Audio value helpers
// =============================================================================

float AudioVisualizerEditor::getFrequencyValue(FrequencyRange range,
                                                 AudioVisualizerProcessor::PanelID panel)
{
    switch (range)
    {
        case FrequencyRange::SubBass:       return audioProcessor.getSubBassEnergy(panel);
        case FrequencyRange::Bass:          return audioProcessor.getBassEnergy(panel);
        case FrequencyRange::LowMids:       return audioProcessor.getLowMidEnergy(panel);
        case FrequencyRange::Mids:          return audioProcessor.getMidEnergy(panel);
        case FrequencyRange::HighMids:      return audioProcessor.getHighMidEnergy(panel);
        case FrequencyRange::Highs:         return audioProcessor.getHighEnergy(panel);
        case FrequencyRange::VeryHighs:     return audioProcessor.getVeryHighEnergy(panel);
        case FrequencyRange::KickTransient: return audioProcessor.getKickTransient(panel);
        case FrequencyRange::FullSpectrum:  return audioProcessor.getFullSpectrum(panel);
        default: return 0.0f;
    }
}

// =============================================================================
// Rendering
// =============================================================================

void AudioVisualizerEditor::renderFrequencyLine(juce::Graphics& g, Panel& p)
{
    auto& b = p.bounds;

    float minFreq = 20.0f, maxFreq = 20000.0f;
    switch (p.config.frequencyRange)
    {
        case FrequencyRange::SubBass:       minFreq = 20.0f;   maxFreq = 60.0f;    break;
        case FrequencyRange::Bass:          minFreq = 60.0f;   maxFreq = 250.0f;   break;
        case FrequencyRange::LowMids:       minFreq = 250.0f;  maxFreq = 500.0f;   break;
        case FrequencyRange::Mids:          minFreq = 500.0f;  maxFreq = 2000.0f;  break;
        case FrequencyRange::HighMids:      minFreq = 2000.0f; maxFreq = 4000.0f;  break;
        case FrequencyRange::Highs:         minFreq = 4000.0f; maxFreq = 8000.0f;  break;
        case FrequencyRange::VeryHighs:     minFreq = 8000.0f; maxFreq = 20000.0f; break;
        case FrequencyRange::KickTransient: minFreq = 50.0f;   maxFreq = 90.0f;    break;
        case FrequencyRange::FullSpectrum:  minFreq = 20.0f;   maxFreq = 20000.0f; break;
    }

    std::vector<float> spectrum;
    audioProcessor.getSpectrumForRange(minFreq, maxFreq, spectrum, 50, p.procID);
    if (spectrum.size() < 2) return;

    if (p.spectrumSmooth.size() != spectrum.size())
        p.spectrumSmooth = spectrum;

    // Spatial smoothing (window = 2)
    std::vector<float> spatial(spectrum.size());
    for (int i = 0; i < (int)spectrum.size(); ++i)
    {
        float sum = 0.0f; int count = 0;
        for (int j = -2; j <= 2; ++j)
        {
            int idx = i + j;
            if (idx >= 0 && idx < (int)spectrum.size())
                { sum += spectrum[idx]; ++count; }
        }
        spatial[i] = sum / count;
    }

    // Temporal smoothing (96% previous, 4% new)
    std::vector<float> smoothed(spectrum.size());
    for (int i = 0; i < (int)spectrum.size(); ++i)
    {
        smoothed[i] = p.spectrumSmooth[i] * 0.96f + spatial[i] * 0.04f;
        p.spectrumSmooth[i] = smoothed[i];
    }

    // Kick transient modulation
    if (p.config.frequencyRange == FrequencyRange::KickTransient)
    {
        float kv = audioProcessor.getKickTransient(p.procID);
        for (auto& v : smoothed) v *= kv;
    }

    // Adaptive normalization
    float currentPeak = 0.0001f;
    for (float v : smoothed) if (v > currentPeak) currentPeak = v;

    if (currentPeak > p.spectrumPeak)
        p.spectrumPeak = p.spectrumPeak * 0.3f + currentPeak * 0.7f;
    else
        p.spectrumPeak = p.spectrumPeak * 0.92f + currentPeak * 0.08f;

    static constexpr float amplitudeGain = 1.5f;
    float normFactor = p.spectrumPeak * amplitudeGain;
    for (auto& v : smoothed) v /= normFactor;

    // Build cubic path
    juce::Path path;
    float xScale = (float)b.getWidth() / (float)(smoothed.size() - 1);

    auto clampY = [&](float v) {
        return juce::jlimit((float)b.getY(), (float)b.getBottom(),
                            b.getBottom() - v * amplitudeGain * b.getHeight());
    };

    path.startNewSubPath((float)b.getX(), clampY(smoothed[0]));

    for (int i = 1; i < (int)smoothed.size(); ++i)
    {
        float x     = b.getX() + i * xScale;
        float y     = clampY(smoothed[i]);
        float prevX = b.getX() + (i - 1) * xScale;
        float prevY = clampY(smoothed[i - 1]);

        path.cubicTo(prevX + (x - prevX) * 0.25f, prevY,
                     prevX + (x - prevX) * 0.75f, y,
                     x, y);
    }

    g.setColour(p.config.effectColor);
    g.strokePath(path, juce::PathStrokeType(1.15f,
        juce::PathStrokeType::curved,
        juce::PathStrokeType::rounded));
}

void AudioVisualizerEditor::renderPanel(juce::Graphics& g, Panel& p, float rawValue)
{
    auto& b = p.bounds;
    auto  t = p.config.type;

    if (t == EffectType::Flutter)
    {
        auto bg = lightMode ? juce::Colours::white : juce::Colours::black;
        g.setColour(bg.interpolatedWith(p.config.effectColor, p.smoothedValue));
        g.fillRect(b);
    }
    else if (t == EffectType::BinaryFlash)
    {
        bool flash = p.smoothedValue > 0.3f;
        g.setColour(flash ? p.config.effectColor
                           : (lightMode ? juce::Colours::white : juce::Colours::black));
        g.fillRect(b);
    }
    else if (t == EffectType::Starfield)
    {
        g.setColour(lightMode ? juce::Colours::white : juce::Colours::black);
        g.fillRect(b);
        bool binaryMode = (p.config.frequencyRange == FrequencyRange::KickTransient);
        float cx = b.getX() + b.getWidth()  * 0.5f;
        float cy = b.getY() + b.getHeight() * 0.5f;
        p.starfield.update(rawValue, binaryMode);
        p.starfield.draw(g, b, cx, cy, lightMode, p.config.effectColor);
    }
    else if (t == EffectType::RotatingCube)
    {
        g.setColour(lightMode ? juce::Colours::white : juce::Colours::black);
        g.fillRect(b);
        p.cube.update(rawValue);
        p.cube.draw(g, b, lightMode, p.config.effectColor);
    }
    else if (t == EffectType::FrequencyLine)
    {
        g.setColour(lightMode ? juce::Colours::white : juce::Colours::black);
        g.fillRect(b);
        renderFrequencyLine(g, p);
    }
}

// =============================================================================
// paint()
// =============================================================================

void AudioVisualizerEditor::paint (juce::Graphics& g)
{
    static constexpr int menuWidth = 220;

    auto fullBounds = getLocalBounds();
    auto vizBounds  = fullBounds;
    if (effectPickerVisible)
        vizBounds.removeFromRight(menuWidth);

    // Compute panel bounds from layout tree
    computeBounds(layoutRoot.get(), vizBounds);

    bool isPlaying = audioProcessor.isPlaying();

    // -------------------------------------------------------------------------
    // Render each panel
    // -------------------------------------------------------------------------
    for (auto& panel : panels)
    {
        if (panel->bounds.isEmpty()) continue;

        float rawValue = getFrequencyValue(panel->config.frequencyRange, panel->procID);

        if (isPlaying)
            panel->smoothedValue = panel->smoothedValue * visualSmoothingFactor
                                 + rawValue * (1.0f - visualSmoothingFactor);
        else
            panel->smoothedValue *= pauseFadeFactor;

        {
            juce::Graphics::ScopedSaveState clip(g);
            g.reduceClipRegion(panel->bounds);
            renderPanel(g, *panel, rawValue);

            // Effect-drop hover highlight
            if (isDraggingEffect && panel->id == effectHoverPanelId)
            {
                g.setColour(juce::Colour(0, 122, 255).withAlpha(0.8f));
                g.drawRect(panel->bounds.toFloat(), 4.0f);
            }
        }

        // Subtle border between panels
        auto borderCol = lightMode ? juce::Colour(180, 180, 180)
                                   : juce::Colour(30, 30, 30);
        g.setColour(borderCol);
        g.drawRect(panel->bounds.toFloat(), 1.0f);
    }

    // -------------------------------------------------------------------------
    // Panel drag overlay
    // -------------------------------------------------------------------------
    if (pdActive)
    {
        // Show a grey preview block of where the panel will land when hovering
        if (hoveredDz >= 0 && hoveredDz < (int)dz.size())
        {
            const auto& zone = dz[hoveredDz];
            juce::Rectangle<int> preview;

            if (auto* target = findPanel(zone.targetId))
            {
                auto b = target->bounds;
                switch (zone.act)
                {
                    case DropZone::Act::Swap:
                        preview = b;
                        break;
                    case DropZone::Act::Top:
                        preview = b.withHeight(b.getHeight() / 2);
                        break;
                    case DropZone::Act::Bottom:
                        preview = b.withTop(b.getY() + b.getHeight() / 2);
                        break;
                    case DropZone::Act::Left:
                        preview = b.withWidth(b.getWidth() / 2);
                        break;
                    case DropZone::Act::Right:
                        preview = b.withLeft(b.getX() + b.getWidth() / 2);
                        break;
                }
            }

            if (!preview.isEmpty())
            {
                g.setColour(juce::Colours::grey.withAlpha(0.55f));
                g.fillRect(preview);
                g.setColour(juce::Colours::white.withAlpha(0.85f));
                g.drawRect(preview.toFloat(), 2.0f);
            }
        }

        // Subtle border on the panel being dragged
        if (auto* src = findPanel(pdDragId))
        {
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            g.drawRect(src->bounds.toFloat(), 3.0f);
        }
    }

    // -------------------------------------------------------------------------
    // Effect picker panel  (frosted-glass aesthetic, tight layout)
    // -------------------------------------------------------------------------
    if (effectPickerVisible)
    {
        auto pk = fullBounds.removeFromRight(menuWidth);

        auto bgBase  = lightMode ? juce::Colour(245, 245, 250) : juce::Colour(18, 18, 24);
        auto sepCol  = lightMode ? juce::Colour(200, 200, 210) : juce::Colour(48, 48, 58);
        auto textCol = lightMode ? juce::Colours::black        : juce::Colours::white;
        auto dimCol  = lightMode ? juce::Colours::black.withAlpha(0.32f)
                                 : juce::Colours::white.withAlpha(0.32f);
        auto rowDiv  = lightMode ? juce::Colour(210, 210, 220) : juce::Colour(38, 38, 48);

        juce::ColourGradient bgGrad(
            bgBase, (float)pk.getX(), (float)pk.getY(),
            lightMode ? juce::Colour(235, 235, 242) : juce::Colour(12, 12, 18),
            (float)pk.getX(), (float)pk.getBottom(), false);
        g.setGradientFill(bgGrad);
        g.fillRect(pk);
        g.setColour(sepCol);
        g.fillRect(juce::Rectangle<int>(pk.getX(), pk.getY(), 1, pk.getHeight()));

        // Reserve footer: toggle (52px) + instructions (38px)
        auto toggleArea = pk.removeFromBottom(52);
        auto instrArea  = pk.removeFromBottom(38);

        pk.removeFromTop(10);

        // ---- Title ----
        auto titleRow = pk.removeFromTop(34);
        g.setColour(textCol);
        g.setFont(juce::Font(15.0f, juce::Font::bold));
        g.drawText("Effects", titleRow.reduced(18, 0), juce::Justification::centredLeft);

        g.setColour(sepCol);
        g.fillRect(pk.removeFromTop(1));
        pk.removeFromTop(2);

        // ---- Color row ----
        auto colorRow = pk.removeFromTop(38).reduced(18, 0);
        g.setColour(textCol);
        g.setFont(13.0f);
        g.drawText("Color", colorRow.removeFromLeft(46), juce::Justification::centredLeft);

        colorPickerBounds = colorRow.removeFromLeft(24).reduced(0, 7);
        g.setColour(selectedColor);
        g.fillRoundedRectangle(colorPickerBounds.toFloat(), 3.0f);
        g.setColour(sepCol);
        g.drawRoundedRectangle(colorPickerBounds.toFloat(), 3.0f, 1.0f);

        g.setColour(dimCol);
        g.setFont(11.0f);
        g.drawText("#" + selectedColor.toDisplayString(false),
                   colorRow.reduced(6, 0), juce::Justification::centredLeft);

        pk.removeFromTop(2);
        g.setColour(sepCol);
        g.fillRect(pk.removeFromTop(1));

        // ---- Effect list (scrollable) ----
        auto listArea = pk;
        effectListAreaH = listArea.getHeight();

        static constexpr int kRowH = 36;
        int totalListH  = 5 * kRowH;
        int maxScroll   = std::max(0, totalListH - effectListAreaH);
        effectListScrollOffset = juce::jlimit(0, maxScroll, effectListScrollOffset);

        static const char* kEffectNames[] = {
            "Binary Flash", "Flutter", "Starfield", "Spectrum", "3D Cube"
        };

        {
            juce::Graphics::ScopedSaveState listClip(g);
            g.reduceClipRegion(listArea);

            for (int i = 0; i < 5; i++)
            {
                int itemY = listArea.getY() + i * kRowH - effectListScrollOffset;
                auto row  = juce::Rectangle<int>(listArea.getX(), itemY,
                                                  listArea.getWidth(), kRowH);

                bool visible = row.getBottom() > listArea.getY()
                            && row.getY()      < listArea.getBottom();
                effectBoxBounds[i] = visible ? row : juce::Rectangle<int>{};
                if (!visible) continue;

                g.setColour(textCol);
                g.setFont(13.0f);
                g.drawText(kEffectNames[i], row.reduced(18, 0),
                           juce::Justification::centredLeft);

                // Six-dot drag handle (right side)
                float hdx = (float)(row.getRight() - 18);
                float hdy = (float)row.getCentreY();
                g.setColour(dimCol);
                for (int col = 0; col < 2; ++col)
                    for (int r2 = -1; r2 <= 1; ++r2)
                        g.fillEllipse(hdx + col * 5.0f - 2.5f,
                                      hdy + r2 * 4.0f - 1.0f, 2.0f, 2.0f);

                if (i < 4)
                {
                    g.setColour(rowDiv);
                    g.fillRect(row.getX() + 18, row.getBottom(), row.getWidth() - 18, 1);
                }
            }

            // Scroll indicator
            if (maxScroll > 0)
            {
                float frac   = (float)effectListScrollOffset / (float)maxScroll;
                int   thumbH = std::max(20, effectListAreaH * effectListAreaH / totalListH);
                int   thumbY = listArea.getY()
                             + (int)((effectListAreaH - thumbH) * frac);
                g.setColour(dimCol.withAlpha(0.55f));
                g.fillRoundedRectangle((float)(listArea.getRight() - 5), (float)thumbY,
                                        3.0f, (float)thumbH, 1.5f);
            }
        }

        // ---- Instructions ----
        g.setColour(sepCol);
        g.fillRect(juce::Rectangle<int>(instrArea.getX(), instrArea.getY(),
                                         instrArea.getWidth(), 1));
        instrArea.removeFromTop(6);
        g.setColour(dimCol);
        g.setFont(10.0f);
        g.drawText("Drag effect onto panel",
                   instrArea.removeFromTop(14).reduced(18, 0),
                   juce::Justification::centredLeft);
        g.drawText("Right-click panel for options",
                   instrArea.removeFromTop(14).reduced(18, 0),
                   juce::Justification::centredLeft);

        // ---- Light / dark toggle — single moving dot ----
        g.setColour(sepCol);
        g.fillRect(juce::Rectangle<int>(toggleArea.getX(), toggleArea.getY(),
                                         toggleArea.getWidth(), 1));

        auto switchArea = juce::Rectangle<int>(toggleArea.getX() + 18,
                                               toggleArea.getCentreY() - 11,
                                               44, 22);
        lightModeToggleBounds = switchArea;

        g.setColour(lightMode ? juce::Colour(190, 190, 205) : juce::Colour(50, 50, 62));
        g.fillRoundedRectangle(switchArea.toFloat(), 11.0f);

        // Dot slides left (dark) or right (light)
        float dotCX = lightMode ? (float)(switchArea.getRight() - 14)
                                 : (float)(switchArea.getX()    + 14);
        float dotCY = (float)switchArea.getCentreY();
        g.setColour(lightMode ? juce::Colours::black.withAlpha(0.55f)
                              : juce::Colours::white.withAlpha(0.90f));
        g.fillEllipse(dotCX - 7.0f, dotCY - 7.0f, 14.0f, 14.0f);
    }

    // -------------------------------------------------------------------------
    // Debug frequency values
    // -------------------------------------------------------------------------
    bool shouldShowDebug = showDebugValues &&
                           (audioProcessor.wrapperType != juce::AudioProcessor::wrapperType_Standalone
                            || audioProcessor.isAudioLoaded());
    if (shouldShowDebug)
    {
        auto getFreqName = [](FrequencyRange r) -> juce::String {
            switch (r) {
                case FrequencyRange::SubBass:       return "Sub-Bass";
                case FrequencyRange::Bass:          return "Bass";
                case FrequencyRange::LowMids:       return "Low-Mids";
                case FrequencyRange::Mids:          return "Mids";
                case FrequencyRange::HighMids:      return "High-Mids";
                case FrequencyRange::Highs:         return "Highs";
                case FrequencyRange::VeryHighs:     return "Very Highs";
                case FrequencyRange::KickTransient: return "Kick";
                case FrequencyRange::FullSpectrum:  return "Full";
                default:                            return "?";
            }
        };

        auto textCol = lightMode ? juce::Colours::black.withAlpha(0.8f)
                                 : juce::Colours::white.withAlpha(0.8f);
        g.setColour(textCol);
        g.setFont(12.0f);

        for (auto& panel : panels)
        {
            float raw = getFrequencyValue(panel->config.frequencyRange, panel->procID);
            juce::String txt = getFreqName(panel->config.frequencyRange)
                             + ": " + juce::String(raw, 2);
            g.drawText(txt, panel->bounds.reduced(10).removeFromTop(20),
                       juce::Justification::topLeft);
        }
    }

    // -------------------------------------------------------------------------
    // Standalone loading overlay
    // -------------------------------------------------------------------------
    if (audioProcessor.wrapperType == juce::AudioProcessor::wrapperType_Standalone &&
        (!audioProcessor.isAudioLoaded() || showLoadedMessage))
    {
        g.setColour(juce::Colours::white);
        g.setFont(16.0f);

        if (showLoadedMessage)
        {
            g.drawText("Audio loaded! Press SPACE to play",
                       getLocalBounds(), juce::Justification::centred);
        }
        else
        {
            g.drawText(statusMessage, getLocalBounds(), juce::Justification::centred);
            g.setFont(14.0f);
            g.setColour(juce::Colours::white.withAlpha(0.7f));
            auto instrArea = getLocalBounds().reduced(20);
            instrArea.setY(getHeight() - 100);
            g.drawText("Supported formats: WAV, AIFF, MP3, FLAC",
                       instrArea, juce::Justification::centredBottom);
        }
    }
}

void AudioVisualizerEditor::resized()
{
    auto vizBounds = getLocalBounds();
    if (effectPickerVisible) vizBounds.removeFromRight(220);
    computeBounds(layoutRoot.get(), vizBounds);
}

// =============================================================================
// Mouse events
// =============================================================================

void AudioVisualizerEditor::mouseDoubleClick(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    toggleEffectPicker();
}

void AudioVisualizerEditor::mouseDown(const juce::MouseEvent& event)
{
    auto pos = event.getPosition();

    // --- Effect picker UI clicks ---
    if (effectPickerVisible)
    {
        if (colorPickerBounds.contains(pos))
        {
            struct ColourPickerWithHex : public juce::Component,
                                         public juce::ChangeListener,
                                         public juce::TextEditor::Listener
            {
                ColourPickerWithHex(AudioVisualizerEditor& ed, juce::Colour init)
                    : editor(ed)
                {
                    colourSelector = std::make_unique<juce::ColourSelector>(
                        juce::ColourSelector::showColourAtTop |
                        juce::ColourSelector::showColourspace);
                    colourSelector->setCurrentColour(init);
                    colourSelector->addChangeListener(this);
                    addAndMakeVisible(*colourSelector);

                    hexInput = std::make_unique<juce::TextEditor>();
                    hexInput->setText(init.toDisplayString(false), false);
                    hexInput->setJustification(juce::Justification::centred);
                    hexInput->addListener(this);
                    addAndMakeVisible(*hexInput);

                    addAndMakeVisible(hexLabel);
                    hexLabel.setText("Hex:", juce::dontSendNotification);
                    hexLabel.setJustificationType(juce::Justification::centredRight);
                    hexLabel.setColour(juce::Label::textColourId, juce::Colours::white);

                    setSize(300, 350);
                }

                void resized() override
                {
                    auto b = getLocalBounds();
                    auto hexArea = b.removeFromBottom(40).reduced(10, 5);
                    hexLabel.setBounds(hexArea.removeFromLeft(40));
                    hexInput->setBounds(hexArea);
                    colourSelector->setBounds(b);
                }

                void changeListenerCallback(juce::ChangeBroadcaster* src) override
                {
                    if (auto* cs = dynamic_cast<juce::ColourSelector*>(src))
                    {
                        auto c = cs->getCurrentColour();
                        editor.selectedColor = c;
                        hexInput->setText(c.toDisplayString(false), false);
                        editor.repaint();
                    }
                }

                void textEditorTextChanged(juce::TextEditor& ed) override
                {
                    // fromString() expects "AARRGGBB" — strip # and prepend full alpha
                    auto hex = ed.getText().trim().trimCharactersAtStart("#");
                    if (hex.length() == 6)
                    {
                        auto c = juce::Colour::fromString("ff" + hex);
                        colourSelector->setCurrentColour(c, juce::dontSendNotification);
                        editor.selectedColor = c;
                        editor.repaint();
                    }
                }

                void textEditorReturnKeyPressed(juce::TextEditor& ed) override { textEditorTextChanged(ed); }
                void textEditorFocusLost(juce::TextEditor& ed) override        { textEditorTextChanged(ed); }

                AudioVisualizerEditor& editor;
                std::unique_ptr<juce::ColourSelector> colourSelector;
                std::unique_ptr<juce::TextEditor> hexInput;
                juce::Label hexLabel;
            };

            auto picker = std::make_unique<ColourPickerWithHex>(*this, selectedColor);
            juce::CallOutBox::launchAsynchronously(std::move(picker), colorPickerBounds, this);
            return;
        }

        if (lightModeToggleBounds.contains(pos))
        {
            lightMode = !lightMode;
            repaint();
            return;
        }
    }

    // --- Right-click: panel options menu ---
    if (event.mods.isPopupMenu())
    {
        int id = panelAtPos(pos);
        if (id >= 0) showPanelMenu(id);
        return;
    }

    // --- Left-click in visualizer: start potential panel drag ---
    auto vizBounds = getLocalBounds();
    if (effectPickerVisible) vizBounds.removeFromRight(220);

    if (vizBounds.contains(pos))
    {
        int id = panelAtPos(pos);
        if (id >= 0)
        {
            pdDragId  = id;
            pdStartPos = pos;
            pdCurPos   = pos;
            pdStartMs  = juce::Time::currentTimeMillis();
            pdActive   = false;
        }
    }
}

void AudioVisualizerEditor::mouseUp(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);

    if (pdActive && hoveredDz >= 0)
        execDrop(hoveredDz);

    pdDragId  = -1;
    pdActive  = false;
    pdStartMs = 0;
    dz.clear();
    hoveredDz = -1;

    repaint();
}

void AudioVisualizerEditor::mouseDrag(const juce::MouseEvent& event)
{
    auto pos = event.getPosition();
    pdCurPos = pos;

    // --- Effect picker drag (drag effect onto panel) ---
    if (!pdActive && effectPickerVisible)
    {
        for (int i = 0; i < 5; i++)
        {
            if (effectBoxBounds[i].contains(pos))
            {
                EffectType effectType;
                switch (i)
                {
                    case 0: effectType = EffectType::BinaryFlash;  break;
                    case 1: effectType = EffectType::Flutter;       break;
                    case 2: effectType = EffectType::Starfield;     break;
                    case 3: effectType = EffectType::FrequencyLine; break;
                    case 4: effectType = EffectType::RotatingCube;  break;
                    default: return;
                }

                auto dragInfo = new juce::DynamicObject();
                dragInfo->setProperty("effectType", static_cast<int>(effectType));
                dragInfo->setProperty("color", selectedColor.toString());
                juce::var effectData(dragInfo);

                juce::Image dragImage(juce::Image::ARGB, 100, 40, true);
                juce::Graphics dg(dragImage);
                dg.fillAll(juce::Colour(60, 60, 65));
                dg.setColour(selectedColor);
                dg.fillRoundedRectangle(5, 5, 20, 30, 3.0f);
                dg.setColour(juce::Colours::white);
                dg.setFont(14.0f);
                static const char* kNames[] = {
                    "Binary Flash","Flutter","Starfield","Spectrum","3D Cube"
                };
                dg.drawText(kNames[i], juce::Rectangle<int>(30, 0, 70, 40),
                            juce::Justification::centredLeft);

                startDragging(effectData, this, dragImage, true);
                return;
            }
        }
    }

    // --- Panel drag: update hover drop zone ---
    if (pdActive)
    {
        updateHoverDz(pos);
        repaint();
    }
}

// =============================================================================
// Timer
// =============================================================================

void AudioVisualizerEditor::timerCallback()
{
    // Panel drag: activate after delay even if mouse hasn't moved
    if (pdDragId >= 0 && !pdActive)
    {
        auto elapsed = juce::Time::currentTimeMillis() - pdStartMs;
        auto dist    = (float)pdCurPos.getDistanceFrom(pdStartPos);
        if (elapsed >= kDragDelayMs && dist >= (float)kDragMinPx)
        {
            pdActive = true;
            buildDropZones();
            repaint();
        }
    }

    // Loaded message countdown
    if (showLoadedMessage)
    {
        if (--loadedMessageTimer <= 0)
            showLoadedMessage = false;
    }

    repaint();
}

// =============================================================================
// Effect picker
// =============================================================================

void AudioVisualizerEditor::toggleEffectPicker()
{
    effectPickerVisible = !effectPickerVisible;
    static constexpr int menuWidth = 220;
    setSize(effectPickerVisible ? getWidth() + menuWidth : getWidth() - menuWidth,
            getHeight());
    repaint();
}

void AudioVisualizerEditor::showPanelMenu(int panelId)
{
    auto* panel = findPanel(panelId);
    if (!panel) return;

    auto  currentRange = panel->config.frequencyRange;
    int   numPanels    = countLeaves(layoutRoot.get());
    bool  hasSidechain = audioProcessor.hasSidechainInput(panel->procID);

    juce::PopupMenu menu;
    menu.addItem(1, "Sub-Bass (20-60 Hz)",      true, currentRange == FrequencyRange::SubBass);
    menu.addItem(2, "Bass (60-250 Hz)",          true, currentRange == FrequencyRange::Bass);
    menu.addItem(3, "Low-Mids (250-500 Hz)",     true, currentRange == FrequencyRange::LowMids);
    menu.addItem(4, "Mids (500-2000 Hz)",        true, currentRange == FrequencyRange::Mids);
    menu.addItem(5, "High-Mids (2000-4000 Hz)",  true, currentRange == FrequencyRange::HighMids);
    menu.addItem(6, "Highs (4000-8000 Hz)",      true, currentRange == FrequencyRange::Highs);
    menu.addItem(7, "Very Highs (8000-20000 Hz)",true, currentRange == FrequencyRange::VeryHighs);
    menu.addItem(8, "Kick Transient (50-90 Hz)", true, currentRange == FrequencyRange::KickTransient);
    menu.addItem(9, "Full Spectrum",             true, currentRange == FrequencyRange::FullSpectrum);

    menu.addSeparator();
    menu.addItem(10, "Show Values", true, showDebugValues);

    menu.addSeparator();
    menu.addItem(11, hasSidechain ? "Input: Sidechain" : "Input: Main Track", false, false);

    menu.addSeparator();
    menu.addItem(20, "Open New Panel", numPanels < 4, false);
    menu.addItem(21, "Close Panel",    numPanels > 1, false);

    menu.showMenuAsync(juce::PopupMenu::Options(), [this, panelId](int result)
    {
        auto* p = findPanel(panelId);
        if (!p) return;

        if (result == 10)
        {
            showDebugValues = !showDebugValues;
            repaint();
            return;
        }
        if (result == 20)
        {
            // Split this panel — direction based on larger dimension
            auto dir = (p->bounds.getWidth() >= p->bounds.getHeight())
                       ? LayoutNode::Split::H
                       : LayoutNode::Split::V;
            splitPanel(panelId, dir, false);
            repaint();
            return;
        }
        if (result == 21)
        {
            closePanel(panelId);
            repaint();
            return;
        }

        FrequencyRange range;
        switch (result)
        {
            case 1: range = FrequencyRange::SubBass;       break;
            case 2: range = FrequencyRange::Bass;          break;
            case 3: range = FrequencyRange::LowMids;       break;
            case 4: range = FrequencyRange::Mids;          break;
            case 5: range = FrequencyRange::HighMids;      break;
            case 6: range = FrequencyRange::Highs;         break;
            case 7: range = FrequencyRange::VeryHighs;     break;
            case 8: range = FrequencyRange::KickTransient; break;
            case 9: range = FrequencyRange::FullSpectrum;  break;
            default: return;
        }
        p->config.frequencyRange = range;
        p->spectrumSmooth.clear();  // reset spectrum buffer on range change
    });
}

void AudioVisualizerEditor::applyEffectToPanel(int panelId, EffectType effect,
                                                 juce::Colour color)
{
    auto* p = findPanel(panelId);
    if (!p) return;

    p->config.type        = effect;
    p->config.effectColor = color;
    if (effect == EffectType::Starfield)
        p->starfield.initStars();

    repaint();
}

// =============================================================================
// Drag and drop (effects from picker onto panels)
// =============================================================================

bool AudioVisualizerEditor::isInterestedInDragSource(
    const juce::DragAndDropTarget::SourceDetails& details)
{
    return details.description.isInt() || details.description.isObject();
}

void AudioVisualizerEditor::itemDragEnter(
    const juce::DragAndDropTarget::SourceDetails& details)
{
    juce::ignoreUnused(details);
    isDraggingEffect = true;
}

void AudioVisualizerEditor::itemDragMove(
    const juce::DragAndDropTarget::SourceDetails& details)
{
    effectHoverPanelId = panelAtPos(details.localPosition.toInt());
    repaint();
}

void AudioVisualizerEditor::itemDragExit(
    const juce::DragAndDropTarget::SourceDetails& details)
{
    juce::ignoreUnused(details);
    isDraggingEffect   = false;
    effectHoverPanelId = -1;
    repaint();
}

void AudioVisualizerEditor::itemDropped(
    const juce::DragAndDropTarget::SourceDetails& details)
{
    isDraggingEffect = false;

    int targetId = panelAtPos(details.localPosition.toInt());
    if (targetId < 0) { repaint(); return; }

    if (details.description.isObject())
    {
        auto* obj = details.description.getDynamicObject();
        if (obj)
        {
            auto effectType = static_cast<EffectType>(
                static_cast<int>(obj->getProperty("effectType")));
            auto color = juce::Colour::fromString(
                obj->getProperty("color").toString());
            applyEffectToPanel(targetId, effectType, color);
        }
    }
    else if (details.description.isInt())
    {
        applyEffectToPanel(targetId,
                           static_cast<EffectType>(static_cast<int>(details.description)),
                           juce::Colours::white);
    }

    effectHoverPanelId = -1;
    repaint();
}

// =============================================================================
// Mouse wheel (scroll effect list)
// =============================================================================

void AudioVisualizerEditor::mouseWheelMove(const juce::MouseEvent& event,
                                             const juce::MouseWheelDetails& wheel)
{
    if (!effectPickerVisible) return;
    if (event.getPosition().getX() < getWidth() - 220) return; // not over picker

    effectListScrollOffset -= (int)(wheel.deltaY * 60.0f);

    int maxScroll = std::max(0, 5 * 36 - effectListAreaH);
    effectListScrollOffset = juce::jlimit(0, maxScroll, effectListScrollOffset);

    repaint();
}

// =============================================================================
// Keyboard
// =============================================================================

bool AudioVisualizerEditor::keyPressed (const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::spaceKey)
    {
        if (audioProcessor.isAudioLoaded())
        {
            audioProcessor.setPlaying(!audioProcessor.isPlaying());
            showLoadedMessage = false;
            repaint();
            return true;
        }
    }

    if (key.getKeyCode() == 'O' || key.getKeyCode() == 'o')
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Select an audio file to visualize...",
            juce::File::getSpecialLocation(juce::File::userMusicDirectory),
            "*.wav;*.aiff;*.aif;*.mp3;*.flac;*.ogg;*.m4a");

        chooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file != juce::File())
                {
                    audioProcessor.loadAudioFile(file);
                    if (audioProcessor.isAudioLoaded())
                    {
                        showLoadedMessage = true;
                        loadedMessageTimer = 120;
                        statusMessage = "Audio loaded: " + file.getFileName();
                    }
                    else
                    {
                        statusMessage = "Failed to load audio file";
                    }
                    repaint();
                }
            });
        return true;
    }

    return false;
}

// =============================================================================
// File drag and drop
// =============================================================================

bool AudioVisualizerEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (f.endsWithIgnoreCase(".wav")  || f.endsWithIgnoreCase(".aif")  ||
            f.endsWithIgnoreCase(".aiff") || f.endsWithIgnoreCase(".mp3")  ||
            f.endsWithIgnoreCase(".flac") || f.endsWithIgnoreCase(".ogg")  ||
            f.endsWithIgnoreCase(".m4a"))
            return true;
    return false;
}

void AudioVisualizerEditor::filesDropped (const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused(x, y);
    if (files.size() > 0)
    {
        audioProcessor.loadAudioFile(juce::File(files[0]));
        if (audioProcessor.isAudioLoaded())
        {
            showLoadedMessage  = true;
            loadedMessageTimer = 120;
            statusMessage = "Audio loaded: " + juce::File(files[0]).getFileName();
        }
        else
        {
            statusMessage = "Failed to load audio file";
        }
        repaint();
    }
}
