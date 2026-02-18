#include "PluginProcessor.h"
#include "PluginEditor.h"

AudioVisualizerEditor::AudioVisualizerEditor (AudioVisualizerProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (800, 600);
    setWantsKeyboardFocus(true);
    setResizable(true, false); // Allow window resizing with native controls

    // Starfield instances are auto-initialized in their constructors

    // Enable native macOS window decorations
    // We need to wait a moment for the window to be created, then access it
    juce::Timer::callAfterDelay(100, [this]()
    {
        if (auto* topLevelComponent = getTopLevelComponent())
        {
            if (auto* documentWindow = dynamic_cast<juce::DocumentWindow*>(topLevelComponent))
            {
                documentWindow->setUsingNativeTitleBar(true);
            }
        }
    });

    startTimerHz(60); // 60 fps refresh
}

AudioVisualizerEditor::~AudioVisualizerEditor()
{
}

void AudioVisualizerEditor::mouseDoubleClick(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    toggleEffectPicker();
}

void AudioVisualizerEditor::mouseDrag(const juce::MouseEvent& event)
{
    if (!effectPickerVisible)
        return;

    auto pos = event.getPosition();

    // Check if dragging from effect box area
    for (int i = 0; i < 4; i++)
    {
        if (effectBoxBounds[i].contains(pos))
        {
            // Start drag operation
            EffectType effectType;
            switch (i)
            {
                case 0: effectType = EffectType::BinaryFlash; break;
                case 1: effectType = EffectType::Flutter; break;
                case 2: effectType = EffectType::Starfield; break;
                case 3: effectType = EffectType::FrequencyLine; break;
                default: return;
            }

            // Pack effect type and color into drag data
            juce::var effectData;
            auto dragInfo = new juce::DynamicObject();
            dragInfo->setProperty("effectType", static_cast<int>(effectType));
            dragInfo->setProperty("color", selectedColor.toString());
            effectData = juce::var(dragInfo);

            // Create drag image showing the effect with selected color
            juce::Image dragImage(juce::Image::ARGB, 100, 40, true);
            juce::Graphics dragG(dragImage);
            dragG.fillAll(juce::Colour(60, 60, 65));

            // Show color preview
            dragG.setColour(selectedColor);
            dragG.fillRoundedRectangle(5, 5, 20, 30, 3.0f);

            // Effect name
            dragG.setColour(juce::Colours::white);
            dragG.setFont(14.0f);
            const char* names[] = { "Binary Flash", "Flutter", "Starfield", "Frequency Line" };
            juce::Rectangle<int> textArea(30, 0, 70, 40);
            dragG.drawText(names[i], textArea, juce::Justification::centredLeft);

            startDragging(effectData, this, dragImage, true);
            break;
        }
    }
}

void AudioVisualizerEditor::mouseDown(const juce::MouseEvent& event)
{
    auto pos = event.getPosition();

    // Check if clicking on color picker
    if (effectPickerVisible && colorPickerBounds.contains(pos))
    {
        // Create a container for color selector + hex input
        struct ColourPickerWithHex : public juce::Component,
                                     public juce::ChangeListener,
                                     public juce::TextEditor::Listener
        {
            ColourPickerWithHex(AudioVisualizerEditor& ed, juce::Colour initialColor)
                : editor(ed)
            {
                colourSelector = std::make_unique<juce::ColourSelector>(
                    juce::ColourSelector::showColourAtTop |
                    juce::ColourSelector::showColourspace);

                colourSelector->setCurrentColour(initialColor);
                colourSelector->addChangeListener(this);
                addAndMakeVisible(*colourSelector);

                // Hex input field
                hexInput = std::make_unique<juce::TextEditor>();
                hexInput->setText(initialColor.toDisplayString(false), false);
                hexInput->setJustification(juce::Justification::centred);
                hexInput->addListener(this);
                addAndMakeVisible(*hexInput);

                // Label
                addAndMakeVisible(hexLabel);
                hexLabel.setText("Hex:", juce::dontSendNotification);
                hexLabel.setJustificationType(juce::Justification::centredRight);
                hexLabel.setColour(juce::Label::textColourId, juce::Colours::white);

                setSize(300, 350);
            }

            void resized() override
            {
                auto bounds = getLocalBounds();
                auto hexArea = bounds.removeFromBottom(40).reduced(10, 5);

                hexLabel.setBounds(hexArea.removeFromLeft(40));
                hexInput->setBounds(hexArea);

                colourSelector->setBounds(bounds);
            }

            void changeListenerCallback(juce::ChangeBroadcaster* source) override
            {
                if (auto* cs = dynamic_cast<juce::ColourSelector*>(source))
                {
                    auto newColor = cs->getCurrentColour();
                    editor.selectedColor = newColor;
                    hexInput->setText(newColor.toDisplayString(false), false);
                    editor.repaint();
                }
            }

            void textEditorTextChanged(juce::TextEditor& ed) override
            {
                auto hexString = ed.getText().trim();

                // Add # if not present
                if (!hexString.startsWith("#"))
                    hexString = "#" + hexString;

                // Only try to parse if it looks like a valid hex color
                if (hexString.length() >= 7) // #RRGGBB minimum
                {
                    auto newColor = juce::Colour::fromString(hexString);
                    colourSelector->setCurrentColour(newColor, juce::dontSendNotification);
                    editor.selectedColor = newColor;
                    editor.repaint();
                }
            }

            void textEditorReturnKeyPressed(juce::TextEditor& ed) override
            {
                textEditorTextChanged(ed);
            }

            void textEditorFocusLost(juce::TextEditor& ed) override
            {
                textEditorTextChanged(ed);
            }

            AudioVisualizerEditor& editor;
            std::unique_ptr<juce::ColourSelector> colourSelector;
            std::unique_ptr<juce::TextEditor> hexInput;
            juce::Label hexLabel;
        };

        auto picker = std::make_unique<ColourPickerWithHex>(*this, selectedColor);
        juce::CallOutBox::launchAsynchronously(std::move(picker), colorPickerBounds, this);
        return;
    }

    // Check if clicking on light mode toggle
    if (effectPickerVisible && lightModeToggleBounds.contains(pos))
    {
        lightMode = !lightMode;
        repaint();
        return;
    }

    if (event.mods.isPopupMenu())  // Right-click
    {
        // Determine which section was clicked
        if (topSectionBounds.contains(pos))
            showFrequencyMenu(SectionID::Top);
        else if (bottomLeftSectionBounds.contains(pos))
            showFrequencyMenu(SectionID::BottomLeft);
        else if (bottomRightSectionBounds.contains(pos))
            showFrequencyMenu(SectionID::BottomRight);
    }
}

void AudioVisualizerEditor::toggleEffectPicker()
{
    effectPickerVisible = !effectPickerVisible;

    static constexpr int menuWidth = 220;

    if (effectPickerVisible)
    {
        // Expand window to show effect picker
        setSize(getWidth() + menuWidth, getHeight());
    }
    else
    {
        // Collapse back to original size
        setSize(getWidth() - menuWidth, getHeight());
    }

    repaint();
}

