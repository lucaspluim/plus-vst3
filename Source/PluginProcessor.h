#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

class AudioVisualizerProcessor : public juce::AudioProcessor
{
public:
    AudioVisualizerProcessor();
    ~AudioVisualizerProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Audio file handling
    void loadAudioFile(const juce::File& file);
    bool isAudioLoaded() const { return audioLoaded; }
    void setPlaying(bool shouldPlay);
    bool isPlaying() const
    {
        // VST3/AU always "playing" since audio flows from track
        // Standalone uses the playing flag
        return (wrapperType != wrapperType_Standalone) || playing;
    }

    // Analysis results - frequency bands
    float getSubBassEnergy() const { return subBassEnergy.load(); }
    float getBassEnergy() const { return bassEnergy.load(); }
    float getLowMidEnergy() const { return lowMidEnergy.load(); }
    float getMidEnergy() const { return midEnergy.load(); }
    float getHighMidEnergy() const { return highMidEnergy.load(); }
    float getHighEnergy() const { return highEnergy.load(); }
    float getVeryHighEnergy() const { return veryHighEnergy.load(); }
    float getKickTransient() const { return kickTransient.load(); }
    float getFullSpectrum() const { return fullSpectrum.load(); }

    // Get FFT spectrum data for frequency range
    void getSpectrumForRange(float minFreq, float maxFreq, std::vector<float>& output, int numPoints) const;

private:
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transportSource;

    bool audioLoaded = false;
    bool playing = false;

    // FFT Analysis
    static constexpr int fftOrder = 11; // 2^11 = 2048 samples
    static constexpr int fftSize = 1 << fftOrder;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { fftSize, juce::dsp::WindowingFunction<float>::hann };

    std::array<float, fftSize * 2> fftData;
    int fftDataPos = 0;

    // Frequency band energies
    std::atomic<float> subBassEnergy { 0.0f };    // 20-60 Hz
    std::atomic<float> bassEnergy { 0.0f };       // 60-250 Hz
    std::atomic<float> lowMidEnergy { 0.0f };     // 250-500 Hz
    std::atomic<float> midEnergy { 0.0f };        // 500-2000 Hz
    std::atomic<float> highMidEnergy { 0.0f };    // 2000-4000 Hz
    std::atomic<float> highEnergy { 0.0f };       // 4000-8000 Hz
    std::atomic<float> veryHighEnergy { 0.0f };   // 8000-20000 Hz
    std::atomic<float> kickTransient { 0.0f };    // Kick drum transient detector
    std::atomic<float> fullSpectrum { 0.0f };     // All frequencies combined

    // Adaptive normalization - running averages for auto-gain
    float subBassAverage = 0.0f;
    float bassAverage = 0.0f;
    float lowMidAverage = 0.0f;
    float midAverage = 0.0f;
    float highMidAverage = 0.0f;
    float highAverage = 0.0f;
    float veryHighAverage = 0.0f;
    float fullSpectrumAverage = 0.0f;

    // Kick detection
    float previousBassForKick = 0.0f;
    float kickDecay = 0.0f;
    int kickCooldown = 0;  // Prevent retriggering too quickly

    // Smoothing factors for adaptive gain
    static constexpr float averageSmoothingFactor = 0.95f;  // How fast to adapt (was 0.99)
    static constexpr float minAverageThreshold = 0.001f;    // Prevent division by zero

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioVisualizerProcessor)
};
