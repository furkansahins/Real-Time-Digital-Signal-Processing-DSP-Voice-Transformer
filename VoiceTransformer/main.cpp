// ============================================================================
//  Real-Time DSP Voice Transformer
//  Adim 4: DSP (Sesi Degistirme) + UML sinif mimarisi
//
//  Akis (UML siniflari ile):
//     Fiziksel Mikrofon
//        -> WASAPIManager   (yakalar, CABLE formatina cevirir)
//        -> AudioBuffer     (FIFO ara depo)
//        -> DSPEngine       (aktif Effect'i uygular: PitchFilter / RingModulator)
//        -> VirtualAudioDriver (CABLE Input'a yazar -> CABLE Output -> Meet/Discord)
//
//  Canli mod degistirme (klavye):
//     [0] Efektsiz   [1] Kalin (-5)   [2] Ince (+5)   [3] Robotik   [q] Cikis
//
//  Kurulum hatirlatmasi: kaynak = FIZIKSEL mikrofon, hedef uygulamada (Meet)
//  mikrofon = CABLE Output, "Bu cihazi dinle" KAPALI, KULAKLIK kullan.
// ============================================================================

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <iostream>
#include <vector>
#include <conio.h>            // _kbhit, _getch (canli tus okuma)

#include "DeviceUtils.h"
#include "AudioBuffer.h"
#include "DSPEngine.h"
#include "PitchFilter.h"
#include "RingModulator.h"
#include "WASAPIManager.h"
#include "VirtualAudioDriver.h"

using namespace std;

#define POLL_INTERVAL_MS 10   // dusuk gecikme icin kucuk sabit polling araligi

// O an aktif modun adini dondurur (ekran icin).
static const wchar_t* CurrentModeName(DSPEngine& dsp)
{
    Effect* e = dsp.getEffect();
    return e ? e->getName() : L"Efektsiz (passthrough)";
}