// Drag and drop implementation
bool AudioVisualizerEditor::isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails& details)
{
    return details.description.isInt() || details.description.isObject();  // Check if it's an effect type
}

void AudioVisualizerEditor::itemDragEnter(const juce::DragAndDropTarget::SourceDetails& details)
{
    juce::ignoreUnused(details);
    isDraggingEffect = true;
}

void AudioVisualizerEditor::itemDragMove(const juce::DragAndDropTarget::SourceDetails& details)
{
    auto pos = details.localPosition;

    // Determine which section is being hovered
    if (topSectionBounds.contains(pos.toInt()))
        hoveredSection = SectionID::Top;
    else if (bottomLeftSectionBounds.contains(pos.toInt()))
        hoveredSection = SectionID::BottomLeft;
    else if (bottomRightSectionBounds.contains(pos.toInt()))
        hoveredSection = SectionID::BottomRight;

    repaint();
}

void AudioVisualizerEditor::itemDragExit(const juce::DragAndDropTarget::SourceDetails& details)
{
    juce::ignoreUnused(details);
    isDraggingEffect = false;
    repaint();
}

void AudioVisualizerEditor::itemDropped(const juce::DragAndDropTarget::SourceDetails& details)
{
    isDraggingEffect = false;

    if (details.description.isObject())
    {
        auto* obj = details.description.getDynamicObject();
        if (obj != nullptr)
        {
            int effectTypeInt = obj->getProperty("effectType");
            juce::String colorString = obj->getProperty("color").toString();

            auto effectType = static_cast<EffectType>(effectTypeInt);
            auto color = juce::Colour::fromString(colorString);

            applyEffectToSection(hoveredSection, effectType, color);
        }
    }
    else if (details.description.isInt())
    {
        // Fallback for old format
        auto effectType = static_cast<EffectType>(static_cast<int>(details.description));
        applyEffectToSection(hoveredSection, effectType, juce::Colours::white);
    }

    repaint();
}

void AudioVisualizerEditor::showFrequencyMenu(SectionID section)
{
    // Get current frequency range for this section
    FrequencyRange currentRange;
    switch (section)
    {
        case SectionID::Top: currentRange = topEffect.frequencyRange; break;
        case SectionID::BottomLeft: currentRange = bottomLeftEffect.frequencyRange; break;
        case SectionID::BottomRight: currentRange = bottomRightEffect.frequencyRange; break;
    }

    juce::PopupMenu menu;
    menu.addItem(1, "Sub-Bass (20-60 Hz)", true, currentRange == FrequencyRange::SubBass);
    menu.addItem(2, "Bass (60-250 Hz)", true, currentRange == FrequencyRange::Bass);
    menu.addItem(3, "Low-Mids (250-500 Hz)", true, currentRange == FrequencyRange::LowMids);
    menu.addItem(4, "Mids (500-2000 Hz)", true, currentRange == FrequencyRange::Mids);
    menu.addItem(5, "High-Mids (2000-4000 Hz)", true, currentRange == FrequencyRange::HighMids);
    menu.addItem(6, "Highs (4000-8000 Hz)", true, currentRange == FrequencyRange::Highs);
    menu.addItem(7, "Very Highs (8000-20000 Hz)", true, currentRange == FrequencyRange::VeryHighs);
    menu.addItem(8, "Kick Transient (50-90 Hz)", true, currentRange == FrequencyRange::KickTransient);
    menu.addItem(9, "Full Spectrum", true, currentRange == FrequencyRange::FullSpectrum);

    menu.addSeparator();
    menu.addItem(10, "Show Values", true, showDebugValues);

    // Show input source info
    menu.addSeparator();
    AudioVisualizerProcessor::PanelID panelID;
    switch (section)
    {
        case SectionID::Top: panelID = AudioVisualizerProcessor::Top; break;
        case SectionID::BottomLeft: panelID = AudioVisualizerProcessor::BottomLeft; break;
        case SectionID::BottomRight: panelID = AudioVisualizerProcessor::BottomRight; break;
        default: panelID = AudioVisualizerProcessor::Main; break;
    }

    bool hasSidechain = audioProcessor.hasSidechainInput(panelID);
    juce::String inputSource = hasSidechain ? "Input: Sidechain" : "Input: Main Track";
    menu.addItem(11, inputSource, false, false);  // Non-clickable info item

    menu.showMenuAsync(juce::PopupMenu::Options(), [this, section](int result)
    {
        if (result > 0)
        {
            if (result == 10)
            {
                // Toggle show debug values
                showDebugValues = !showDebugValues;
                repaint();
                return;
            }

            FrequencyRange range;
            switch (result)
            {
                case 1: range = FrequencyRange::SubBass; break;
                case 2: range = FrequencyRange::Bass; break;
                case 3: range = FrequencyRange::LowMids; break;
                case 4: range = FrequencyRange::Mids; break;
                case 5: range = FrequencyRange::HighMids; break;
                case 6: range = FrequencyRange::Highs; break;
                case 7: range = FrequencyRange::VeryHighs; break;
                case 8: range = FrequencyRange::KickTransient; break;
                case 9: range = FrequencyRange::FullSpectrum; break;
                default: return;
            }

            // Update frequency range for the section
            switch (section)
            {
                case SectionID::Top:
                    topEffect.frequencyRange = range;
                    break;
                case SectionID::BottomLeft:
                    bottomLeftEffect.frequencyRange = range;
                    break;
                case SectionID::BottomRight:
                    bottomRightEffect.frequencyRange = range;
                    break;
            }
        }
    });
}

void AudioVisualizerEditor::applyEffectToSection(SectionID section, EffectType effect, juce::Colour color)
{
    switch (section)
    {
        case SectionID::Top:
            topEffect.type = effect;
            topEffect.effectColor = color;
            if (effect == EffectType::Starfield)
                topStarfield.initStars();  // Reinitialize this section's starfield
            break;

        case SectionID::BottomLeft:
            bottomLeftEffect.type = effect;
            bottomLeftEffect.effectColor = color;
            if (effect == EffectType::Starfield)
                bottomLeftStarfield.initStars();
            break;

        case SectionID::BottomRight:
            bottomRightEffect.type = effect;
            bottomRightEffect.effectColor = color;
            if (effect == EffectType::Starfield)
                bottomRightStarfield.initStars();
            break;
    }

    repaint();
}

