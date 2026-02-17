#include "PluginProcessor.h"
#include "PluginEditor.h"

AudioVisualizerProcessor::AudioVisualizerProcessor()
    : AudioProcessor (BusesProperties()
                     .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                     .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    formatManager.registerBasicFormats();
    fftData.fill(0.0f);
}

AudioVisualizerProcessor::~AudioVisualizerProcessor()
{
}

const juce::String AudioVisualizerProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AudioVisualizerProcessor::acceptsMidi() const
{
    return false;
}

bool AudioVisualizerProcessor::producesMidi() const
{
    return false;
}

bool AudioVisualizerProcessor::isMidiEffect() const
{
    return false;
}

double AudioVisualizerProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AudioVisualizerProcessor::getNumPrograms()
{
    return 1;
}

int AudioVisualizerProcessor::getCurrentProgram()
{
    return 0;
}

void AudioVisualizerProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String AudioVisualizerProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void AudioVisualizerProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void AudioVisualizerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    transportSource.prepareToPlay(samplesPerBlock, sampleRate);
}

void AudioVisualizerProcessor::releaseResources()
{
    transportSource.releaseResources();
}

bool AudioVisualizerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void AudioVisualizerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    // Runtime check: use loaded audio only for Standalone builds
    bool usingLoadedAudio = (wrapperType == wrapperType_Standalone);

    if (usingLoadedAudio)
    {
        // Standalone: Clear buffer and use loaded audio file
        buffer.clear();

        if (audioLoaded && playing)
        {
            juce::AudioSourceChannelInfo channelInfo(buffer);
            transportSource.getNextAudioBlock(channelInfo);
        }
    }
    // For VST3/AU: Don't clear buffer, audio passes through

    // Always perform FFT analysis on whatever audio is in the buffer
    if (buffer.getNumSamples() > 0)
    {
        // Perform FFT analysis
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const float* channelData = buffer.getReadPointer(channel);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                // Add to FFT buffer
                fftData[fftDataPos] = channelData[i];
                fftDataPos++;

                // When we have enough samples, perform FFT
                if (fftDataPos >= fftSize)
                {
                    fftDataPos = 0;

                    // Apply windowing function
                    window.multiplyWithWindowingTable(fftData.data(), fftSize);

                    // Perform FFT
                    fft.performFrequencyOnlyForwardTransform(fftData.data());

                    // Analyze frequency bands
                    float sampleRate = getSampleRate();
                    float binWidth = sampleRate / fftSize;

                    // Calculate frequency ranges for all bands
                    int subBassStart = static_cast<int>(20.0f / binWidth);
                    int subBassEnd = static_cast<int>(60.0f / binWidth);
                    int bassStart = subBassEnd;
                    int bassEnd = static_cast<int>(250.0f / binWidth);
                    int kickStart = static_cast<int>(50.0f / binWidth);   // Kick-specific range (narrower)
                    int kickEnd = static_cast<int>(90.0f / binWidth);     // 50-90 Hz = tight kick fundamentals
                    int lowMidStart = bassEnd;
                    int lowMidEnd = static_cast<int>(500.0f / binWidth);
                    int midStart = lowMidEnd;
                    int midEnd = static_cast<int>(2000.0f / binWidth);
                    int highMidStart = midEnd;
                    int highMidEnd = static_cast<int>(4000.0f / binWidth);
                    int highStart = highMidEnd;
                    int highEnd = static_cast<int>(8000.0f / binWidth);
                    int veryHighStart = highEnd;
                    int veryHighEnd = static_cast<int>(20000.0f / binWidth);

                    // Sum energy in each band
                    float subBass = 0.0f, bass = 0.0f, kick = 0.0f;
                    float lowMid = 0.0f, mid = 0.0f, highMid = 0.0f;
                    float high = 0.0f, veryHigh = 0.0f, full = 0.0f;

                    for (int bin = subBassStart; bin < subBassEnd && bin < fftSize / 2; ++bin)
                        subBass += fftData[bin];
                    for (int bin = bassStart; bin < bassEnd && bin < fftSize / 2; ++bin)
                        bass += fftData[bin];
                    for (int bin = kickStart; bin < kickEnd && bin < fftSize / 2; ++bin)
                        kick += fftData[bin];  // Separate kick detection band
                    for (int bin = lowMidStart; bin < lowMidEnd && bin < fftSize / 2; ++bin)
                        lowMid += fftData[bin];
                    for (int bin = midStart; bin < midEnd && bin < fftSize / 2; ++bin)
                        mid += fftData[bin];
                    for (int bin = highMidStart; bin < highMidEnd && bin < fftSize / 2; ++bin)
                        highMid += fftData[bin];
                    for (int bin = highStart; bin < highEnd && bin < fftSize / 2; ++bin)
                        high += fftData[bin];
                    for (int bin = veryHighStart; bin < veryHighEnd && bin < fftSize / 2; ++bin)
                        veryHigh += fftData[bin];
                    for (int bin = subBassStart; bin < veryHighEnd && bin < fftSize / 2; ++bin)
                        full += fftData[bin];

                    // Normalize by number of bins
                    subBass /= std::max(1, subBassEnd - subBassStart);
                    bass /= std::max(1, bassEnd - bassStart);
                    kick /= std::max(1, kickEnd - kickStart);
                    lowMid /= std::max(1, lowMidEnd - lowMidStart);
                    mid /= std::max(1, midEnd - midStart);
                    highMid /= std::max(1, highMidEnd - highMidStart);
                    high /= std::max(1, highEnd - highStart);
                    veryHigh /= std::max(1, veryHighEnd - veryHighStart);
                    full /= std::max(1, veryHighEnd - subBassStart);

                    // Initial scaling for frequency response
                    subBass *= 0.4f;   // Sub-bass is very strong
                    bass *= 0.5f;      // Bass is already strong
                    kick *= 0.5f;      // Kick in same range as bass
                    lowMid *= 1.5f;    // Slight boost
                    mid *= 2.0f;       // Boost mids
                    highMid *= 3.0f;   // More boost for high-mids
                    high *= 5.0f;      // Boost highs significantly
                    veryHigh *= 8.0f;  // Very highs need most boost
                    full *= 1.0f;      // Full spectrum already balanced

                    // Adaptive normalization - track running averages
                    subBassAverage = subBassAverage * averageSmoothingFactor + subBass * (1.0f - averageSmoothingFactor);
                    bassAverage = bassAverage * averageSmoothingFactor + bass * (1.0f - averageSmoothingFactor);
                    lowMidAverage = lowMidAverage * averageSmoothingFactor + lowMid * (1.0f - averageSmoothingFactor);
                    midAverage = midAverage * averageSmoothingFactor + mid * (1.0f - averageSmoothingFactor);
                    highMidAverage = highMidAverage * averageSmoothingFactor + highMid * (1.0f - averageSmoothingFactor);
                    highAverage = highAverage * averageSmoothingFactor + high * (1.0f - averageSmoothingFactor);
                    veryHighAverage = veryHighAverage * averageSmoothingFactor + veryHigh * (1.0f - averageSmoothingFactor);
                    fullSpectrumAverage = fullSpectrumAverage * averageSmoothingFactor + full * (1.0f - averageSmoothingFactor);

                    // Track kick average separately for normalized kick detection
                    float kickAverage = bassAverage;  // Use bass average as baseline for kick

                    // Ensure minimum threshold to avoid division by zero
                    float subBassNorm = std::max(subBassAverage, minAverageThreshold);
                    float bassNorm = std::max(bassAverage, minAverageThreshold);
                    float lowMidNorm = std::max(lowMidAverage, minAverageThreshold);
                    float midNorm = std::max(midAverage, minAverageThreshold);
                    float highMidNorm = std::max(highMidAverage, minAverageThreshold);
                    float highNorm = std::max(highAverage, minAverageThreshold);
                    float veryHighNorm = std::max(veryHighAverage, minAverageThreshold);
                    float kickNorm = std::max(kickAverage, minAverageThreshold);
                    float fullNorm = std::max(fullSpectrumAverage, minAverageThreshold);

                    // Normalize current values by running average
                    float subBassNormalized = (subBass / subBassNorm) * 0.5f;
                    float bassNormalized = (bass / bassNorm) * 0.5f;
                    float kickNormalized = (kick / kickNorm) * 0.5f;
                    float lowMidNormalized = (lowMid / lowMidNorm) * 0.5f;
                    float midNormalized = (mid / midNorm) * 0.5f;
                    float highMidNormalized = (highMid / highMidNorm) * 0.5f;
                    float highNormalized = (high / highNorm) * 0.5f;
                    float veryHighNormalized = (veryHigh / veryHighNorm) * 0.5f;
                    float fullNormalized = (full / fullNorm) * 0.5f;

                    // Store results with clamping
                    subBassEnergy.store(juce::jlimit(0.0f, 1.0f, subBassNormalized));
                    bassEnergy.store(juce::jlimit(0.0f, 1.0f, bassNormalized));
                    lowMidEnergy.store(juce::jlimit(0.0f, 1.0f, lowMidNormalized));
                    midEnergy.store(juce::jlimit(0.0f, 1.0f, midNormalized));
                    highMidEnergy.store(juce::jlimit(0.0f, 1.0f, highMidNormalized));
                    highEnergy.store(juce::jlimit(0.0f, 1.0f, highNormalized));
                    veryHighEnergy.store(juce::jlimit(0.0f, 1.0f, veryHighNormalized));
                    fullSpectrum.store(juce::jlimit(0.0f, 1.0f, fullNormalized));

                    // Kick transient detection - VERY selective criteria
                    float kickChange = kickNormalized - previousBassForKick;

                    // Multiple strict conditions must ALL be met:
                    bool sharpIncrease = kickChange > 0.35f;               // Very sharp increase (3x more selective)
                    bool strongEnergy = kickNormalized > 0.45f;            // Must be strong kick (2x higher threshold)
                    bool significantlyAboveAverage = kickNormalized > 1.5f; // Must be well above average
                    bool cooldownReady = kickCooldown <= 0;

                    // Only trigger if ALL conditions met
                    if (sharpIncrease &&
                        strongEnergy &&
                        significantlyAboveAverage &&
                        cooldownReady)
                    {
                        kickDecay = 1.0f;  // Trigger flash
                        kickCooldown = 20; // Even longer cooldown (~300ms minimum between kicks)
                    }

                    // Decay kick flash quickly (mimics transient duration)
                    kickDecay *= 0.75f;  // Faster decay for sharper flash

                    // Countdown cooldown
                    if (kickCooldown > 0)
                        kickCooldown--;

                    kickTransient.store(juce::jlimit(0.0f, 1.0f, kickDecay));
                    previousBassForKick = kickNormalized;  // Store normalized value for next comparison
                }
            }
        }
    }

    // Only decay for standalone when not playing
    if (wrapperType == wrapperType_Standalone && (!playing || !audioLoaded))
    {
        // Decay when stopped
        subBassEnergy.store(subBassEnergy.load() * 0.95f);
        bassEnergy.store(bassEnergy.load() * 0.95f);
        lowMidEnergy.store(lowMidEnergy.load() * 0.95f);
        midEnergy.store(midEnergy.load() * 0.95f);
        highMidEnergy.store(highMidEnergy.load() * 0.95f);
        highEnergy.store(highEnergy.load() * 0.95f);
        veryHighEnergy.store(veryHighEnergy.load() * 0.95f);
        kickTransient.store(kickTransient.load() * 0.95f);
        fullSpectrum.store(fullSpectrum.load() * 0.95f);
    }
}

