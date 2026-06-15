#pragma once

#include <windows.h>

using namespace std;

class Effect
{
public:
    bool isEnabled = true;

    virtual ~Effect() {}

    virtual const wchar_t* getName() const = 0;

    virtual void applyEffect(float* data, UINT32 numFrames, UINT32 channels) = 0;
};
