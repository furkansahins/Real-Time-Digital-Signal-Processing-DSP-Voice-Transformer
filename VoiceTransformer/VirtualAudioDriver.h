// ============================================================================
//  VirtualAudioDriver.h
//
//  UML'deki "VirtualAudioDriver" sinifi.
//
//  GOREVI: Islenmis sesi sisteme yeni bir cikis gibi gondermek. Pratikte
//  bunu VB-Audio Virtual Cable uzerinden yapariz: sesi "CABLE Input" sahte
//  hoparlorune yazariz; VB-Cable bunu "CABLE Output" sahte mikrofonuna
//  yonlendirir; Discord/Meet o sahte mikrofonu okur.
//
//  Iceride WASAPI'nin IAudioClient + IAudioRenderClient arayuzlerini sarar.
//  Cihazin mix formatini (sampleRate/kanal/bit) buradan elde edip butun
//  zincirin ORTAK formati olarak kullaniriz.
// ============================================================================
#pragma once

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <string>
#include <cstring>   // memcpy
#include "DeviceUtils.h"

using namespace std;

class VirtualAudioDriver
{
public:
    // --- UML alani: hedef cihazin adi (ornek: "CABLE Input ...") ---
    wstring deviceName;

    UINT32 bufferFrames = 0;   // CABLE render tamponunun boyutu (frame)
    UINT32 channels     = 0;

    ~VirtualAudioDriver() { release(); }

    // ---- CABLE cikis cihazini hazirlar ----
    // cableDevice: "CABLE Input (VB-Audio Virtual Cable)" cihazi.
    bool init(IMMDevice* cableDevice)
    {
        deviceName = GetDeviceName(cableDevice);

        HRESULT hr = cableDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
                                           nullptr, (void**)&m_client);
        if (FAILED(hr)) return false;

        // CABLE'in mix formatini al -> tum zincirin ORTAK formati bu olacak.
        hr = m_client->GetMixFormat(&m_format);
        if (FAILED(hr)) return false;
        m_blockAlign = m_format->nBlockAlign;
        channels     = m_format->nChannels;

        // Render akisini Shared Mode'da, 1 sn'lik tampon ile baslat.
        hr = m_client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0,
                                  10000000 /*1 sn*/, 0, m_format, nullptr);
        if (FAILED(hr)) return false;

        hr = m_client->GetService(__uuidof(IAudioRenderClient), (void**)&m_render);
        if (FAILED(hr)) return false;

        m_client->GetBufferSize(&bufferFrames);
        return true;
    }

    // Tum zincirin kullanacagi ortak ses formati (CABLE mix formati).
    WAVEFORMATEX* getFormat() const { return m_format; }

    bool start() { return m_client && SUCCEEDED(m_client->Start()); }

    // CABLE render tamponunda su an KAC frame'lik bos yer var?
    UINT32 framesAvailable()
    {
        UINT32 padding = 0;
        if (FAILED(m_client->GetCurrentPadding(&padding))) return 0;
        return bufferFrames - padding;
    }

    // ---- UML'deki "sendOutput()" ----
    // Islenmis 'frames' kadar frame'i CABLE Input'a yazar.
    void sendOutput(const float* data, UINT32 frames)
    {
        if (frames == 0) return;
        BYTE* pRender = nullptr;
        if (SUCCEEDED(m_render->GetBuffer(frames, &pRender))) {
            memcpy(pRender, data, (size_t)frames * m_blockAlign);
            m_render->ReleaseBuffer(frames, 0);
        }
    }

    void stop() { if (m_client) m_client->Stop(); }

private:
    void release()
    {
        if (m_format) { CoTaskMemFree(m_format); m_format = nullptr; }
        if (m_render) { m_render->Release();     m_render = nullptr; }
        if (m_client) { m_client->Release();     m_client = nullptr; }
    }

    IAudioClient*       m_client     = nullptr;
    IAudioRenderClient* m_render     = nullptr;
    WAVEFORMATEX*       m_format     = nullptr;   // CABLE mix formati (sahibi biziz)
    UINT16              m_blockAlign = 0;
};
