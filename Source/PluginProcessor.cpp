#include "PluginProcessor.h"
#include "PluginEditor.h"

AudioVisualizerProcessor::AudioVisualizerProcessor()
    : AudioProcessor (BusesProperties()
                     .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                     .withInput  ("Top Panel", juce::AudioChannelSet::stereo(), false)
                     .withInput  ("Bottom Left", juce::AudioChannelSet::stereo(), false)
                     .withInput  ("Bottom Right", juce::AudioChannelSet::stereo(), false)
                     .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    formatManager.registerBasicFormats();
    fftData.fill(0.0f);
    topFftData.fill(0.0f);
    bottomLeftFftData.fill(0.0f);
    bottomRightFftData.fill(0.0f);
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
    // Main output must be mono or stereo
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // Main input must be mono or stereo
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // Accept any configuration of sidechain buses
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

    // Always perform FFT analysis on main input bus only (not sidechains)
    auto mainInputBus = getBusBuffer(buffer, true, 0);
    if (mainInputBus.getNumSamples() > 0)
    {
        // Perform FFT analysis on main input only
        for (int channel = 0; channel < mainInputBus.getNumChannels(); ++channel)
        {
            const float* channelData = mainInputBus.getReadPointer(channel);

            for (int i = 0; i < mainInputBus.getNumSamples(); ++i)
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

                    // Simplified kick detection: focus on transient + reasonable energy
                    bool sharpTransient = kickChange > 0.2f;      // Sharp increase indicates transient
                    bool hasEnergy = kickNormalized > 0.3f;       // Has some energy (not total silence)
                    bool cooldownReady = kickCooldown <= 0;

                    // Trigger on transient with energy
                    if (sharpTransient && hasEnergy && cooldownReady)
                    {
                        kickDecay = 1.0f;  // Trigger flash
                        kickCooldown = 3; // Short cooldown to allow fast kick patterns
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

    // Reset sidechain flags
    topHasSidechain.store(false);
    bottomLeftHasSidechain.store(false);
    bottomRightHasSidechain.store(false);

    // Set default values for all panels (use main analysis)
    topSubBass.store(subBassEnergy.load());
    topBass.store(bassEnergy.load());
    topLowMid.store(lowMidEnergy.load());
    topMid.store(midEnergy.load());
    topHighMid.store(highMidEnergy.load());
    topHigh.store(highEnergy.load());
    topVeryHigh.store(veryHighEnergy.load());
    topKick.store(kickTransient.load());
    topFull.store(fullSpectrum.load());

    bottomLeftSubBass.store(subBassEnergy.load());
    bottomLeftBass.store(bassEnergy.load());
    bottomLeftLowMid.store(lowMidEnergy.load());
    bottomLeftMid.store(midEnergy.load());
    bottomLeftHighMid.store(highMidEnergy.load());
    bottomLeftHigh.store(highEnergy.load());
    bottomLeftVeryHigh.store(veryHighEnergy.load());
    bottomLeftKick.store(kickTransient.load());
    bottomLeftFull.store(fullSpectrum.load());

    bottomRightSubBass.store(subBassEnergy.load());
    bottomRightBass.store(bassEnergy.load());
    bottomRightLowMid.store(lowMidEnergy.load());
    bottomRightMid.store(midEnergy.load());
    bottomRightHighMid.store(highMidEnergy.load());
    bottomRightHigh.store(highEnergy.load());
    bottomRightVeryHigh.store(veryHighEnergy.load());
    bottomRightKick.store(kickTransient.load());
    bottomRightFull.store(fullSpectrum.load());

    // Analyze sidechain buses independently (check each bus individually)
    if (!usingLoadedAudio)
    {
        // Analyze Top Panel sidechain (bus 1)
        auto topBus = getBusBuffer(buffer, true, 1);
        if (topBus.getNumChannels() > 0 && topBus.getNumSamples() > 0)
        {
            float magnitude = topBus.getMagnitude(0, topBus.getNumSamples());
            if (magnitude > 0.0001f)
            {
                topHasSidechain.store(true);
            }

            // Always analyze if bus exists, even if silent
            analyzeSidechainBus(topBus, topFftData, topFftDataPos,
                               topSubBass, topBass, topLowMid, topMid,
                               topHighMid, topHigh, topVeryHigh, topKick, topFull);

            // Mix sidechain audio into main output so it's audible (only if has audio)
            if (magnitude > 0.0001f)
            {
                for (int ch = 0; ch < juce::jmin(topBus.getNumChannels(), 2); ++ch)
                {
                    buffer.addFrom(ch, 0, topBus, ch, 0, topBus.getNumSamples());
                }
            }
        }

        // Analyze Bottom Left sidechain (bus 2)
        auto bottomLeftBus = getBusBuffer(buffer, true, 2);
        if (bottomLeftBus.getNumChannels() > 0 && bottomLeftBus.getNumSamples() > 0)
        {
            float magnitude = bottomLeftBus.getMagnitude(0, bottomLeftBus.getNumSamples());
            if (magnitude > 0.0001f)  // Lower threshold
            {
                bottomLeftHasSidechain.store(true);
            }

            // Always analyze if bus exists, even if silent
            analyzeSidechainBus(bottomLeftBus, bottomLeftFftData, bottomLeftFftDataPos,
                               bottomLeftSubBass, bottomLeftBass, bottomLeftLowMid, bottomLeftMid,
                               bottomLeftHighMid, bottomLeftHigh, bottomLeftVeryHigh, bottomLeftKick, bottomLeftFull);

            // Mix sidechain audio into main output so it's audible (only if has audio)
            if (magnitude > 0.0001f)
            {
                for (int ch = 0; ch < juce::jmin(bottomLeftBus.getNumChannels(), 2); ++ch)
                {
                    buffer.addFrom(ch, 0, bottomLeftBus, ch, 0, bottomLeftBus.getNumSamples());
                }
            }
        }

        // Analyze Bottom Right sidechain (bus 3)
        auto bottomRightBus = getBusBuffer(buffer, true, 3);
        if (bottomRightBus.getNumChannels() > 0 && bottomRightBus.getNumSamples() > 0)
        {
            float magnitude = bottomRightBus.getMagnitude(0, bottomRightBus.getNumSamples());
            if (magnitude > 0.0001f)
            {
                bottomRightHasSidechain.store(true);
            }

            // Always analyze if bus exists, even if silent
            analyzeSidechainBus(bottomRightBus, bottomRightFftData, bottomRightFftDataPos,
                               bottomRightSubBass, bottomRightBass, bottomRightLowMid, bottomRightMid,
                               bottomRightHighMid, bottomRightHigh, bottomRightVeryHigh, bottomRightKick, bottomRightFull);

            // Mix sidechain audio into main output so it's audible (only if has audio)
            if (magnitude > 0.0001f)
            {
                for (int ch = 0; ch < juce::jmin(bottomRightBus.getNumChannels(), 2); ++ch)
                {
                    buffer.addFrom(ch, 0, bottomRightBus, ch, 0, bottomRightBus.getNumSamples());
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

void AudioVisualizerProcessor::getSpectrumForRange(float minFreq, float maxFreq, std::vector<float>& output, int numPoints, PanelID panel) const
{
    output.clear();
    output.resize(numPoints, 0.0f);

    // Select the appropriate FFT data array based on panel and whether it has active sidechain
    const std::array<float, fftSize * 2>* fftDataPtr = &fftData;
    if (panel == Top && topHasSidechain.load())
        fftDataPtr = &topFftData;
    else if (panel == BottomLeft && bottomLeftHasSidechain.load())
        fftDataPtr = &bottomLeftFftData;
    else if (panel == BottomRight && bottomRightHasSidechain.load())
        fftDataPtr = &bottomRightFftData;
    // Otherwise use main fftData

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
            float real = (*fftDataPtr)[bin * 2];
            float imag = (*fftDataPtr)[bin * 2 + 1];
            float magnitude = std::sqrt(real * real + imag * imag);

            // Normalize and scale for display
            output[i] = juce::jlimit(0.0f, 1.0f, magnitude * 0.1f);
        }
    }
}

void AudioVisualizerProcessor::analyzeSidechainBus(const juce::AudioBuffer<float>& bus,
                                                   std::array<float, fftSize * 2>& fftDataArray,
                                                   int& fftPos,
                                                   std::atomic<float>& subBass, std::atomic<float>& bass,
                                                   std::atomic<float>& lowMid, std::atomic<float>& mid,
                                                   std::atomic<float>& highMid, std::atomic<float>& high,
                                                   std::atomic<float>& veryHigh, std::atomic<float>& kick,
                                                   std::atomic<float>& full)
{
    if (bus.getNumSamples() == 0) return;

    for (int channel = 0; channel < bus.getNumChannels(); ++channel)
    {
        const float* channelData = bus.getReadPointer(channel);
        for (int i = 0; i < bus.getNumSamples(); ++i)
        {
            fftDataArray[fftPos] = channelData[i];
            fftPos++;

            if (fftPos >= fftSize)
            {
                fftPos = 0;
                window.multiplyWithWindowingTable(fftDataArray.data(), fftSize);
                fft.performFrequencyOnlyForwardTransform(fftDataArray.data());

                float sampleRate = getSampleRate();
                float binWidth = sampleRate / fftSize;

                int subBassStart = static_cast<int>(20.0f / binWidth);
                int subBassEnd = static_cast<int>(60.0f / binWidth);
                int bassStart = subBassEnd;
                int bassEnd = static_cast<int>(250.0f / binWidth);
                int kickStart = static_cast<int>(50.0f / binWidth);
                int kickEnd = static_cast<int>(90.0f / binWidth);
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

                float subBassVal = 0.0f, bassVal = 0.0f, kickVal = 0.0f;
                float lowMidVal = 0.0f, midVal = 0.0f, highMidVal = 0.0f;
                float highVal = 0.0f, veryHighVal = 0.0f, fullVal = 0.0f;

                for (int bin = subBassStart; bin < subBassEnd && bin < fftSize / 2; ++bin)
                    subBassVal += fftDataArray[bin];
                for (int bin = bassStart; bin < bassEnd && bin < fftSize / 2; ++bin)
                    bassVal += fftDataArray[bin];
                for (int bin = kickStart; bin < kickEnd && bin < fftSize / 2; ++bin)
                    kickVal += fftDataArray[bin];
                for (int bin = lowMidStart; bin < lowMidEnd && bin < fftSize / 2; ++bin)
                    lowMidVal += fftDataArray[bin];
                for (int bin = midStart; bin < midEnd && bin < fftSize / 2; ++bin)
                    midVal += fftDataArray[bin];
                for (int bin = highMidStart; bin < highMidEnd && bin < fftSize / 2; ++bin)
                    highMidVal += fftDataArray[bin];
                for (int bin = highStart; bin < highEnd && bin < fftSize / 2; ++bin)
                    highVal += fftDataArray[bin];
                for (int bin = veryHighStart; bin < veryHighEnd && bin < fftSize / 2; ++bin)
                    veryHighVal += fftDataArray[bin];
                for (int bin = subBassStart; bin < veryHighEnd && bin < fftSize / 2; ++bin)
                    fullVal += fftDataArray[bin];

                subBassVal /= std::max(1, subBassEnd - subBassStart);
                bassVal /= std::max(1, bassEnd - bassStart);
                kickVal /= std::max(1, kickEnd - kickStart);
                lowMidVal /= std::max(1, lowMidEnd - lowMidStart);
                midVal /= std::max(1, midEnd - midStart);
                highMidVal /= std::max(1, highMidEnd - highMidStart);
                highVal /= std::max(1, highEnd - highStart);
                veryHighVal /= std::max(1, veryHighEnd - veryHighStart);
                fullVal /= std::max(1, veryHighEnd - subBassStart);

                subBassVal *= 0.2f;
                bassVal *= 0.25f;
                kickVal *= 0.25f;
                lowMidVal *= 0.75f;
                midVal *= 1.0f;
                highMidVal *= 1.5f;
                highVal *= 2.5f;
                veryHighVal *= 4.0f;
                fullVal *= 0.5f;

                subBass.store(juce::jlimit(0.0f, 1.0f, subBassVal));
                bass.store(juce::jlimit(0.0f, 1.0f, bassVal));
                lowMid.store(juce::jlimit(0.0f, 1.0f, lowMidVal));
                mid.store(juce::jlimit(0.0f, 1.0f, midVal));
                highMid.store(juce::jlimit(0.0f, 1.0f, highMidVal));
                high.store(juce::jlimit(0.0f, 1.0f, highVal));
                veryHigh.store(juce::jlimit(0.0f, 1.0f, veryHighVal));
                kick.store(juce::jlimit(0.0f, 1.0f, kickVal));
                full.store(juce::jlimit(0.0f, 1.0f, fullVal));
            }
        }
    }
}

// Panel getters
float AudioVisualizerProcessor::getSubBassEnergy(PanelID panel) const {
    if (panel == Top) return topSubBass.load();
    if (panel == BottomLeft) return bottomLeftSubBass.load();
    if (panel == BottomRight) return bottomRightSubBass.load();
    return subBassEnergy.load();
}

float AudioVisualizerProcessor::getBassEnergy(PanelID panel) const {
    if (panel == Top) return topBass.load();
    if (panel == BottomLeft) return bottomLeftBass.load();
    if (panel == BottomRight) return bottomRightBass.load();
    return bassEnergy.load();
}

float AudioVisualizerProcessor::getLowMidEnergy(PanelID panel) const {
    if (panel == Top) return topLowMid.load();
    if (panel == BottomLeft) return bottomLeftLowMid.load();
    if (panel == BottomRight) return bottomRightLowMid.load();
    return lowMidEnergy.load();
}

float AudioVisualizerProcessor::getMidEnergy(PanelID panel) const {
    if (panel == Top) return topMid.load();
    if (panel == BottomLeft) return bottomLeftMid.load();
    if (panel == BottomRight) return bottomRightMid.load();
    return midEnergy.load();
}

float AudioVisualizerProcessor::getHighMidEnergy(PanelID panel) const {
    if (panel == Top) return topHighMid.load();
    if (panel == BottomLeft) return bottomLeftHighMid.load();
    if (panel == BottomRight) return bottomRightHighMid.load();
    return highMidEnergy.load();
}

float AudioVisualizerProcessor::getHighEnergy(PanelID panel) const {
    if (panel == Top) return topHigh.load();
    if (panel == BottomLeft) return bottomLeftHigh.load();
    if (panel == BottomRight) return bottomRightHigh.load();
    return highEnergy.load();
}

float AudioVisualizerProcessor::getVeryHighEnergy(PanelID panel) const {
    if (panel == Top) return topVeryHigh.load();
    if (panel == BottomLeft) return bottomLeftVeryHigh.load();
    if (panel == BottomRight) return bottomRightVeryHigh.load();
    return veryHighEnergy.load();
}

float AudioVisualizerProcessor::getKickTransient(PanelID panel) const {
    if (panel == Top) return topKick.load();
    if (panel == BottomLeft) return bottomLeftKick.load();
    if (panel == BottomRight) return bottomRightKick.load();
    return kickTransient.load();
}

float AudioVisualizerProcessor::getFullSpectrum(PanelID panel) const {
    if (panel == Top) return topFull.load();
    if (panel == BottomLeft) return bottomLeftFull.load();
    if (panel == BottomRight) return bottomRightFull.load();
    return fullSpectrum.load();
}

// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioVisualizerProcessor();
}