float AudioVisualizerEditor::getFrequencyValue(FrequencyRange range, AudioVisualizerProcessor::PanelID panel)
{
    switch (range)
    {
        case FrequencyRange::SubBass:
            return audioProcessor.getSubBassEnergy(panel);
        case FrequencyRange::Bass:
            return audioProcessor.getBassEnergy(panel);
        case FrequencyRange::LowMids:
            return audioProcessor.getLowMidEnergy(panel);
        case FrequencyRange::Mids:
            return audioProcessor.getMidEnergy(panel);
        case FrequencyRange::HighMids:
            return audioProcessor.getHighMidEnergy(panel);
        case FrequencyRange::Highs:
            return audioProcessor.getHighEnergy(panel);
        case FrequencyRange::VeryHighs:
            return audioProcessor.getVeryHighEnergy(panel);
        case FrequencyRange::KickTransient:
            return audioProcessor.getKickTransient(panel);
        case FrequencyRange::FullSpectrum:
            return audioProcessor.getFullSpectrum(panel);
        default:
            return 0.0f;
    }
}

void AudioVisualizerEditor::paint (juce::Graphics& g)
{
    static constexpr int menuWidth = 220;

    auto fullBounds = getLocalBounds();

    // If effect picker is visible, reserve space for it on the right
    auto visualizerBounds = fullBounds;
    if (effectPickerVisible)
    {
        visualizerBounds.removeFromRight(menuWidth);
    }

    // Store section bounds for drag & drop - use visualizer bounds, not full bounds
    topSectionBounds = visualizerBounds;
    topSectionBounds.setHeight(visualizerBounds.getHeight() / 2);

    auto bottomHalf = visualizerBounds;
    bottomHalf.setY(visualizerBounds.getHeight() / 2);
    bottomHalf.setHeight(visualizerBounds.getHeight() / 2);

    bottomLeftSectionBounds = bottomHalf;
    bottomLeftSectionBounds.setWidth(bottomHalf.getWidth() / 2);

    bottomRightSectionBounds = bottomHalf;
    bottomRightSectionBounds.setX(bottomHalf.getWidth() / 2);
    bottomRightSectionBounds.setWidth(bottomHalf.getWidth() / 2);

    // Get frequency values for each section based on their configured ranges
    float topRawValue = getFrequencyValue(topEffect.frequencyRange, AudioVisualizerProcessor::Top);
    float bottomLeftRawValue = getFrequencyValue(bottomLeftEffect.frequencyRange, AudioVisualizerProcessor::BottomLeft);
    float bottomRightRawValue = getFrequencyValue(bottomRightEffect.frequencyRange, AudioVisualizerProcessor::BottomRight);

    // Check if playing
    bool isPlaying = audioProcessor.isPlaying();

    if (isPlaying)
    {
        // Playing: apply temporal smoothing for Flutter effects
        smoothedTopValue = smoothedTopValue * visualSmoothingFactor + topRawValue * (1.0f - visualSmoothingFactor);
        smoothedBottomLeftValue = smoothedBottomLeftValue * visualSmoothingFactor + bottomLeftRawValue * (1.0f - visualSmoothingFactor);
        smoothedBottomRightValue = smoothedBottomRightValue * visualSmoothingFactor + bottomRightRawValue * (1.0f - visualSmoothingFactor);
    }
    else
    {
        // Paused: gradually fade to black for Flutter/BinaryFlash, but starfield continues
        smoothedTopValue *= pauseFadeFactor;
        smoothedBottomLeftValue *= pauseFadeFactor;
        smoothedBottomRightValue *= pauseFadeFactor;
    }

    // Define regions - use visualizer bounds, not full bounds
    auto topHalf = visualizerBounds.removeFromTop(visualizerBounds.getHeight() / 2);     // Top 50% for mids
    auto bottomLeft = visualizerBounds.removeFromLeft(visualizerBounds.getWidth() / 2);  // Bottom-left 25% for bass
    auto bottomRight = visualizerBounds;                                                  // Bottom-right 25% for highs

    // TOP HALF: Render based on effect config
    {
        // Clip all rendering to panel bounds
        juce::Graphics::ScopedSaveState sectionClip(g);
        g.reduceClipRegion(topHalf);

        // Render effect based on topEffect.type
        if (topEffect.type == EffectType::Flutter)
        {
            // Flutter: Gradual fade from background to effect color
            auto bgColor = lightMode ? juce::Colours::white : juce::Colours::black;
            auto finalColor = bgColor.interpolatedWith(topEffect.effectColor, smoothedTopValue);
            g.setColour(finalColor);
            g.fillRect(topHalf);
        }
        else if (topEffect.type == EffectType::BinaryFlash)
        {
            // Binary flash: Use configured color or background
            float threshold = 0.3f;
            bool shouldFlash = smoothedTopValue > threshold;
            if (lightMode)
                g.setColour(shouldFlash ? topEffect.effectColor : juce::Colours::white);
            else
                g.setColour(shouldFlash ? topEffect.effectColor : juce::Colours::black);
            g.fillRect(topHalf);
        }
        else if (topEffect.type == EffectType::Starfield)
        {
            // Starfield on top section (independent instance)
            // Binary mode only for kick transient, continuous for all others
            bool isBinaryMode = (topEffect.frequencyRange == FrequencyRange::KickTransient);

            // Background color based on mode
            g.setColour(lightMode ? juce::Colours::white : juce::Colours::black);
            g.fillRect(topHalf);

            float centerX = topHalf.getX() + topHalf.getWidth() * 0.5f;
            float centerY = topHalf.getY() + topHalf.getHeight() * 0.5f;

            topStarfield.update(topRawValue, isBinaryMode);
            topStarfield.draw(g, topHalf, centerX, centerY, lightMode, topEffect.effectColor);
        }
        else if (topEffect.type == EffectType::FrequencyLine)
        {
            // Background color based on mode
            g.setColour(lightMode ? juce::Colours::white : juce::Colours::black);
            g.fillRect(topHalf);

            // Get frequency range for this section
            float minFreq = 20.0f, maxFreq = 20000.0f;
            switch (topEffect.frequencyRange)
            {
                case FrequencyRange::SubBass: minFreq = 20.0f; maxFreq = 60.0f; break;
                case FrequencyRange::Bass: minFreq = 60.0f; maxFreq = 250.0f; break;
                case FrequencyRange::LowMids: minFreq = 250.0f; maxFreq = 500.0f; break;
                case FrequencyRange::Mids: minFreq = 500.0f; maxFreq = 2000.0f; break;
                case FrequencyRange::HighMids: minFreq = 2000.0f; maxFreq = 4000.0f; break;
                case FrequencyRange::Highs: minFreq = 4000.0f; maxFreq = 8000.0f; break;
                case FrequencyRange::VeryHighs: minFreq = 8000.0f; maxFreq = 20000.0f; break;
                case FrequencyRange::KickTransient: minFreq = 50.0f; maxFreq = 90.0f; break;
                case FrequencyRange::FullSpectrum: minFreq = 20.0f; maxFreq = 20000.0f; break;
            }

            // Get spectrum data with moderate points for spiky but smooth curves
            std::vector<float> spectrum;
            int numPoints = 50; // More points for spikier appearance
            audioProcessor.getSpectrumForRange(minFreq, maxFreq, spectrum, numPoints, AudioVisualizerProcessor::Top);

            // Initialize smoothing buffer if needed
            if (topSpectrumSmooth.size() != spectrum.size())
            {
                topSpectrumSmooth = spectrum;
            }

            // Apply light spatial smoothing for spiky but not jagged appearance
            std::vector<float> spatialSmoothed(spectrum.size());
            for (int i = 0; i < (int)spectrum.size(); ++i)
            {
                float sum = 0.0f;
                int count = 0;
                int windowSize = 2; // Smaller window for more spiky peaks

                for (int j = -windowSize; j <= windowSize; ++j)
                {
                    int idx = i + j;
                    if (idx >= 0 && idx < (int)spectrum.size())
                    {
                        sum += spectrum[idx];
                        count++;
                    }
                }
                spatialSmoothed[i] = sum / count;
            }

            // Apply temporal smoothing (smooth with previous frame)
            std::vector<float> smoothedSpectrum(spectrum.size());
            float temporalSmoothing = 0.96f; // Very high smoothing for fluid, gliding movement (96% previous, 4% new)
            for (int i = 0; i < (int)spectrum.size(); ++i)
            {
                smoothedSpectrum[i] = topSpectrumSmooth[i] * temporalSmoothing +
                                     spatialSmoothed[i] * (1.0f - temporalSmoothing);
                topSpectrumSmooth[i] = smoothedSpectrum[i]; // Store for next frame
            }

            // For kick transient mode, modulate spectrum by actual kick detection
            if (topEffect.frequencyRange == FrequencyRange::KickTransient)
            {
                float kickValue = audioProcessor.getKickTransient(AudioVisualizerProcessor::Top);
                for (int i = 0; i < (int)smoothedSpectrum.size(); ++i)
                {
                    smoothedSpectrum[i] *= kickValue;  // Only show high values during detected kicks
                }
            }

            // amplitudeGain for visual dynamics
            float amplitudeGain = 1.5f;

            // Draw smooth frequency line - only draw points within bounds
            if (!smoothedSpectrum.empty() && smoothedSpectrum.size() > 1)
            {
                // Save graphics state and clip to panel bounds
                juce::Graphics::ScopedSaveState saveState(g);
                g.reduceClipRegion(topHalf);

                // Adaptive normalization - find current peak and smooth it over time
                float currentPeak = 0.0001f;
                for (int i = 0; i < (int)smoothedSpectrum.size(); ++i)
                {
                    if (smoothedSpectrum[i] > currentPeak)
                        currentPeak = smoothedSpectrum[i];
                }

                // Smooth the peak: fast attack, medium release for adaptive scaling
                if (currentPeak > topSpectrumPeak)
                    topSpectrumPeak = topSpectrumPeak * 0.3f + currentPeak * 0.7f;  // Fast attack
                else
                    topSpectrumPeak = topSpectrumPeak * 0.92f + currentPeak * 0.08f;  // Medium release

                // Normalize by smoothed peak to keep visualization in reasonable range
                float normalizationFactor = topSpectrumPeak * amplitudeGain;
                for (int i = 0; i < (int)smoothedSpectrum.size(); ++i)
                {
                    smoothedSpectrum[i] /= normalizationFactor;
                }

                juce::Path linePath;
                float xScale = (float)topHalf.getWidth() / (float)(smoothedSpectrum.size() - 1);

                // Calculate first Y with clamping
                float firstY = topHalf.getBottom() - (smoothedSpectrum[0] * amplitudeGain * topHalf.getHeight());
                firstY = juce::jlimit((float)topHalf.getY(), (float)topHalf.getBottom(), firstY);
                linePath.startNewSubPath((float)topHalf.getX(), firstY);

                // Draw smooth cubic curves with all points clamped to bounds
                for (int i = 1; i < (int)smoothedSpectrum.size(); ++i)
                {
                    float x = topHalf.getX() + (i * xScale);
                    float y = topHalf.getBottom() - (smoothedSpectrum[i] * amplitudeGain * topHalf.getHeight());
                    y = juce::jlimit((float)topHalf.getY(), (float)topHalf.getBottom(), y);

                    float prevX = topHalf.getX() + ((i - 1) * xScale);
                    float prevY = topHalf.getBottom() - (smoothedSpectrum[i - 1] * amplitudeGain * topHalf.getHeight());
                    prevY = juce::jlimit((float)topHalf.getY(), (float)topHalf.getBottom(), prevY);

                    // Control points for smooth curves - also clamped
                    float ctrlX1 = prevX + (x - prevX) * 0.25f;
                    float ctrlY1 = prevY;
                    float ctrlX2 = prevX + (x - prevX) * 0.75f;
                    float ctrlY2 = y;

                    linePath.cubicTo(ctrlX1, ctrlY1, ctrlX2, ctrlY2, x, y);
                }

                // Draw ultra-thin, rounded line
                if (!linePath.isEmpty())
                {
                    g.setColour(topEffect.effectColor);
                    juce::PathStrokeType strokeType(1.15f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
                    g.strokePath(linePath, strokeType);
                }
            }
        }

        // Draw blue outline if hovering during drag
        if (isDraggingEffect && hoveredSection == SectionID::Top)
        {
            g.setColour(juce::Colour(0, 122, 255).withAlpha(0.8f));
            g.drawRect(topHalf.toFloat(), 4.0f);
        }
    }

    // BOTTOM LEFT: Render based on effect config
    {
        // Clip all rendering to panel bounds
        juce::Graphics::ScopedSaveState sectionClip(g);
        g.reduceClipRegion(bottomLeft);

        // Render effect based on bottomLeftEffect.type
        if (bottomLeftEffect.type == EffectType::Flutter)
        {
            // Flutter: Gradual fade from background to effect color
            auto bgColor = lightMode ? juce::Colours::white : juce::Colours::black;
            auto finalColor = bgColor.interpolatedWith(bottomLeftEffect.effectColor, smoothedBottomLeftValue);
            g.setColour(finalColor);
            g.fillRect(bottomLeft);
        }
        else if (bottomLeftEffect.type == EffectType::BinaryFlash)
        {
            // Binary flash: Use configured color or background
            float threshold = 0.3f;
            bool shouldFlash = smoothedBottomLeftValue > threshold;
            if (lightMode)
                g.setColour(shouldFlash ? bottomLeftEffect.effectColor : juce::Colours::white);
            else
                g.setColour(shouldFlash ? bottomLeftEffect.effectColor : juce::Colours::black);
            g.fillRect(bottomLeft);
        }
        else if (bottomLeftEffect.type == EffectType::Starfield)
        {
            // Starfield on bottom-left section (independent instance)
            // Binary mode only for kick transient, continuous for all others
            bool isBinaryMode = (bottomLeftEffect.frequencyRange == FrequencyRange::KickTransient);

            // Background color based on mode
            g.setColour(lightMode ? juce::Colours::white : juce::Colours::black);
            g.fillRect(bottomLeft);

            float centerX = bottomLeft.getX() + bottomLeft.getWidth() * 0.5f;
            float centerY = bottomLeft.getY() + bottomLeft.getHeight() * 0.5f;

            bottomLeftStarfield.update(bottomLeftRawValue, isBinaryMode);
            bottomLeftStarfield.draw(g, bottomLeft, centerX, centerY, lightMode, bottomLeftEffect.effectColor);
        }
        else if (bottomLeftEffect.type == EffectType::FrequencyLine)
        {
            // Background color based on mode
            g.setColour(lightMode ? juce::Colours::white : juce::Colours::black);
            g.fillRect(bottomLeft);

            // Get frequency range for this section
            float minFreq = 20.0f, maxFreq = 20000.0f;
            switch (bottomLeftEffect.frequencyRange)
            {
                case FrequencyRange::SubBass: minFreq = 20.0f; maxFreq = 60.0f; break;
                case FrequencyRange::Bass: minFreq = 60.0f; maxFreq = 250.0f; break;
                case FrequencyRange::LowMids: minFreq = 250.0f; maxFreq = 500.0f; break;
                case FrequencyRange::Mids: minFreq = 500.0f; maxFreq = 2000.0f; break;
                case FrequencyRange::HighMids: minFreq = 2000.0f; maxFreq = 4000.0f; break;
                case FrequencyRange::Highs: minFreq = 4000.0f; maxFreq = 8000.0f; break;
                case FrequencyRange::VeryHighs: minFreq = 8000.0f; maxFreq = 20000.0f; break;
                case FrequencyRange::KickTransient: minFreq = 50.0f; maxFreq = 90.0f; break;
                case FrequencyRange::FullSpectrum: minFreq = 20.0f; maxFreq = 20000.0f; break;
            }

            // Get spectrum data with moderate points for spiky but smooth curves
            std::vector<float> spectrum;
            int numPoints = 50; // More points for spikier appearance
            audioProcessor.getSpectrumForRange(minFreq, maxFreq, spectrum, numPoints, AudioVisualizerProcessor::BottomLeft);

            // Initialize smoothing buffer if needed
            if (bottomLeftSpectrumSmooth.size() != spectrum.size())
            {
                bottomLeftSpectrumSmooth = spectrum;
            }

            // Apply light spatial smoothing for spiky but not jagged appearance
            std::vector<float> spatialSmoothed(spectrum.size());
            for (int i = 0; i < (int)spectrum.size(); ++i)
            {
                float sum = 0.0f;
                int count = 0;
                int windowSize = 2; // Smaller window for more spiky peaks

                for (int j = -windowSize; j <= windowSize; ++j)
                {
                    int idx = i + j;
                    if (idx >= 0 && idx < (int)spectrum.size())
                    {
                        sum += spectrum[idx];
                        count++;
                    }
                }
                spatialSmoothed[i] = sum / count;
            }

            // Apply temporal smoothing (smooth with previous frame)
            std::vector<float> smoothedSpectrum(spectrum.size());
            float temporalSmoothing = 0.96f; // Very high smoothing for fluid, gliding movement (96% previous, 4% new)
            for (int i = 0; i < (int)spectrum.size(); ++i)
            {
                smoothedSpectrum[i] = bottomLeftSpectrumSmooth[i] * temporalSmoothing +
                                     spatialSmoothed[i] * (1.0f - temporalSmoothing);
                bottomLeftSpectrumSmooth[i] = smoothedSpectrum[i]; // Store for next frame
            }

            // For kick transient mode, modulate spectrum by actual kick detection
            if (bottomLeftEffect.frequencyRange == FrequencyRange::KickTransient)
            {
                float kickValue = audioProcessor.getKickTransient(AudioVisualizerProcessor::BottomLeft);
                for (int i = 0; i < (int)smoothedSpectrum.size(); ++i)
                {
                    smoothedSpectrum[i] *= kickValue;  // Only show high values during detected kicks
                }
            }

            // amplitudeGain for visual dynamics
            float amplitudeGain = 1.5f;

            // Draw smooth frequency line - only draw points within bounds
            if (!smoothedSpectrum.empty() && smoothedSpectrum.size() > 1)
            {
                // Save graphics state and clip to panel bounds
                juce::Graphics::ScopedSaveState saveState(g);
                g.reduceClipRegion(bottomLeft);

                // Adaptive normalization - find current peak and smooth it over time
                float currentPeak = 0.0001f;
                for (int i = 0; i < (int)smoothedSpectrum.size(); ++i)
                {
                    if (smoothedSpectrum[i] > currentPeak)
                        currentPeak = smoothedSpectrum[i];
                }

                // Smooth the peak: fast attack (0.3), slow release (0.995)
                if (currentPeak > bottomLeftSpectrumPeak)
                    bottomLeftSpectrumPeak = bottomLeftSpectrumPeak * 0.3f + currentPeak * 0.7f;  // Fast attack
                else
                    bottomLeftSpectrumPeak = bottomLeftSpectrumPeak * 0.995f + currentPeak * 0.005f;  // Slow release

                // Normalize by smoothed peak to keep visualization in reasonable range
                float normalizationFactor = bottomLeftSpectrumPeak * amplitudeGain;
                for (int i = 0; i < (int)smoothedSpectrum.size(); ++i)
                {
                    smoothedSpectrum[i] /= normalizationFactor;
                }

                juce::Path linePath;
                float xScale = (float)bottomLeft.getWidth() / (float)(smoothedSpectrum.size() - 1);

                // Calculate first Y with clamping
                float firstY = bottomLeft.getBottom() - (smoothedSpectrum[0] * amplitudeGain * bottomLeft.getHeight());
                firstY = juce::jlimit((float)bottomLeft.getY(), (float)bottomLeft.getBottom(), firstY);
                linePath.startNewSubPath((float)bottomLeft.getX(), firstY);

                // Draw smooth cubic curves with all points clamped to bounds
                for (int i = 1; i < (int)smoothedSpectrum.size(); ++i)
                {
                    float x = bottomLeft.getX() + (i * xScale);
                    float y = bottomLeft.getBottom() - (smoothedSpectrum[i] * amplitudeGain * bottomLeft.getHeight());
                    y = juce::jlimit((float)bottomLeft.getY(), (float)bottomLeft.getBottom(), y);

                    float prevX = bottomLeft.getX() + ((i - 1) * xScale);
                    float prevY = bottomLeft.getBottom() - (smoothedSpectrum[i - 1] * amplitudeGain * bottomLeft.getHeight());
                    prevY = juce::jlimit((float)bottomLeft.getY(), (float)bottomLeft.getBottom(), prevY);

                    // Control points for smooth curves - also clamped
                    float ctrlX1 = prevX + (x - prevX) * 0.25f;
                    float ctrlY1 = prevY;
                    float ctrlX2 = prevX + (x - prevX) * 0.75f;
                    float ctrlY2 = y;

                    linePath.cubicTo(ctrlX1, ctrlY1, ctrlX2, ctrlY2, x, y);
                }

                // Draw ultra-thin, rounded line
                if (!linePath.isEmpty())
                {
                    g.setColour(bottomLeftEffect.effectColor);
                    juce::PathStrokeType strokeType(1.15f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
                    g.strokePath(linePath, strokeType);
                }
            }
        }

        // Draw blue outline if hovering during drag
        if (isDraggingEffect && hoveredSection == SectionID::BottomLeft)
        {
            g.setColour(juce::Colour(0, 122, 255).withAlpha(0.8f));
            g.drawRect(bottomLeft.toFloat(), 4.0f);
        }
    }

    // BOTTOM RIGHT: Render based on effect config
    {
        // Clip all rendering to panel bounds
        juce::Graphics::ScopedSaveState sectionClip(g);
        g.reduceClipRegion(bottomRight);

        // Render effect based on bottomRightEffect.type
        if (bottomRightEffect.type == EffectType::Flutter)
        {
            // Flutter: Gradual fade from background to effect color
            auto bgColor = lightMode ? juce::Colours::white : juce::Colours::black;
            auto finalColor = bgColor.interpolatedWith(bottomRightEffect.effectColor, smoothedBottomRightValue);
            g.setColour(finalColor);
            g.fillRect(bottomRight);
        }
        else if (bottomRightEffect.type == EffectType::BinaryFlash)
        {
            // Binary flash: Use configured color or background
            float threshold = 0.3f;
            bool shouldFlash = smoothedBottomRightValue > threshold;
            if (lightMode)
                g.setColour(shouldFlash ? bottomRightEffect.effectColor : juce::Colours::white);
            else
                g.setColour(shouldFlash ? bottomRightEffect.effectColor : juce::Colours::black);
            g.fillRect(bottomRight);
        }
        else if (bottomRightEffect.type == EffectType::Starfield)
        {
            // Starfield on bottom-right section (independent instance)
            // Binary mode only for kick transient, continuous for all others
            bool isBinaryMode = (bottomRightEffect.frequencyRange == FrequencyRange::KickTransient);

            // Background color based on mode
            g.setColour(lightMode ? juce::Colours::white : juce::Colours::black);
            g.fillRect(bottomRight);

            float centerX = bottomRight.getX() + bottomRight.getWidth() * 0.5f;
            float centerY = bottomRight.getY() + bottomRight.getHeight() * 0.5f;

            bottomRightStarfield.update(bottomRightRawValue, isBinaryMode);
            bottomRightStarfield.draw(g, bottomRight, centerX, centerY, lightMode, bottomRightEffect.effectColor);
        }
        else if (bottomRightEffect.type == EffectType::FrequencyLine)
        {
            // Background color based on mode
            g.setColour(lightMode ? juce::Colours::white : juce::Colours::black);
            g.fillRect(bottomRight);

            // Get frequency range for this section
            float minFreq = 20.0f, maxFreq = 20000.0f;
            switch (bottomRightEffect.frequencyRange)
            {
                case FrequencyRange::SubBass: minFreq = 20.0f; maxFreq = 60.0f; break;
                case FrequencyRange::Bass: minFreq = 60.0f; maxFreq = 250.0f; break;
                case FrequencyRange::LowMids: minFreq = 250.0f; maxFreq = 500.0f; break;
                case FrequencyRange::Mids: minFreq = 500.0f; maxFreq = 2000.0f; break;
                case FrequencyRange::HighMids: minFreq = 2000.0f; maxFreq = 4000.0f; break;
                case FrequencyRange::Highs: minFreq = 4000.0f; maxFreq = 8000.0f; break;
                case FrequencyRange::VeryHighs: minFreq = 8000.0f; maxFreq = 20000.0f; break;
                case FrequencyRange::KickTransient: minFreq = 50.0f; maxFreq = 90.0f; break;
                case FrequencyRange::FullSpectrum: minFreq = 20.0f; maxFreq = 20000.0f; break;
            }

            // Get spectrum data with moderate points for spiky but smooth curves
            std::vector<float> spectrum;
            int numPoints = 50; // More points for spikier appearance
            audioProcessor.getSpectrumForRange(minFreq, maxFreq, spectrum, numPoints, AudioVisualizerProcessor::BottomRight);

            // Initialize smoothing buffer if needed
            if (bottomRightSpectrumSmooth.size() != spectrum.size())
            {
                bottomRightSpectrumSmooth = spectrum;
            }

            // Apply light spatial smoothing for spiky but not jagged appearance
            std::vector<float> spatialSmoothed(spectrum.size());
            for (int i = 0; i < (int)spectrum.size(); ++i)
            {
                float sum = 0.0f;
                int count = 0;
                int windowSize = 2; // Smaller window for more spiky peaks

                for (int j = -windowSize; j <= windowSize; ++j)
                {
                    int idx = i + j;
                    if (idx >= 0 && idx < (int)spectrum.size())
                    {
                        sum += spectrum[idx];
                        count++;
                    }
                }
                spatialSmoothed[i] = sum / count;
            }

            // Apply temporal smoothing (smooth with previous frame)
            std::vector<float> smoothedSpectrum(spectrum.size());
            float temporalSmoothing = 0.96f; // Very high smoothing for fluid, gliding movement (96% previous, 4% new)
            for (int i = 0; i < (int)spectrum.size(); ++i)
            {
                smoothedSpectrum[i] = bottomRightSpectrumSmooth[i] * temporalSmoothing +
                                     spatialSmoothed[i] * (1.0f - temporalSmoothing);
                bottomRightSpectrumSmooth[i] = smoothedSpectrum[i]; // Store for next frame
            }

            // For kick transient mode, modulate spectrum by actual kick detection
            if (bottomRightEffect.frequencyRange == FrequencyRange::KickTransient)
            {
                float kickValue = audioProcessor.getKickTransient(AudioVisualizerProcessor::BottomRight);
                for (int i = 0; i < (int)smoothedSpectrum.size(); ++i)
                {
                    smoothedSpectrum[i] *= kickValue;  // Only show high values during detected kicks
                }
            }

            // amplitudeGain for visual dynamics
            float amplitudeGain = 1.5f;

            // Draw smooth frequency line - only draw points within bounds
            if (!smoothedSpectrum.empty() && smoothedSpectrum.size() > 1)
            {
                // Save graphics state and clip to panel bounds
                juce::Graphics::ScopedSaveState saveState(g);
                g.reduceClipRegion(bottomRight);

                // Adaptive normalization - find current peak and smooth it over time
                float currentPeak = 0.0001f;
                for (int i = 0; i < (int)smoothedSpectrum.size(); ++i)
                {
                    if (smoothedSpectrum[i] > currentPeak)
                        currentPeak = smoothedSpectrum[i];
                }

                // Smooth the peak: fast attack (0.3), slow release (0.995)
                if (currentPeak > bottomRightSpectrumPeak)
                    bottomRightSpectrumPeak = bottomRightSpectrumPeak * 0.3f + currentPeak * 0.7f;  // Fast attack
                else
                    bottomRightSpectrumPeak = bottomRightSpectrumPeak * 0.995f + currentPeak * 0.005f;  // Slow release

                // Normalize by smoothed peak to keep visualization in reasonable range
                float normalizationFactor = bottomRightSpectrumPeak * amplitudeGain;
                for (int i = 0; i < (int)smoothedSpectrum.size(); ++i)
                {
                    smoothedSpectrum[i] /= normalizationFactor;
                }

                juce::Path linePath;
                float xScale = (float)bottomRight.getWidth() / (float)(smoothedSpectrum.size() - 1);

                // Calculate first Y with clamping
                float firstY = bottomRight.getBottom() - (smoothedSpectrum[0] * amplitudeGain * bottomRight.getHeight());
                firstY = juce::jlimit((float)bottomRight.getY(), (float)bottomRight.getBottom(), firstY);
                linePath.startNewSubPath((float)bottomRight.getX(), firstY);

                // Draw smooth cubic curves with all points clamped to bounds
                for (int i = 1; i < (int)smoothedSpectrum.size(); ++i)
                {
                    float x = bottomRight.getX() + (i * xScale);
                    float y = bottomRight.getBottom() - (smoothedSpectrum[i] * amplitudeGain * bottomRight.getHeight());
                    y = juce::jlimit((float)bottomRight.getY(), (float)bottomRight.getBottom(), y);

                    float prevX = bottomRight.getX() + ((i - 1) * xScale);
                    float prevY = bottomRight.getBottom() - (smoothedSpectrum[i - 1] * amplitudeGain * bottomRight.getHeight());
                    prevY = juce::jlimit((float)bottomRight.getY(), (float)bottomRight.getBottom(), prevY);

                    // Control points for smooth curves - also clamped
                    float ctrlX1 = prevX + (x - prevX) * 0.25f;
                    float ctrlY1 = prevY;
                    float ctrlX2 = prevX + (x - prevX) * 0.75f;
                    float ctrlY2 = y;

                    linePath.cubicTo(ctrlX1, ctrlY1, ctrlX2, ctrlY2, x, y);
                }

                // Draw ultra-thin, rounded line
                if (!linePath.isEmpty())
                {
                    g.setColour(bottomRightEffect.effectColor);
                    juce::PathStrokeType strokeType(1.15f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
                    g.strokePath(linePath, strokeType);
                }
            }
        }

        // Draw blue outline if hovering during drag
        if (isDraggingEffect && hoveredSection == SectionID::BottomRight)
        {
            g.setColour(juce::Colour(0, 122, 255).withAlpha(0.8f));
            g.drawRect(bottomRight.toFloat(), 4.0f);
        }
    }

    // Draw effect picker panel if visible
    if (effectPickerVisible)
    {
        // Menu takes full height and is positioned on the right side
        auto pickerBounds = fullBounds.removeFromRight(menuWidth);
        auto menuFullBounds = pickerBounds;  // Save original bounds for instructions

        // Background with subtle border
        g.setColour(juce::Colour(35, 35, 40));
        g.fillRect(pickerBounds);

        // Left border separator
        g.setColour(juce::Colour(60, 60, 65));
        g.fillRect(juce::Rectangle<int>(pickerBounds.getX(), pickerBounds.getY(), 1, pickerBounds.getHeight()));

        // Title area
        auto titleArea = pickerBounds.removeFromTop(60);
        g.setColour(juce::Colour(45, 45, 50));
        g.fillRect(titleArea);

        g.setColour(juce::Colours::white);
        g.setFont(20.0f);
        g.drawText("Effects", titleArea, juce::Justification::centred);

        // Instructions directly under title
        auto instrArea = pickerBounds.removeFromTop(65);
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.setFont(10.0f);
        g.drawText("Drag effects onto sections\nRight-click sections for frequency\nDouble-click to close",
                   instrArea, juce::Justification::centred);

        // Color picker
        auto colorPickerArea = pickerBounds.removeFromTop(50);
        colorPickerArea = colorPickerArea.reduced(20, 10);

        // Color picker label
        g.setColour(juce::Colours::white);
        g.setFont(11.0f);
        auto labelArea = colorPickerArea.removeFromLeft(50);
        g.drawText("Color:", labelArea, juce::Justification::centredLeft);

        // Color picker swatch (clickable)
        colorPickerBounds = colorPickerArea.reduced(0, 5);
        colorPickerBounds.setWidth(colorPickerBounds.getHeight());  // Make it square

        // Draw color swatch
        g.setColour(selectedColor);
        g.fillRoundedRectangle(colorPickerBounds.toFloat(), 4.0f);

        // Border
        g.setColour(juce::Colour(70, 70, 75));
        g.drawRoundedRectangle(colorPickerBounds.toFloat(), 4.0f, 1.5f);

        pickerBounds.removeFromTop(10);  // Spacing

        // Draw available effects (draggable)
        auto effectArea = pickerBounds.reduced(15, 20);
        int yPos = effectArea.getY();

        struct EffectInfo { const char* name; EffectType type; };
        EffectInfo effects[] = {
            { "Binary Flash", EffectType::BinaryFlash },
            { "Flutter", EffectType::Flutter },
            { "Starfield", EffectType::Starfield },
            { "Frequency Line", EffectType::FrequencyLine }
        };

        for (int i = 0; i < 4; i++)
        {
            auto effectBox = juce::Rectangle<int>(effectArea.getX(), yPos, effectArea.getWidth(), 65);

            // Effect box background with hover effect
            g.setColour(juce::Colour(50, 50, 55));
            g.fillRoundedRectangle(effectBox.toFloat(), 6.0f);

            // Subtle border
            g.setColour(juce::Colour(70, 70, 75));
            g.drawRoundedRectangle(effectBox.toFloat(), 6.0f, 1.0f);

            // Effect name
            g.setColour(juce::Colours::white);
            g.setFont(15.0f);
            g.drawText(effects[i].name, effectBox, juce::Justification::centred);

            // Store effect box bounds for drag detection
            effectBoxBounds[i] = effectBox;

            yPos += 80;
        }

        // Light/Dark mode toggle at bottom
        auto toggleArea = menuFullBounds;
        toggleArea.removeFromBottom(25);  // Padding from bottom
        toggleArea = toggleArea.removeFromBottom(35);
        toggleArea = toggleArea.reduced(40, 0);

        // Toggle switch (simple with icons)
        auto switchArea = toggleArea;
        switchArea.setHeight(24);
        switchArea.setWidth(48);
        switchArea = switchArea.withCentre(toggleArea.getCentre());
        lightModeToggleBounds = switchArea;

        // Switch background
        g.setColour(juce::Colour(60, 60, 65));
        g.fillRoundedRectangle(switchArea.toFloat(), 12.0f);

        // Moon icon (left side)
        auto moonArea = switchArea.removeFromLeft(24).reduced(5);
        g.setColour(lightMode ? juce::Colours::white.withAlpha(0.3f) : juce::Colours::white.withAlpha(0.7f));
        g.fillEllipse(moonArea.toFloat());
        // Add shadow to moon (crescent effect)
        auto moonShadow = moonArea.reduced(2).translated(3, 0);
        g.setColour(juce::Colour(60, 60, 65));
        g.fillEllipse(moonShadow.toFloat());

        // Sun icon (right side)
        auto sunArea = switchArea.removeFromRight(24).reduced(6);
        g.setColour(lightMode ? juce::Colours::white.withAlpha(0.9f) : juce::Colours::white.withAlpha(0.3f));
        g.fillEllipse(sunArea.toFloat());
        // Sun rays (simple lines)
        float sunCenterX = sunArea.getCentreX();
        float sunCenterY = sunArea.getCentreY();
        float rayLength = 3.0f;
        for (int i = 0; i < 8; ++i)
        {
            float angle = i * juce::MathConstants<float>::pi / 4.0f;
            float x1 = sunCenterX + std::cos(angle) * 7.0f;
            float y1 = sunCenterY + std::sin(angle) * 7.0f;
            float x2 = sunCenterX + std::cos(angle) * (7.0f + rayLength);
            float y2 = sunCenterY + std::sin(angle) * (7.0f + rayLength);
            g.drawLine(x1, y1, x2, y2, 1.0f);
        }
    }

    // Debug: Show frequency range and value for each section (if enabled)
    // For VST3/AU always show when enabled, for Standalone only when audio loaded
    bool shouldShowDebug = showDebugValues &&
                          (audioProcessor.wrapperType != juce::AudioProcessor::wrapperType_Standalone ||
                           audioProcessor.isAudioLoaded());
    if (shouldShowDebug)
    {
        // Use appropriate color based on light/dark mode
        auto textColor = lightMode ? juce::Colours::black.withAlpha(0.8f) : juce::Colours::white.withAlpha(0.8f);
        g.setColour(textColor);
        g.setFont(12.0f);

        auto getFreqRangeName = [](FrequencyRange range) -> juce::String {
            switch (range) {
                case FrequencyRange::SubBass: return "Sub-Bass";
                case FrequencyRange::Bass: return "Bass";
                case FrequencyRange::LowMids: return "Low-Mids";
                case FrequencyRange::Mids: return "Mids";
                case FrequencyRange::HighMids: return "High-Mids";
                case FrequencyRange::Highs: return "Highs";
                case FrequencyRange::VeryHighs: return "Very Highs";
                case FrequencyRange::KickTransient: return "Kick";
                case FrequencyRange::FullSpectrum: return "Full";
                default: return "Unknown";
            }
        };

        // Top section debug
        juce::String topDebug = getFreqRangeName(topEffect.frequencyRange) + ": " +
                                juce::String(topRawValue, 2);
        g.drawText(topDebug, topSectionBounds.reduced(10).removeFromTop(20),
                   juce::Justification::topLeft);

        // Bottom left debug
        juce::String blDebug = getFreqRangeName(bottomLeftEffect.frequencyRange) + ": " +
                               juce::String(bottomLeftRawValue, 2);
        g.drawText(blDebug, bottomLeftSectionBounds.reduced(10).removeFromTop(20),
                   juce::Justification::topLeft);

        // Bottom right debug
        juce::String brDebug = getFreqRangeName(bottomRightEffect.frequencyRange) + ": " +
                               juce::String(bottomRightRawValue, 2);
        g.drawText(brDebug, bottomRightSectionBounds.reduced(10).removeFromTop(20),
                   juce::Justification::topLeft);
    }

    // Only show text for Standalone version when audio is not loaded or just loaded
    if (audioProcessor.wrapperType == juce::AudioProcessor::wrapperType_Standalone &&
        (!audioProcessor.isAudioLoaded() || showLoadedMessage))
    {
        g.setColour(juce::Colours::white);
        g.setFont(16.0f);

        if (showLoadedMessage)
        {
            g.drawText("Audio loaded! Press SPACE to play", getLocalBounds(), juce::Justification::centred);
        }
        else
        {
            g.drawText(statusMessage, getLocalBounds(), juce::Justification::centred);

            // Instructions
            g.setFont(14.0f);
            g.setColour(juce::Colours::white.withAlpha(0.7f));
            auto instructionsArea = getLocalBounds().reduced(20);
            instructionsArea.setY(getHeight() - 100);
            g.drawText("Supported formats: WAV, AIFF, MP3, FLAC",
                       instructionsArea,
                       juce::Justification::centredBottom);
        }
    }
}

void AudioVisualizerEditor::resized()
{
}

bool AudioVisualizerEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    // Check if any file is an audio file
    for (const auto& filename : files)
    {
        if (filename.endsWithIgnoreCase(".wav") ||
            filename.endsWithIgnoreCase(".aif") ||
            filename.endsWithIgnoreCase(".aiff") ||
            filename.endsWithIgnoreCase(".mp3") ||
            filename.endsWithIgnoreCase(".flac") ||
            filename.endsWithIgnoreCase(".ogg") ||
            filename.endsWithIgnoreCase(".m4a"))
        {
            return true;
        }
    }
    return false;
}

void AudioVisualizerEditor::filesDropped (const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused(x, y);

    if (files.size() > 0)
    {
        juce::File audioFile(files[0]);
        audioProcessor.loadAudioFile(audioFile);

        if (audioProcessor.isAudioLoaded())
        {
            showLoadedMessage = true;
            loadedMessageTimer = 120; // Show for 2 seconds at 60fps
            statusMessage = "Audio loaded: " + audioFile.getFileName();
        }
        else
        {
            statusMessage = "Failed to load audio file";
        }

        repaint();
    }
}

bool AudioVisualizerEditor::keyPressed (const juce::KeyPress& key)
{
    // Spacebar to play/pause
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

    // 'O' to open file dialog
    if (key.getKeyCode() == 'O' || key.getKeyCode() == 'o')
    {
        auto chooser = std::make_shared<juce::FileChooser>("Select an audio file to visualize...",
                                                            juce::File::getSpecialLocation(juce::File::userMusicDirectory),
                                                            "*.wav;*.aiff;*.aif;*.mp3;*.flac;*.ogg;*.m4a");

        auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

        chooser->launchAsync(flags, [this, chooser](const juce::FileChooser& fc)
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

void AudioVisualizerEditor::timerCallback()
{
    // Handle loaded message timeout
    if (showLoadedMessage)
    {
        loadedMessageTimer--;
        if (loadedMessageTimer <= 0)
        {
            showLoadedMessage = false;
        }
    }

    // Trigger repaint for animation
    repaint();
}