bool AudioVisualizerProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* AudioVisualizerProcessor::createEditor()
{
    return new AudioVisualizerEditor (*this);
}

void AudioVisualizerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ignoreUnused (destData);
}

void AudioVisualizerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::ignoreUnused (data, sizeInBytes);
}

void AudioVisualizerProcessor::loadAudioFile(const juce::File& file)
{
    auto* reader = formatManager.createReaderFor(file);

    if (reader != nullptr)
    {
        auto newSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
        transportSource.setSource(newSource.get(), 0, nullptr, reader->sampleRate);
        readerSource.reset(newSource.release());
        audioLoaded = true;
        playing = false; // Reset playing state
        transportSource.setPosition(0.0);

        // Reset adaptive normalization for new song
        bassAverage = 0.0f;
        midAverage = 0.0f;
        highAverage = 0.0f;
    }
}

void AudioVisualizerProcessor::setPlaying(bool shouldPlay)
{
    // Just set the flag - don't touch transportSource
    // The transport keeps running but we control output via the playing flag in processBlock
    playing = shouldPlay;

    // Only start the transport if it's not already playing
    if (playing && !transportSource.isPlaying())
    {
        transportSource.start();
    }
    // NOTE: We deliberately DON'T call transportSource.stop() because it blocks
    // Instead, processBlock will simply not read from the transport when paused
    // The transport continues running silently in the background
}

