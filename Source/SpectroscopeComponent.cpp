/*
  ==============================================================================

    SpectroscopeComponent.cpp
    Created: 8 Apr 2017 12:46:51pm
    Author:  Nick Thompson

  ==============================================================================
*/

#include "../JuceLibraryCode/JuceHeader.h"
#include "SpectroscopeComponent.h"

//==============================================================================
SpectroscopeComponent::SpectroscopeComponent()
:   m_fifoIndex(0),
    m_fftBlockReady(false),
    m_forwardFFT(kFFTOrder, false),
    m_baseColour(Colours::white)
{
    zeromem(m_outputData, sizeof(m_outputData));
    setSize(700, 200);
    startTimerHz(60);
}

SpectroscopeComponent::~SpectroscopeComponent()
{
}

void SpectroscopeComponent::paint (Graphics& g)
{
    const float width = (float) getWidth();
    const float height = (float) getHeight();

    // The values in the output bins after the FFT have a range that I don't understand
    // and isn't explained in the docs. It seems that if I scale down by the size of the
    // fft buffer, I get somewhat reasonable results on the graph. But in examples I've
    // seen, we would just divide here by the maximum value in the bins at the time of
    // drawing. Seeing as that would be inconsistent between frames, I'm defaulting to the
    // size of the fft here unless the max value in the bins is larger.
    Range<float> maxBin = FloatVectorOperations::findMinAndMax(m_outputData, kOutputSize);
    const float scale = 1.0f / jmax((float) kFFTSize, maxBin.getEnd());

    Path p;

    // TODO: Try 2x oversampling with windowed sinc ipol since we are drawing
    // only 512 samples in a region potentially larger than 512px wide.
    for (int i = 0; i < kOutputSize; ++i)
    {
        const float xPos = (float) i / (float) kOutputSize;
        const float x = std::exp(std::log(xPos) * 0.2f) * width;

        const float yMag = m_outputData[i] * scale;
        const float yDecibel = Decibels::gainToDecibels(yMag);
        const float y = jmap(yDecibel, -90.0f, 3.0f, height, 0.0f);

        if (i == 0)
            p.startNewSubPath(0.0f, y);
        else
            p.lineTo(x, y);
    }

    // Clear the drawing target
    g.setColour(Colours::transparentBlack);
    g.fillAll();

    // Stroke the line
    g.setColour(m_baseColour);
    g.strokePath(p, PathStrokeType(1.0f));

    // Fill under the line with a gradient
    Colour start = m_baseColour.withAlpha(0.2f);
    Colour stop = m_baseColour.withAlpha(0.8f);

    // Wrap the line around the bottom of the graph before closing the path.
    p.lineTo(p.getCurrentPosition().getX(), height);
    p.lineTo(0.0f, height);
    p.closeSubPath();
    g.setGradientFill(ColourGradient(start, 0.0f, height, stop, 0.0f, 0.0f, false));
    g.fillPath(p);

}

void SpectroscopeComponent::resized()
{
}

void SpectroscopeComponent::timerCallback()
{
    if (m_fftBlockReady)
    {
        // Compute the frequency transform
        m_forwardFFT.performFrequencyOnlyForwardTransform(m_fftData);

        // Copy the frequency bins into the output data buffer, taking
        // max(output[i], fftData[i]) for each bin. Note that after computing the
        // FrequencyOnlyForwardTransform on an array A of size N, A[N/2, N) is full
        // of zeros, and A[0, N/4) is a mirror of A[N/4, N/2). Therefore we only copy
        // kFFTSize / 2 samples into the output data buffer here.
        FloatVectorOperations::max(m_outputData, m_outputData, m_fftData, kOutputSize);

        m_fftBlockReady = false;
    }

    // Decay the output bin magnitudes
    for (int i = 0; i < kOutputSize; ++i)
        m_outputData[i] *= 0.707f;

    repaint();
}

void SpectroscopeComponent::pushBuffer(AudioSampleBuffer &buffer)
{
    if (buffer.getNumChannels() > 0)
    {
        const int numSamples = buffer.getNumSamples();
        const float* channelData = buffer.getReadPointer(0);

        for (int i = 0; i < numSamples; ++i)
            pushSample(channelData[i]);
    }
}

inline void SpectroscopeComponent::pushSample(float sample)
{
    // When we wrap around the fifo table, we copy the data into the
    // FFT buffer and prepare to perform the transform.
    if (m_fifoIndex == kFFTSize)
    {
        if (!m_fftBlockReady)
        {
            zeromem(m_fftData, sizeof(m_fftData));
            memcpy(m_fftData, m_fifo, sizeof(m_fifo));
            m_fftBlockReady = true;
        }

        m_fifoIndex = 0;
    }

    m_fifo[m_fifoIndex] = windowSample(sample, m_fifoIndex, kFFTSize);
    m_fifoIndex++;
}

inline float SpectroscopeComponent::windowSample(float sample, int sampleIndex, int windowSize)
{
    // A simple Hann window implementation. If the FFT size never changes, it would
    // be much more efficient here to compute the window once over a buffer of the
    // same size, then, in `pushSample`, instead of using memcpy to copy the buffer
    // over, use a FloatVectorOperations function which multiplies each sample in the
    // source buffer by the corresponding sample in the window buffer and writes the
    // result to the output buffer.
    float num = 2.0f * float_Pi * (float) sampleIndex;
    float denom = (float) (windowSize - 1);
    return sample * 0.5f * (1.0f - std::cos(num / denom));
}

void SpectroscopeComponent::setBaseColour(Colour c)
{
    m_baseColour = c;
}
