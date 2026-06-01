// ============================================================================
//  Effect.h
//
//  UML'deki "Effect" TABAN (base / abstract) sinifi.
//
//  Bu sinif, tum ses efektlerinin ortak atasidir. Tek basina kullanilmaz;
//  PitchFilter ve RingModulator gibi somut efektler bundan TUREYIP
//  (inheritance) applyEffect() fonksiyonunu kendine gore doldurur.
//
//  Bu tasarima "Strategy Pattern" denir: DSPEngine somut efekti bilmez,
//  sadece elinde bir "Effect*" tutar; calisma aninda hangi turetilmis efekt
//  bagliysa onun applyEffect()'i cagrilir (polimorfizm). Yeni bir efekt
//  eklemek = sadece Effect'ten yeni bir sinif turetmek.
// ============================================================================
#pragma once

#include <windows.h>

using namespace std;

class Effect
{
public:
    // UML'deki "isEnabled": efekt acik mi? Kapaliysa DSPEngine onu atlar.
    bool isEnabled = true;

    // Taban sinif pointer'i uzerinden silme guvenli olsun diye sanal yikici.
    virtual ~Effect() {}

    // Efektin ekranda gosterilecek adi (her turetilmis sinif kendi adini verir).
    virtual const wchar_t* getName() const = 0;

    // ---- UML'deki "applyEffect(buffer)" ----
    // Ses ornekleri uzerinde efekti UYGULAR (yerinde / in-place degistirir).
    //   data     : ic ice gecmis (interleaved) float ornek dizisi
    //   numFrames: kac frame (zaman ani) var
    //   channels : kanal sayisi (stereo'da 2)
    // "= 0" => saf sanal (pure virtual): turetilmis siniflar doldurmak ZORUNDA.
    virtual void applyEffect(float* data, UINT32 numFrames, UINT32 channels) = 0;
};
