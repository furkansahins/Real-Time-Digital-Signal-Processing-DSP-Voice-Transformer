// ============================================================================
//  AudioBuffer.h
//
//  UML'deki "AudioBuffer" sinifi.
//  Gorevi: ses verisini kisa sureligine tutmak (mikrofon ile cikis arasindaki
//  ara depo / FIFO tampon). Mimaride bahsi gecen "Circular Buffer" fikrinin
//  pratik bir uygulamasidir: mikrofondan gelen ornekleri sona ekleriz
//  (write), cikisa gonderirken bastan tuketiriz (read).
//
//  Ses formatimiz: 32-bit float, kanallar ic ice gecmis (interleaved).
//  Yani 2 kanal icin dizi: [L0, R0, L1, R1, L2, R2, ...]
//  Bir "frame" = tum kanallarin tek bir ani (stereo'da 2 float).
// ============================================================================
#pragma once

#include <windows.h>
#include <vector>
#include <cstring>   // memcpy

using namespace std;

class AudioBuffer
{
public:
    // --- UML alanlari ---
    UINT32 channels   = 0;   // kanal sayisi (1=mono, 2=stereo)
    UINT32 sampleRate = 0;   // ornekleme frekansi (Hz), ornek: 48000

    // Ic ice gecmis (interleaved) float ornekler. UML'deki "audioData".
    vector<float> data;

    // Tamponu kullanmadan once kanal sayisi ve frekansi tanitiriz.
    void configure(UINT32 ch, UINT32 sr) { channels = ch; sampleRate = sr; }

    // UML'deki "frameCount": tamponda kac frame (zaman ani) bekliyor.
    UINT32 availableFrames() const
    {
        return channels ? (UINT32)(data.size() / channels) : 0;
    }

    // ---- write(frame): mikrofondan gelen frame'leri tamponun SONUNA ekler ----
    void writeFrames(const float* src, UINT32 frames)
    {
        data.insert(data.end(), src, src + (size_t)frames * channels);
    }

    // Sessizlik (sifir) frame'leri yazar. (Cihaz SILENT bayragi gonderdiginde.)
    void writeSilence(UINT32 frames)
    {
        data.insert(data.end(), (size_t)frames * channels, 0.0f);
    }

    // ---- read(frame): tamponun BASINDAN frame'leri okur ve siler (FIFO) ----
    // Istenen kadar veri yoksa, eldeki kadarini verir. Gercek okunan frame
    // sayisini dondurur.
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

    // ---- Gecikme guvenligi: tampon cok dolarsa en ESKI veriyi at ----
    // Boylece bir takilma olsa bile gecikme (latency) sinirli kalir, birikmez.
    void clampToMaxFrames(UINT32 maxFrames)
    {
        UINT32 avail = availableFrames();
        if (avail > maxFrames) {
            UINT32 drop = avail - maxFrames;
            data.erase(data.begin(), data.begin() + (size_t)drop * channels);
        }
    }
};
