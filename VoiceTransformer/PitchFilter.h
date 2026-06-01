// ============================================================================
//  PitchFilter.h
//
//  UML'deki "PitchFilter" sinifi (Effect'ten TUREYEN somut efekt).
//
//  GOREVI: Perde kaydirma (pitch shifting). Sesin HIZINI/TEMPOSUNU bozmadan
//  sadece TINISINI (frekansini) yukari ya da asagi tasir:
//     - Negatif yari-ton (semitone)  => ses KALINLASIR (daha tok/derin)
//     - Pozitif yari-ton             => ses INCELIR (daha tiz)
//
//  ALGORITMA: Granular / overlap-add (zaman-domeni) perde kaydirici.
//  Mantik: Gelen sesi bir "gecikme hatti"na (delay line) yazariz. Cikis icin
//  bu hatti, yazma hizindan FARKLI bir hizda (pitch orani kadar) okuruz:
//     okuma_hizi = ratio  =>  cikis perdesi = giris perdesi * ratio
//  Tek bir okuma kafasi kullansak, okuma yazmadan uzaklasip ucan/kayan ses
//  yapardi. Bunu onlemek icin YARIM PENCERE kaydirilmis IKI okuma kafasi
//  kullaniriz ve uclarini ucgen (triangular) zarf ile capraz-gecisle
//  (crossfade) birlestiririz. Iki zarfin toplami her an 1 oldugundan ses
//  seviyesi sabit kalir ve ekleme noktalarindaki "catlama" gizlenir.
//
//  Her ornek icin sabit islem => O(1), gercek zamanli ve dusuk gecikmeli.
// ============================================================================
#pragma once

#include <windows.h>
#include "Effect.h"
#include <vector>
#include <cmath>     // pow, fabs

using namespace std;

class PitchFilter : public Effect   // <-- UML'deki "Extends" (kalitim)
{
public:
    // ctor: kac yari-ton kaydiracagimizi ve ekranda gosterilecek adi alir.
    //   semitones < 0  => kalinlastir,  semitones > 0 => incelt
    PitchFilter(float semitones, const wchar_t* displayName)
        : m_semitones(semitones), m_name(displayName)
    {
        recomputeRatio();
    }

    // Calisirken perdeyi degistirmek istersek.
    void setSemitones(float semitones)
    {
        m_semitones = semitones;
        recomputeRatio();
    }

    // Ses bicimi bilinince (frekans + kanal) cagrilir; ic tamponlari hazirlar.
    void configure(UINT32 sampleRate, UINT32 channels)
    {
        m_sampleRate = sampleRate;
        m_channels   = channels;
        // Her kanal icin ayri bir gecmis (history) tamponu; hepsi sifirla baslar.
        m_history.assign(channels, vector<float>(HISTORY, 0.0f));
        m_writeIdx = 0;
        m_delay    = 0.0f;
    }

    const wchar_t* getName() const override { return m_name; }

    // ---- Asil efekt: ornekleri yerinde perde-kaydirir ----
    void applyEffect(float* data, UINT32 numFrames, UINT32 channels) override
    {
        // Henuz configure edilmediyse (veya kanal uyusmuyorsa) dokunma.
        if (m_history.size() != channels || channels == 0) return;

        const float grain = (float)GRAIN;

        for (UINT32 f = 0; f < numFrames; ++f) {
            // 1) Okuma gecikmesini her frame'de "(1 - ratio)" kadar kaydir.
            //    Boylece okuma kafasinin efektif hizi 'ratio' olur:
            //    d(okuma)/d(yazma) = 1 - d(delay)/d(yazma) = 1 - (1-ratio) = ratio
            m_delay += (1.0f - m_ratio);
            // Gecikmeyi [0, GRAIN) araligina sar (wrap).
            while (m_delay <  0.0f)  m_delay += grain;
            while (m_delay >= grain) m_delay -= grain;

            // 2) Iki okuma kafasi: ikincisi yarim pencere (GRAIN/2) otelenmis.
            float d0 = m_delay;
            float d1 = m_delay + grain * 0.5f;
            if (d1 >= grain) d1 -= grain;

            // 3) Ucgen zarflar: pencere ortasinda 1, uclarinda 0.
            //    (Toplamlari her an 1 -> sabit ses seviyesi, puruzsuz crossfade)
            float e0 = 1.0f - fabsf(2.0f * d0 / grain - 1.0f);
            float e1 = 1.0f - fabsf(2.0f * d1 / grain - 1.0f);

            // 4) Her kanali ayri isle (ayni gecikme, farkli gecmis tamponu).
            for (UINT32 c = 0; c < channels; ++c) {
                // Once gelen ornegi gecmis tampona yaz.
                m_history[c][m_writeIdx] = data[f * channels + c];
                // Iki okuma kafasindan ara-degerli (interpolated) oku ve karistir.
                float s0 = readSample(c, (float)m_writeIdx - d0);
                float s1 = readSample(c, (float)m_writeIdx - d1);
                data[f * channels + c] = s0 * e0 + s1 * e1;
            }

            // 5) Yazma indeksini bir frame ilerlet (dairesel).
            m_writeIdx = (m_writeIdx + 1) & (HISTORY - 1);
        }
    }

private:
    // Gecmis tamponundan KESIRLI konumdan lineer interpolasyonla ornek okur.
    float readSample(UINT32 ch, float index) const
    {
        // index'i [0, HISTORY) araligina sar.
        while (index <  0.0f)            index += HISTORY;
        while (index >= (float)HISTORY)  index -= HISTORY;

        int   i0   = (int)index;
        float frac = index - (float)i0;
        int   i1   = (i0 + 1) & (HISTORY - 1);
        return m_history[ch][i0] * (1.0f - frac) + m_history[ch][i1] * frac;
    }

    // Yari-ton sayisini frekans oranina cevirir: ratio = 2^(yari-ton / 12)
    void recomputeRatio()
    {
        m_ratio = (float)pow(2.0, (double)m_semitones / 12.0);
    }

    // HISTORY ve GRAIN 2'nin kuvveti olmali (hizli "& (N-1)" sarma icin).
    static const int HISTORY = 4096;   // gecmis tampon uzunlugu (ornek)
    static const int GRAIN   = 2048;   // pencere/grain boyutu (ornek) ~43ms@48k

    float m_semitones = 0.0f;          // kac yari-ton kaydirilacak
    float m_ratio     = 1.0f;          // 2^(semitones/12)
    const wchar_t* m_name = L"Pitch";

    UINT32 m_sampleRate = 48000;
    UINT32 m_channels   = 0;
    vector<vector<float>> m_history;   // [kanal][HISTORY] dairesel gecmis
    UINT32 m_writeIdx = 0;             // ortak yazma indeksi
    float  m_delay    = 0.0f;          // suzulen okuma gecikmesi [0, GRAIN)
};
