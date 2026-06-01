// ============================================================================
//  WASAPIManager.h
//
//  UML'deki "WASAPIManager" sinifi.
//
//  GOREVI: Mikrofon ile kodumuz arasindaki baglantiyi yonetmek. WASAPI'nin
//  IAudioClient + IAudioCaptureClient arayuzlerini sarar (encapsulate eder).
//  Mikrofondan gelen ham PCM ornekleri AudioBuffer (FIFO) icine bosaltir.
//
//  NOT: Yakalama akisini, hedef (CABLE) formatina AUTOCONVERTPCM ile
//  cevirtiyoruz. Boylece mikrofonun kendi formati ne olursa olsun, FIFO'ya
//  hep CABLE ile AYNI formatta (32-bit float) veri girer ve sonradan manuel
//  ornekleme-cevrimi (resampling) gerekmez.
// ============================================================================
#pragma once

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include "AudioBuffer.h"

using namespace std;

class WASAPIManager
{
public:
    // --- UML alanlari ---
    UINT32 sampleRate = 0;   // ornekleme frekansi (Hz)
    UINT32 channels   = 0;   // kanal sayisi
    UINT32 bufferSize = 0;   // yakalama tamponu boyutu (frame)

    ~WASAPIManager() { release(); }

    // ---- Mikrofonu hazirlar ----
    //   micDevice   : kullanicinin sectigi fiziksel mikrofon
    //   sharedFormat: hedef (CABLE) mix formati; AUTOCONVERT ile buna cevrilir
    bool init(IMMDevice* micDevice, WAVEFORMATEX* sharedFormat)
    {
        m_blockAlign = sharedFormat->nBlockAlign;
        channels     = sharedFormat->nChannels;
        sampleRate   = sharedFormat->nSamplesPerSec;

        // Mikrofon cihazindan IAudioClient'i etkinlestir.
        HRESULT hr = micDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
                                         nullptr, (void**)&m_client);
        if (FAILED(hr)) return false;

        // Shared Mode + AUTOCONVERTPCM: mikrofon sesini sharedFormat'a cevirt.
        hr = m_client->Initialize(
                AUDCLNT_SHAREMODE_SHARED,
                AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
                10000000 /*1 sn tampon*/, 0, sharedFormat, nullptr);
        if (FAILED(hr)) return false;

        // Yakalama servisini (capture client) al.
        hr = m_client->GetService(__uuidof(IAudioCaptureClient), (void**)&m_capture);
        if (FAILED(hr)) return false;

        m_client->GetBufferSize(&bufferSize);
        return true;
    }

    // UML'deki "startCapture()": yakalamayi baslatir.
    bool startCapture() { return m_client && SUCCEEDED(m_client->Start()); }

    // ---- Mikrofonda bekleyen TUM paketleri FIFO'ya bosaltir ----
    // Donus: bu turda yakalanan toplam frame sayisi.
    UINT32 drainInto(AudioBuffer& fifo)
    {
        UINT32 captured = 0;
        UINT32 packetLength = 0;
        if (FAILED(m_capture->GetNextPacketSize(&packetLength))) return 0;

        while (packetLength != 0) {
            BYTE*  pData     = nullptr;
            UINT32 numFrames = 0;
            DWORD  flags     = 0;
            if (FAILED(m_capture->GetBuffer(&pData, &numFrames, &flags, nullptr, nullptr)))
                break;

            if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) || !pData) {
                // Cihaz sessizlik bildirdi: FIFO'ya sifir (sessiz) frame yaz.
                fifo.writeSilence(numFrames);
            } else {
                // sharedFormat 32-bit float oldugu icin pData'yi float* okuyabiliriz.
                fifo.writeFrames(reinterpret_cast<const float*>(pData), numFrames);
            }
            captured += numFrames;

            m_capture->ReleaseBuffer(numFrames);   // ONEMLI: aldigimizi geri ver
            m_capture->GetNextPacketSize(&packetLength);
        }
        return captured;
    }

    void stop() { if (m_client) m_client->Stop(); }

private:
    void release()
    {
        if (m_capture) { m_capture->Release(); m_capture = nullptr; }
        if (m_client)  { m_client->Release();  m_client  = nullptr; }
    }

    IAudioClient*        m_client     = nullptr;
    IAudioCaptureClient* m_capture    = nullptr;
    UINT16               m_blockAlign = 0;   // bir frame'in bayt boyutu
};
