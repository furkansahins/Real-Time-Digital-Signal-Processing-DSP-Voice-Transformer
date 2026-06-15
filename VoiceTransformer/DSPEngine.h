#pragma once

#include <windows.h>
#include "Effect.h"

using namespace std;

class DSPEngine
{
public:
    void setEffect(Effect* effect) { m_activeEffect = effect; }

    Effect* getEffect() const { return m_activeEffect; }

    void process(float* data, UINT32 numFrames, UINT32 channels)
    {
        if (m_activeEffect && m_activeEffect->isEnabled) {
            m_activeEffect->applyEffect(data, numFrames, channels);
        }
    }

private:
    Effect* m_activeEffect = nullptr;
};
