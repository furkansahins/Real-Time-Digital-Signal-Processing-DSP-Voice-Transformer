#pragma once

#include <windows.h>
#include <vector>
#include <cstring>

using namespace std;

class AudioBuffer
{
public:

    UINT32 channels   = 0;
    UINT32 sampleRate = 0;

    vector<float> data;

    void configure(UINT32 ch, UINT32 sr) { channels = ch; sampleRate = sr; }

    UINT32 availableFrames() const
    {
        return channels ? (UINT32)(data.size() / channels) : 0;
    }

    void writeFrames(const float* src, UINT32 frames)
    {
        data.insert(data.end(), src, src + (size_t)frames * channels);
    }

    void writeSilence(UINT32 frames)
    {
        data.insert(data.end(), (size_t)frames * channels, 0.0f);
    }

    UINT32 readFrames(float* dst, UINT32 frames)
    {
        UINT32 avail = availableFrames();
        UINT32 n = (frames < avail) ? frames : avail;
        if (n > 0) {
            memcpy(dst, data.data(), (size_t)n * channels * sizeof(float));
            data.erase(data.begin(), data.begin() + (size_t)n * channels);
        }
        return n;
    }


    void clampToMaxFrames(UINT32 maxFrames)
    {
        UINT32 avail = availableFrames();
        if (avail > maxFrames) {
            UINT32 drop = avail - maxFrames;
            data.erase(data.begin(), data.begin() + (size_t)drop * channels);
        }
    }
};