void AudioVisualizerProcessor::getSpectrumForRange(float minFreq, float maxFreq, std::vector<float>& output, int numPoints) const
{
    output.clear();
    output.resize(numPoints, 0.0f);

    // Get sample rate from transport source
    double sampleRate = 44100.0; // Default
    if (readerSource && readerSource->getAudioFormatReader())
        sampleRate = readerSource->getAudioFormatReader()->sampleRate;

    // Calculate bin range for this frequency range
    float binWidth = (float)sampleRate / (float)fftSize;
    int minBin = (int)(minFreq / binWidth);
    int maxBin = (int)(maxFreq / binWidth);

    // Clamp to valid range
    minBin = juce::jlimit(0, fftSize / 2, minBin);
    maxBin = juce::jlimit(0, fftSize / 2, maxBin);

    if (maxBin <= minBin)
        return;

    // Sample the FFT bins and map to output points
    for (int i = 0; i < numPoints; ++i)
    {
        float ratio = (float)i / (float)(numPoints - 1);
        int bin = minBin + (int)(ratio * (maxBin - minBin));

        if (bin < fftSize / 2)
        {
            // Get magnitude from FFT data (complex to magnitude)
            float real = fftData[bin * 2];
            float imag = fftData[bin * 2 + 1];
            float magnitude = std::sqrt(real * real + imag * imag);

            // Normalize and scale for display
            output[i] = juce::jlimit(0.0f, 1.0f, magnitude * 0.1f);
        }
    }
}

// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioVisualizerProcessor();
}
