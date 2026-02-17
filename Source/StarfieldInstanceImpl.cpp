#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

void AudioVisualizerEditor::StarfieldInstance::initStars()
{
    stars.clear();

    for (int i = 0; i < 200; ++i)
    {
        Star star;
        star.x = random.nextFloat() * 2000.0f - 1000.0f;
        star.y = random.nextFloat() * 2000.0f - 1000.0f;
        star.z = random.nextFloat() * 2000.0f + 100.0f;
        star.prevX = star.x;
        star.prevY = star.y;
        star.prevZ = star.z;
        stars.push_back(star);
    }
}

void AudioVisualizerEditor::StarfieldInstance::update(float value, bool isBinaryMode)
{
    // Target speeds - ALWAYS animating
    float baseSpeed = 2.0f;
    float maxSpeed = 80.0f;
    float targetSpeed;

    if (isBinaryMode)
    {
        // Binary mode (kick transient): Either zooming or idle, based on threshold
        float threshold = 0.3f;
        bool isActive = value > threshold;
        targetSpeed = isActive ? maxSpeed : baseSpeed;

        // Smooth interpolation
        if (isActive)
        {
            // Quick attack
            currentSpeed = currentSpeed * 0.1f + targetSpeed * 0.9f;
        }
        else
        {
            // Ease back, clamped to minimum
            float newSpeed = currentSpeed * 0.92f + targetSpeed * 0.08f;
            currentSpeed = std::max(newSpeed, baseSpeed);
        }
    }
    else
    {
        // Continuous mode: Speed proportional to frequency volume
        // Map value (0.0-1.0) to speed range (baseSpeed-maxSpeed)
        targetSpeed = baseSpeed + (value * (maxSpeed - baseSpeed));

        // Clamp to valid range
        targetSpeed = juce::jlimit(baseSpeed, maxSpeed, targetSpeed);

        // Smooth interpolation towards target
        currentSpeed = currentSpeed * 0.85f + targetSpeed * 0.15f;

        // Ensure never goes below base speed
        currentSpeed = std::max(currentSpeed, baseSpeed);
    }

    // Update star positions
    for (auto& star : stars)
    {
        star.prevX = star.x;
        star.prevY = star.y;
        star.prevZ = star.z;

        star.z -= currentSpeed;

        if (star.z < 1.0f)
        {
            star.x = random.nextFloat() * 2000.0f - 1000.0f;
            star.y = random.nextFloat() * 2000.0f - 1000.0f;
            star.z = 2000.0f;
            star.prevZ = star.z;
        }
    }
}

void AudioVisualizerEditor::StarfieldInstance::draw(juce::Graphics& g,
                                                     const juce::Rectangle<int>& bounds,
                                                     float centerX,
                                                     float centerY,
                                                     bool lightMode,
                                                     juce::Colour starColor)
{
    g.saveState();
    g.reduceClipRegion(bounds);

    float scale = 200.0f;

    // Show streaks when speed is significantly above base (threshold for visual effect)
    float baseSpeed = 2.0f;
    float streakThreshold = 15.0f;  // Show streaks when speed > this
    bool showStreaks = currentSpeed > streakThreshold;

    for (const auto& star : stars)
    {
        float screenX = centerX + (star.x / star.z) * scale;
        float screenY = centerY + (star.y / star.z) * scale;

        if (screenX < bounds.getX() - 20 || screenX > bounds.getRight() + 20 ||
            screenY < bounds.getY() - 20 || screenY > bounds.getBottom() + 20)
            continue;

        float distFromCenterX = screenX - centerX;
        float distFromCenterY = screenY - centerY;
        float distFromCenter = std::sqrt(distFromCenterX * distFromCenterX +
                                         distFromCenterY * distFromCenterY);

        // Use configured star color with subtle gradient based on distance
        float maxRadius = bounds.getWidth() * 0.5f;
        float colorBlend = juce::jlimit(0.0f, 1.0f, distFromCenter / maxRadius);

        // Create a slightly darker/lighter version for gradient effect
        juce::Colour gradientColor = lightMode ?
            starColor.darker(0.3f) :
            starColor.darker(0.4f);

        juce::Colour finalStarColor = starColor.interpolatedWith(gradientColor, colorBlend * 0.5f);
        g.setColour(finalStarColor);

        if (showStreaks)
        {
            float minDistForStreak = bounds.getWidth() * 0.18f;

            if (distFromCenter > minDistForStreak)
            {
                float dirX = distFromCenterX;
                float dirY = distFromCenterY;
                float length = distFromCenter;

                if (length > 0.001f)
                {
                    dirX /= length;
                    dirY /= length;
                }

                // Streak length proportional to speed
                float speedFactor = juce::jlimit(0.0f, 1.0f, (currentSpeed - streakThreshold) / (80.0f - streakThreshold));
                float baseStreakLength = juce::jmap(star.z, 1.0f, 2000.0f, 50.0f, 12.0f);
                float streakLength = baseStreakLength * (0.3f + speedFactor * 0.7f);

                float startX = screenX - dirX * streakLength;
                float startY = screenY - dirY * streakLength;
                float thickness = juce::jmap(star.z, 1.0f, 2000.0f, 1.5f, 0.6f);

                juce::Line<float> line(startX, startY, screenX, screenY);
                g.drawLine(line, thickness);

                g.setColour(starColor.brighter(0.2f));
                float dotSize = juce::jmap(star.z, 1.0f, 2000.0f, 2.5f, 1.0f);
                g.fillEllipse(screenX - dotSize * 0.5f, screenY - dotSize * 0.5f, dotSize, dotSize);
            }
            else
            {
                float size = 1.2f;
                g.fillEllipse(screenX - size * 0.5f, screenY - size * 0.5f, size, size);
            }
        }
        else
        {
            float size = juce::jmap(star.z, 1.0f, 2000.0f, 2.5f, 1.0f);
            g.fillEllipse(screenX - size * 0.5f, screenY - size * 0.5f, size, size);
        }
    }

    g.restoreState();
}
