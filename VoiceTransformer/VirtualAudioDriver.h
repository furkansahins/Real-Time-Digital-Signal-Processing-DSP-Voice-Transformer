#pragma once

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <string>
#include <cstring>
#include "DeviceUtils.h"

using namespace std;

class VirtualAudioDriver
{
public:

    wstring deviceName;

    UINT32 bufferFrames = 0;
    UINT32 channels     = 0;

    ~VirtualAudioDriver() { release(); }

    bool init(IMMDevice* cableDevice)
    {
        deviceName = GetDeviceName(cableDevice);

        HRESULT hr = cableDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
                                           nullptr, (void**)&m_client);
        if (FAILED(hr)) return false;

        hr = m_client->GetMixFormat(&m_format);
        if (FAILED(hr)) return false;
        m_blockAlign = m_format->nBlockAlign;
        channels     = m_format->nChannels;

        hr = m_client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0,
                                  10000000 , 0, m_format, nullptr);
        if (FAILED(hr)) return false;

        hr = m_client->GetService(__uuidof(IAudioRenderClient), (void**)&m_render);
        if (FAILED(hr)) return false;

        m_client->GetBufferSize(&bufferFrames);
        return true;
    }

    WAVEFORMATEX* getFormat() const { return m_format; }

    bool start() { return m_client && SUCCEEDED(m_client->Start()); }

    UINT32 framesAvailable()
    {
        UINT32 padding = 0;
        if (FAILED(m_client->GetCurrentPadding(&padding))) return 0;
        return bufferFrames - padding;
    }

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
    WAVEFORMATEX*       m_format     = nullptr;
    UINT16              m_blockAlign = 0;
};
