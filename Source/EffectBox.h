#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EffectSystem.h"

// Draggable effect box component
class EffectBox : public juce::Component
{
public:
    EffectBox(EffectType type, const juce::String& name)
        : effectType(type), effectName(name)
    {
    }

    void paint(juce::Graphics& g) override
    {
        // Box background
        g.setColour(juce::Colour(60, 60, 65));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);

        // Effect name
        g.setColour(juce::Colours::white);
        g.setFont(16.0f);
        g.drawText(effectName, getLocalBounds(), juce::Justification::centred);
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        // Start drag operation
        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this))
        {
            auto dragImage = createComponentSnapshot(getLocalBounds(), true, 1.0f);

            juce::var effectData;
            effectData = static_cast<int>(effectType);

            container->startDragging(effectData, this, dragImage, true);
        }
    }

    EffectType getEffectType() const { return effectType; }

private:
    EffectType effectType;
    juce::String effectName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectBox)
};
