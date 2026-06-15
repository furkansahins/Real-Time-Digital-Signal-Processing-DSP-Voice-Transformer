#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <iostream>
#include <vector>
#include <conio.h>          
#include "DeviceUtils.h"
#include "AudioBuffer.h"
#include "DSPEngine.h"
#include "PitchFilter.h"
#include "RingModulator.h"
#include "WASAPIManager.h"
#include "VirtualAudioDriver.h"

using namespace std;

#define POLL_INTERVAL_MS 10

static const wchar_t* CurrentModeName(DSPEngine& dsp)
{
    Effect* e = dsp.getEffect();
    return e ? e->getName() : L"Efektsiz (passthrough)";
}

static float ComputePeak(const float* data, UINT32 numFrames, UINT32 channels)
{
    float peak = 0.0f;
    UINT32 n = numFrames * channels;
    for (UINT32 i = 0; i < n; ++i) {
        float a = data[i] < 0 ? -data[i] : data[i];
        if (a > peak) peak = a;
    }
    return peak;
}

static void PrintLevelBar(float peak)
{
    const int width = 16;
    int filled = (int)(peak * width + 0.5f);
    if (filled > width) filled = width;
    wcout << L"[";
    for (int i = 0; i < width; ++i) wcout << (i < filled ? L'#' : L'-');
    wcout << L"]";
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    wcout << L"Real-Time DSP Voice Transformer - Adim 4: DSP (Voice Change)\n";
    wcout << L"==============================================================\n";

    IMMDeviceEnumerator* pEnumerator = nullptr;
    IMMDevice*           pCableDevice = nullptr;
    IMMDevice*           pMicDevice   = nullptr;
    int exitCode = 0;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) { wcerr << L"[ERROR] CoInitializeEx\n"; return 1; }

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) { wcerr << L"[ERROR] MMDeviceEnumerator\n"; exitCode = 1; goto Cleanup; }

    pCableDevice = FindRenderDeviceByName(pEnumerator, L"CABLE Input");
    if (!pCableDevice) {
        wcerr << L"[ERROR] 'CABLE Input' did not find. Is VB-Audio Virtual Cable installed?\n";
        exitCode = 1; goto Cleanup;
    }

    pMicDevice = SelectCaptureDevice(pEnumerator);
    if (!pMicDevice) { wcerr << L"[ERROR] Microphone is not selected.\n"; exitCode = 1; goto Cleanup; }

    float pitchlevel = 0.0f;
    bool inputcontroller = false;              

    while (!inputcontroller) {                 
        cout << "Adjust pitch level: ";

        string satir;
        getline(cin, satir);                

        try {
            size_t idx = 0;
            pitchlevel = stof(satir, &idx);  

            if (idx != satir.size())
                throw invalid_argument("Extra character");

            inputcontroller = true;            
        }
        catch (const exception& e) {
            cout << "Invalid value! Please enter a number.\n";
        }
    }

    {
        VirtualAudioDriver cable;    
        WASAPIManager      mic;     
        AudioBuffer        fifo;    
        DSPEngine          dsp;     

        if (!cable.init(pCableDevice)) { wcerr << L"[ERROR] CABLE init\n"; exitCode = 1; goto Cleanup; }
        WAVEFORMATEX* fmt = cable.getFormat();

        if (!mic.init(pMicDevice, fmt)) { wcerr << L"[ERROR] Microphone initialization failed: AUTOCONVERT not supported.\n"; exitCode = 1; goto Cleanup; }

        fifo.configure(fmt->nChannels, fmt->nSamplesPerSec);

        PitchFilter   deepEffect(-pitchlevel, L"Deep ");
        PitchFilter   thinEffect(+pitchlevel, L"High ");
        RingModulator robotEffect(50.0f);                       

  
        deepEffect.configure(fmt->nSamplesPerSec, fmt->nChannels);
        thinEffect.configure(fmt->nSamplesPerSec, fmt->nChannels);
        robotEffect.configure(fmt->nSamplesPerSec);

        dsp.setEffect(nullptr);

        wcout << L"\nSource (input): " << GetDeviceName(pMicDevice) << L"\n";
        wcout << L"Target (output) : " << cable.deviceName << L"\n";
        wcout << L"Common format  : " << fmt->nSamplesPerSec << L" Hz, "
              << fmt->nChannels << L" channels, " << fmt->wBitsPerSample << L" bit\n\n";
        wcout << L"--- MODES (press a key while running) ---\n";
        wcout << L"  [0] Bypass   [1] Deep   [2] High   [3] Robot   [q] Quit\n\n";

        if (!cable.start()) { wcerr << L"[ERROR] CABLE start\n"; exitCode = 1; goto Cleanup; }
        if (!mic.startCapture()) { wcerr << L"[ERROR] Mikrofon start\n"; exitCode = 1; goto Cleanup; }

        wcout << L"Running! Please talk and try modes...\n\n";

        vector<float> work;
        bool running = true;

        while (running) {
            while (_kbhit()) {
                int key = _getch();
                switch (key) {
                    case '0': dsp.setEffect(nullptr);       break;
                    case '1': dsp.setEffect(&deepEffect);   break;
                    case '2': dsp.setEffect(&thinEffect);   break;
                    case '3': dsp.setEffect(&robotEffect);  break;
                    case 'q': case 'Q': case 27 /*ESC*/: running = false; break;
                    default: break;
                }
            }

            Sleep(POLL_INTERVAL_MS);

            mic.drainInto(fifo);

            fifo.clampToMaxFrames(fmt->nSamplesPerSec / 10);

            UINT32 framesAvail   = cable.framesAvailable();
            UINT32 framesInFifo  = fifo.availableFrames();
            UINT32 framesToWrite = framesAvail < framesInFifo ? framesAvail : framesInFifo;

            float peak = 0.0f;
            if (framesToWrite > 0) {
                work.resize((size_t)framesToWrite * fmt->nChannels);
                fifo.readFrames(work.data(), framesToWrite);

                dsp.process(work.data(), framesToWrite, fmt->nChannels);

                peak = ComputePeak(work.data(), framesToWrite, fmt->nChannels);

                cable.sendOutput(work.data(), framesToWrite);
            }

            wcout << L"\rMod: " << CurrentModeName(dsp) << L"   ";
            PrintLevelBar(peak);
            wcout << L"  FIFO: " << framesInFifo << L" frame      ";
            wcout.flush();
        }

        mic.stop();
        cable.stop();
        wcout << L"\n\nStoped.\n";
    }

Cleanup:
    if (pMicDevice)   pMicDevice->Release();
    if (pCableDevice) pCableDevice->Release();
    if (pEnumerator)  pEnumerator->Release();
    CoUninitialize();

    wcout << L"\nPress enter to quit...";
    cin.get();
    return exitCode;
}
