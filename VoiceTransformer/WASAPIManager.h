#pragma once

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include "AudioBuffer.h"

using namespace std;

class WASAPIManager
{
public:

    UINT32 sampleRate = 0;  
    UINT32 channels   = 0;  
    UINT32 bufferSize = 0;

    ~WASAPIManager() { release(); }


    bool init(IMMDevice* micDevice, WAVEFORMATEX* sharedFormat)
    {
        m_blockAlign = sharedFormat->nBlockAlign;
        channels     = sharedFormat->nChannels;
        sampleRate   = sharedFormat->nSamplesPerSec;

        HRESULT hr = micDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
                                         nullptr, (void**)&m_client);
        if (FAILED(hr)) return false;

        hr = m_client->Initialize(
                AUDCLNT_SHAREMODE_SHARED,
                AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
                10000000 , 0, sharedFormat, nullptr);
        if (FAILED(hr)) return false;

        hr = m_client->GetService(__uuidof(IAudioCaptureClient), (void**)&m_capture);
        if (FAILED(hr)) return false;

        m_client->GetBufferSize(&bufferSize);
        return true;
    }

    bool startCapture() { return m_client && SUCCEEDED(m_client->Start()); }

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
                fifo.writeSilence(numFrames);
            } else {
                fifo.writeFrames(reinterpret_cast<const float*>(pData), numFrames);
            }
            captured += numFrames;

            m_capture->ReleaseBuffer(numFrames); 
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
    UINT16               m_blockAlign = 0; 
};