// Bir ses blogundaki en yuksek genligi (0..1) hesaplar (level meter icin).
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
    wcout << L"Real-Time DSP Voice Transformer - Adim 4: DSP (Sesi Degistirme)\n";
    wcout << L"==============================================================\n";

    IMMDeviceEnumerator* pEnumerator = nullptr;
    IMMDevice*           pCableDevice = nullptr;
    IMMDevice*           pMicDevice   = nullptr;
    int exitCode = 0;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) { wcerr << L"[HATA] CoInitializeEx\n"; return 1; }

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) { wcerr << L"[HATA] MMDeviceEnumerator\n"; exitCode = 1; goto Cleanup; }

    // 1) HEDEF: CABLE Input cihazini bul.
    pCableDevice = FindRenderDeviceByName(pEnumerator, L"CABLE Input");
    if (!pCableDevice) {
        wcerr << L"[HATA] 'CABLE Input' bulunamadi. VB-Audio Virtual Cable kurulu mu?\n";
        exitCode = 1; goto Cleanup;
    }

    // 2) KAYNAK: fiziksel mikrofonu kullaniciya sectir (CABLE secimi sonsuz yanki yapar).
    pMicDevice = SelectCaptureDevice(pEnumerator);
    if (!pMicDevice) { wcerr << L"[HATA] Mikrofon secilemedi.\n"; exitCode = 1; goto Cleanup; }

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
                throw invalid_argument("fazladan karakter");

            inputcontroller = true;            
        }
        catch (const exception& e) {
            cout << "Gecersiz deger! Lutfen bir sayi girin.\n";
        }
    }

    {
        // ---- UML siniflarini olustur ve birbirine bagla ----
        VirtualAudioDriver cable;     // cikis (CABLE Input)
        WASAPIManager      mic;       // giris (fiziksel mikrofon)
        AudioBuffer        fifo;      // ara depo (mikrofon -> DSP arasinda)
        DSPEngine          dsp;       // efekt uygulayici beyin

        // Once CABLE'i hazirla; ortak format (mix format) ondan gelir.
        if (!cable.init(pCableDevice)) { wcerr << L"[HATA] CABLE init\n"; exitCode = 1; goto Cleanup; }
        WAVEFORMATEX* fmt = cable.getFormat();

        // Mikrofonu, CABLE formatina cevirerek hazirla (AUTOCONVERT).
        if (!mic.init(pMicDevice, fmt)) { wcerr << L"[HATA] Mikrofon init (AUTOCONVERT desteklenmiyor olabilir)\n"; exitCode = 1; goto Cleanup; }

        fifo.configure(fmt->nChannels, fmt->nSamplesPerSec);

        // ---- Efektleri olustur (sahiplik burada; DSPEngine yalnizca isaret eder) ----
        PitchFilter   deepEffect(-pitchlevel, L"Kalin ");   // ses kalinlastir
        PitchFilter   thinEffect(+pitchlevel, L"Ince ");    // ses incelt
        RingModulator robotEffect(50.0f);                       // robotik

        // Efektlere ses bicimini tanit (ic tamponlarini hazirlarlar).
        deepEffect.configure(fmt->nSamplesPerSec, fmt->nChannels);
        thinEffect.configure(fmt->nSamplesPerSec, fmt->nChannels);
        robotEffect.configure(fmt->nSamplesPerSec);

        dsp.setEffect(nullptr);   // baslangic: efektsiz (passthrough)

        // Bilgi ekrani
        wcout << L"\nKaynak (giris): " << GetDeviceName(pMicDevice) << L"\n";
        wcout << L"Hedef (cikis) : " << cable.deviceName << L"\n";
        wcout << L"Ortak format  : " << fmt->nSamplesPerSec << L" Hz, "
              << fmt->nChannels << L" kanal, " << fmt->wBitsPerSample << L" bit\n\n";
        wcout << L"--- MODLAR (calisirken tusa bas) ---\n";
        wcout << L"  [0] Efektsiz   [1] Kalin    [2] Ince    [3] Robotik   [q] Cikis\n\n";

        // ---- Akislari baslat ----
        if (!cable.start()) { wcerr << L"[HATA] CABLE start\n"; exitCode = 1; goto Cleanup; }
        if (!mic.startCapture()) { wcerr << L"[HATA] Mikrofon start\n"; exitCode = 1; goto Cleanup; }

        wcout << L"Calisiyor! Konusun ve mod tuslarini deneyin...\n\n";

        vector<float> work;            // DSP'nin uzerinde calisacagi gecici blok
        bool running = true;

        // ======================= ANA DONGU =======================
        while (running) {
            // (1) Klavye: mod degistir / cik.
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

            // (2) Mikrofondan gelen sesi FIFO'ya bosalt.
            mic.drainInto(fifo);

            // (3) Gecikme guvenligi: FIFO 100ms'i asarsa eski veriyi dusur.
            fifo.clampToMaxFrames(fmt->nSamplesPerSec / 10);

            // (4) CABLE'da bos yer kadar, FIFO'dan veri al -> DSP'den gecir -> yaz.
            UINT32 framesAvail   = cable.framesAvailable();
            UINT32 framesInFifo  = fifo.availableFrames();
            UINT32 framesToWrite = framesAvail < framesInFifo ? framesAvail : framesInFifo;

            float peak = 0.0f;
            if (framesToWrite > 0) {
                work.resize((size_t)framesToWrite * fmt->nChannels);
                fifo.readFrames(work.data(), framesToWrite);

                // ---- DSP: aktif efekti uygula (efektsizse dokunmaz) ----
                dsp.process(work.data(), framesToWrite, fmt->nChannels);

                peak = ComputePeak(work.data(), framesToWrite, fmt->nChannels);

                // ---- Islenmis sesi CABLE Input'a gonder ----
                cable.sendOutput(work.data(), framesToWrite);
            }

            // (5) Durum satiri (ayni satiri gunceller).
            wcout << L"\rMod: " << CurrentModeName(dsp) << L"   ";
            PrintLevelBar(peak);
            wcout << L"  FIFO: " << framesInFifo << L" frame      ";
            wcout.flush();
        }
        // ===================== ANA DONGU SONU =====================

        mic.stop();
        cable.stop();
        wcout << L"\n\nDurduruldu.\n";
        // cable/mic/fifo/dsp ve efektler burada (kapsam sonunda) otomatik yikilir.
    }

Cleanup:
    if (pMicDevice)   pMicDevice->Release();
    if (pCableDevice) pCableDevice->Release();
    if (pEnumerator)  pEnumerator->Release();
    CoUninitialize();

    wcout << L"\nCikmak icin Enter'a basin...";
    cin.get();
    return exitCode;
}
