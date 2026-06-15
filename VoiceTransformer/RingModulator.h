#pragma once

#include <windows.h>
#include "Effect.h"
#include <cmath>  

using namespace std;

class RingModulator : public Effect 
{
public:
    explicit RingModulator(float carrierFreq = 50.0f)
        : m_carrierFreq(carrierFreq) {}

    void configure(UINT32 sampleRate) { m_sampleRate = sampleRate; }

    void setCarrierFreq(float hz) { m_carrierFreq = hz; }

    const wchar_t* getName() const override { return L"Robotik (ring mod)"; }

    void applyEffect(float* data, UINT32 numFrames, UINT32 channels) override
    {
        const double twoPi = 6.283185307179586;
        double phaseInc = twoPi * (double)m_carrierFreq / (double)m_sampleRate;

        for (UINT32 f = 0; f < numFrames; ++f) {
            float carrier = (float)sin(m_phase); 
            for (UINT32 c = 0; c < channels; ++c) {
                data[f * channels + c] *= carrier;
            }
            m_phase += phaseInc;
            if (m_phase >= twoPi) m_phase -= twoPi;
        }
    }

private:
    float  m_carrierFreq = 50.0f; 
    UINT32 m_sampleRate  = 48000; 
    double m_phase       = 0.0;    
};
