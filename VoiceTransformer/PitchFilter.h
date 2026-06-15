#pragma once

#include <windows.h>
#include "Effect.h"
#include <vector>
#include <cmath> 

using namespace std;

class PitchFilter : public Effect  
{
public:
    PitchFilter(float semitones, const wchar_t* displayName)
        : m_semitones(semitones), m_name(displayName)
    {
        recomputeRatio();
    }

    void setSemitones(float semitones)
    {
        m_semitones = semitones;
        recomputeRatio();
    }

    void configure(UINT32 sampleRate, UINT32 channels)
    {
        m_sampleRate = sampleRate;
        m_channels   = channels;
        m_history.assign(channels, vector<float>(HISTORY, 0.0f));
        m_writeIdx = 0;
        m_delay    = 0.0f;
    }

    const wchar_t* getName() const override { return m_name; }

    void applyEffect(float* data, UINT32 numFrames, UINT32 channels) override
    {
        if (m_history.size() != channels || channels == 0) return;

        const float grain = (float)GRAIN;

        for (UINT32 f = 0; f < numFrames; ++f) {

            m_delay += (1.0f - m_ratio);

            while (m_delay <  0.0f)  m_delay += grain;
            while (m_delay >= grain) m_delay -= grain;

            float d0 = m_delay;
            float d1 = m_delay + grain * 0.5f;
            if (d1 >= grain) d1 -= grain;

            float e0 = 1.0f - fabsf(2.0f * d0 / grain - 1.0f);
            float e1 = 1.0f - fabsf(2.0f * d1 / grain - 1.0f);

            for (UINT32 c = 0; c < channels; ++c) {

                m_history[c][m_writeIdx] = data[f * channels + c];
                float s0 = readSample(c, (float)m_writeIdx - d0);
                float s1 = readSample(c, (float)m_writeIdx - d1);
                data[f * channels + c] = s0 * e0 + s1 * e1;
            }

            m_writeIdx = (m_writeIdx + 1) & (HISTORY - 1);
        }
    }

private:
    float readSample(UINT32 ch, float index) const
    {
        while (index <  0.0f)            index += HISTORY;
        while (index >= (float)HISTORY)  index -= HISTORY;

        int   i0   = (int)index;
        float frac = index - (float)i0;
        int   i1   = (i0 + 1) & (HISTORY - 1);
        return m_history[ch][i0] * (1.0f - frac) + m_history[ch][i1] * frac;
    }

    void recomputeRatio()
    {
        m_ratio = (float)pow(2.0, (double)m_semitones / 12.0);
    }

    static const int HISTORY = 4096;
    static const int GRAIN   = 2048;

    float m_semitones = 0.0f;
    float m_ratio     = 1.0f;         
    const wchar_t* m_name = L"Pitch";

    UINT32 m_sampleRate = 48000;
    UINT32 m_channels   = 0;
    vector<vector<float>> m_history;  
    UINT32 m_writeIdx = 0;            
    float  m_delay    = 0.0f;        
};
