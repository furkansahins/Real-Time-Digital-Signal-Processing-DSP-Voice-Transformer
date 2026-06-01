// ============================================================================
//  DSPEngine.h
//
//  UML'deki "DSPEngine" sinifi.
//
//  GOREVI: Ses motorunun "beyni". Somut efektleri (PitchFilter, RingModulator)
//  BILMEZ; sadece elinde bir taban-sinif isaretcisi (Effect*) tutar. Hangi
//  efekt bagliysa, ses bloguna onun applyEffect()'ini uygular. Bu sayede
//  calisma aninda efekt degistirmek tek satir: setEffect(...).
//  (Strategy Pattern: efekt = degistirilebilir strateji.)
//
//  activeEffect = nullptr  =>  EFEKTSIZ (passthrough): ses oldugu gibi gecer.
// ============================================================================
#pragma once

#include <windows.h>
#include "Effect.h"

using namespace std;

class DSPEngine
{
public:
    // UML'deki "setEffect(Effect newEffect)": aktif efekti degistirir.
    // nullptr verilirse efekt kapatilir (ses degismeden gecer).
    void setEffect(Effect* effect) { m_activeEffect = effect; }

    // O an hangi efekt bagli? (Ekranda mod adini gostermek icin.)
    Effect* getEffect() const { return m_activeEffect; }

    // ---- UML'deki "process(buffer)" ----
    // Verilen ses blogunu (interleaved float) aktif efektten gecirir.
    // Aktif efekt yoksa ya da kapaliysa: dokunmaz (passthrough).
    void process(float* data, UINT32 numFrames, UINT32 channels)
    {
        if (m_activeEffect && m_activeEffect->isEnabled) {
            m_activeEffect->applyEffect(data, numFrames, channels);
        }
    }

private:
    // UML alani "activeEffect : Effect*". Sahiplik DISARIDADIR (main tutar);
    // DSPEngine yalnizca isaret eder, silmez.
    Effect* m_activeEffect = nullptr;
};
