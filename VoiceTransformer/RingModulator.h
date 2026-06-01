// ============================================================================
//  RingModulator.h
//
//  UML'deki "RingModulator" sinifi (Effect'ten TUREYEN somut efekt).
//
//  GOREVI: Ses sinyalini sentetik bir tasiyici dalga (sinus) ile CARPAR.
//  Sonuc: metalik / robotik bir tini (klasik "Dalek" veya telsiz sesi).
//
//  MATEMATIK:  cikis[n] = giris[n] * sin(2*pi*f * n / sampleRate)
//  Burada f = tasiyici frekans (carrierFreq). Dusuk f (~30-80 Hz) titrek/
//  robotik bir his verir. Her ornekte tasiyici dalganin fazini ilerletiriz.
//  Islem ornek basina sabit => O(1), gercek zamanli.
// ============================================================================
#pragma once

#include <windows.h>
#include "Effect.h"
#include <cmath>   // sin

using namespace std;

class RingModulator : public Effect   // <-- UML'deki "Extends" (kalitim)
{
public:
    // UML alani: tasiyici frekans (Hz). Robotik etki icin tipik deger ~50 Hz.
    explicit RingModulator(float carrierFreq = 50.0f)
        : m_carrierFreq(carrierFreq) {}

    // Ses bicimi bilinince cagrilir (faz adimini frekansa gore ayarlamak icin).
    void configure(UINT32 sampleRate) { m_sampleRate = sampleRate; }

    void setCarrierFreq(float hz) { m_carrierFreq = hz; }

    const wchar_t* getName() const override { return L"Robotik (ring mod)"; }

    // ---- Asil efekt: her ornegi tasiyici sinus dalgasiyla carpar ----
    void applyEffect(float* data, UINT32 numFrames, UINT32 channels) override
    {
        const double twoPi = 6.283185307179586;
        // Her frame icin fazin ne kadar ilerleyecegi (radyan).
        double phaseInc = twoPi * (double)m_carrierFreq / (double)m_sampleRate;

        for (UINT32 f = 0; f < numFrames; ++f) {
            float carrier = (float)sin(m_phase);     // o anki tasiyici degeri
            // Ayni frame'deki tum kanallari ayni tasiyiciyla carp.
            for (UINT32 c = 0; c < channels; ++c) {
                data[f * channels + c] *= carrier;
            }
            // Fazi ilerlet ve tasmamasi icin 2*pi'de sar.
            m_phase += phaseInc;
            if (m_phase >= twoPi) m_phase -= twoPi;
        }
    }

private:
    float  m_carrierFreq = 50.0f;   // tasiyici frekans (Hz)
    UINT32 m_sampleRate  = 48000;   // ornekleme frekansi
    double m_phase       = 0.0;     // tasiyici dalganin o anki fazi (radyan)
};
