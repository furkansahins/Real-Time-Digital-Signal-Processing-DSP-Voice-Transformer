// ============================================================================
//  Real-Time DSP Voice Transformer
//  Adim 3: Render (Sesi VB-Cable'a Yollama / Passthrough)
//
//  Varsayilan mikrofondan yakalanan sesi DSP'ye SOKMADAN dogrudan
//  "CABLE Input (VB-Audio Virtual Cable)" sahte hoparlorune yazar.
//  Boylece sesin karsiya kayipsiz ulastigini test ederiz:
//      Mikrofon --> [FIFO tampon] --> CABLE Input  (==> CABLE Output)
//
//  Test: Windows Ses ayarlarinda CABLE Output'u dinle ya da Discord'da
//  mikrofon olarak CABLE Output'u sec; konustugunda sesini duymalisin.
//
//  Format uyumu icin yakalama akisi AUTOCONVERTPCM ile CABLE'in formatina
//  cevrilir; boylece dogrudan kopyalariz (manuel resampling yok).
//
//  Durdurmak icin herhangi bir tusa bas.
// ============================================================================

#include <windows.h>
#include <mmdeviceapi.h>      // IMMDeviceEnumerator, IMMDevice
#include <audioclient.h>      // IAudioClient, IAudioCaptureClient, IAudioRenderClient
#include <functiondiscoverykeys_devpkey.h>  // PKEY_Device_FriendlyName
#include <iostream>
#include <string>
#include <vector>
#include <conio.h>           // _kbhit, _getch

using namespace std;

#define REFTIMES_PER_SEC 10000000  // 1 saniye = 10.000.000 x 100ns

#define CHECK_HR(hr, msg)                                              \
    if (FAILED(hr)) {                                                  \
        wcerr << L"[HATA] " << msg << L" (HRESULT=0x"                  \
              << hex << (hr) << L")\n";                                \
        goto Cleanup;                                                  \
    }

// Cihazin dostane (friendly) ismini dondurur.
wstring GetDeviceName(IMMDevice* pDevice)
{
    wstring result = L"(bilinmeyen cihaz)";
    IPropertyStore* pProps = nullptr;
    if (SUCCEEDED(pDevice->OpenPropertyStore(STGM_READ, &pProps))) {
        PROPVARIANT v; PropVariantInit(&v);
        if (SUCCEEDED(pProps->GetValue(PKEY_Device_FriendlyName, &v)) && v.pwszVal)
            result = v.pwszVal;
        PropVariantClear(&v);
        pProps->Release();
    }
    return result;
}

// Cikis (render) cihazlari arasinda ismi 'substr' iceren ILK cihazi bulur.
// Ornek: L"CABLE Input" -> "CABLE Input (VB-Audio Virtual Cable)"
IMMDevice* FindRenderDeviceByName(IMMDeviceEnumerator* pEnum, const wchar_t* substr)
{
    IMMDeviceCollection* pCol = nullptr;
    if (FAILED(pEnum->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCol)))
        return nullptr;

    UINT count = 0; pCol->GetCount(&count);
    IMMDevice* found = nullptr;
    for (UINT i = 0; i < count; ++i) {
        IMMDevice* pDev = nullptr;
        if (FAILED(pCol->Item(i, &pDev))) continue;
        if (GetDeviceName(pDev).find(substr) != wstring::npos) {
            found = pDev;  // bulundu (sahipligi cagirana devrediyoruz)
            break;
        }
        pDev->Release();
    }
    pCol->Release();
    return found;
}

// Ses paketindeki en yuksek genligi (0..1) hesaplar (level meter icin).
float ComputePeak(const BYTE* data, UINT32 numFrames, const WAVEFORMATEX* fmt)
{
    float peak = 0.0f;
    UINT32 n = numFrames * fmt->nChannels;
    if (fmt->wBitsPerSample == 32) {
        const float* s = reinterpret_cast<const float*>(data);
        for (UINT32 i = 0; i < n; ++i) { float a = s[i] < 0 ? -s[i] : s[i]; if (a > peak) peak = a; }
    } else if (fmt->wBitsPerSample == 16) {
        const short* s = reinterpret_cast<const short*>(data);
        for (UINT32 i = 0; i < n; ++i) { float a = (s[i] < 0 ? -(float)s[i] : (float)s[i]) / 32768.0f; if (a > peak) peak = a; }
    }
    return peak;
}

void PrintLevelBar(float peak)
{
    const int width = 20;
    int filled = (int)(peak * width + 0.5f);
    if (filled > width) filled = width;
    wcout << L" [";
    for (int i = 0; i < width; ++i) wcout << (i < filled ? L'#' : L'-');
    wcout << L"]";
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    wcout << L"Real-Time DSP Voice Transformer - Adim 3: Render (VB-Cable Passthrough)\n";
    wcout << L"=====================================================================\n";

    // --- Tum yerel degiskenler en ustte (goto Cleanup bypass hatasini onler) ---
    IMMDeviceEnumerator* pEnumerator        = nullptr;
    IMMDevice*           pMicDevice         = nullptr;  // kaynak: varsayilan mikrofon
    IMMDevice*           pCableDevice       = nullptr;  // hedef: CABLE Input
    IAudioClient*        pCaptureAudioClient = nullptr;
    IAudioCaptureClient* pCaptureClient     = nullptr;
    IAudioClient*        pRenderAudioClient = nullptr;
    IAudioRenderClient*  pRenderClient      = nullptr;
    WAVEFORMATEX*        fmt                = nullptr;  // CABLE'in mix formati (ortak format)

    UINT32 renderBufferFrames = 0;
    UINT16 blockAlign         = 0;     // bir frame'in bayt boyutu
    DWORD  sleepMs            = 1;
    unsigned long long totalCaptured = 0;
    unsigned long long totalRendered = 0;
    vector<BYTE> fifo;                 // mikrofon ile CABLE arasindaki tampon

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    CHECK_HR(hr, L"CoInitializeEx basarisiz");

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    CHECK_HR(hr, L"MMDeviceEnumerator olusturulamadi");

    // 1) HEDEF: CABLE Input cihazini bul.
    pCableDevice = FindRenderDeviceByName(pEnumerator, L"CABLE Input");
    if (!pCableDevice) {
        wcerr << L"[HATA] 'CABLE Input' bulunamadi. VB-Audio Virtual Cable kurulu mu?\n";
        goto Cleanup;
    }
    wcout << L"Hedef (cikis) : " << GetDeviceName(pCableDevice) << L"\n";

    // 2) KAYNAK: varsayilan mikrofon.
    hr = pEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &pMicDevice);
    CHECK_HR(hr, L"Varsayilan mikrofon bulunamadi");
    wcout << L"Kaynak (giris): " << GetDeviceName(pMicDevice) << L"\n";

    // 3) RENDER (CABLE) tarafini hazirla; ortak format olarak CABLE mix formatini kullan.
    hr = pCableDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pRenderAudioClient);
    CHECK_HR(hr, L"CABLE IAudioClient etkinlestirilemedi");
    hr = pRenderAudioClient->GetMixFormat(&fmt);
    CHECK_HR(hr, L"CABLE GetMixFormat basarisiz");
    blockAlign = fmt->nBlockAlign;
    wcout << L"Ortak format  : " << fmt->nSamplesPerSec << L" Hz, "
          << fmt->nChannels << L" kanal, " << fmt->wBitsPerSample << L" bit\n";

    hr = pRenderAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0,
                                        REFTIMES_PER_SEC, 0, fmt, nullptr);
    CHECK_HR(hr, L"CABLE render Initialize basarisiz");
    hr = pRenderAudioClient->GetService(__uuidof(IAudioRenderClient), (void**)&pRenderClient);
    CHECK_HR(hr, L"IAudioRenderClient alinamadi");
    pRenderAudioClient->GetBufferSize(&renderBufferFrames);

    // 4) CAPTURE (mikrofon) tarafini hazirla. AUTOCONVERTPCM ile CABLE formatina
    //    cevirt => yakalanan frame'ler dogrudan render'a kopyalanabilir.
    hr = pMicDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pCaptureAudioClient);
    CHECK_HR(hr, L"Mikrofon IAudioClient etkinlestirilemedi");
    hr = pCaptureAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                         AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
                                         REFTIMES_PER_SEC, 0, fmt, nullptr);
    CHECK_HR(hr, L"Mikrofon capture Initialize basarisiz (AUTOCONVERT desteklenmiyor olabilir)");
    hr = pCaptureAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);
    CHECK_HR(hr, L"IAudioCaptureClient alinamadi");

    // Polling icin bekleme: capture tamponunun yarisi kadar.
    {
        UINT32 capFrames = 0; pCaptureAudioClient->GetBufferSize(&capFrames);
        sleepMs = (DWORD)((double)capFrames / fmt->nSamplesPerSec * 1000.0 / 2.0);
        if (sleepMs < 1) sleepMs = 1;
    }

    // 5) Her iki akisi da baslat.
    hr = pCaptureAudioClient->Start();
    CHECK_HR(hr, L"Capture Start basarisiz");
    hr = pRenderAudioClient->Start();
    CHECK_HR(hr, L"Render Start basarisiz");

    wcout << L"\nPassthrough basladi: Mikrofon --> CABLE Input. Konusun!\n";
    wcout << L"(Sesi duymak icin Ses ayarlarinda CABLE Output'u dinleyin)\n";
    wcout << L"(Durdurmak icin bir tusa basin)\n\n";

    // 6) Ana dongu: yakala -> FIFO'ya yaz -> FIFO'dan CABLE'a aktar.
    while (!_kbhit()) {
        Sleep(sleepMs);

        // (a) Mikrofondan bekleyen tum paketleri FIFO'ya bosalt.
        UINT32 packetLength = 0;
        float peak = 0.0f;
        if (FAILED(pCaptureClient->GetNextPacketSize(&packetLength))) break;
        while (packetLength != 0) {
            BYTE* pData = nullptr; UINT32 numFrames = 0; DWORD flags = 0;
            if (FAILED(pCaptureClient->GetBuffer(&pData, &numFrames, &flags, nullptr, nullptr))) break;

            size_t byteCount = (size_t)numFrames * blockAlign;
            if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) || !pData) {
                fifo.insert(fifo.end(), byteCount, 0);  // sessizlik = sifir baytlar
            } else {
                fifo.insert(fifo.end(), pData, pData + byteCount);
                peak = ComputePeak(pData, numFrames, fmt);
            }
            totalCaptured += numFrames;
            pCaptureClient->ReleaseBuffer(numFrames);
            pCaptureClient->GetNextPacketSize(&packetLength);
        }

        // (b) CABLE'da bos yer kadar FIFO'dan veri yaz.
        UINT32 padding = 0;
        pRenderAudioClient->GetCurrentPadding(&padding);
        UINT32 framesAvail = renderBufferFrames - padding;       // CABLE'da bos frame
        UINT32 framesInFifo = (UINT32)(fifo.size() / blockAlign); // FIFO'da bekleyen frame
        UINT32 framesToWrite = framesAvail < framesInFifo ? framesAvail : framesInFifo;

        if (framesToWrite > 0) {
            BYTE* pRender = nullptr;
            if (SUCCEEDED(pRenderClient->GetBuffer(framesToWrite, &pRender))) {
                size_t bytes = (size_t)framesToWrite * blockAlign;
                memcpy(pRender, fifo.data(), bytes);
                pRenderClient->ReleaseBuffer(framesToWrite, 0);
                fifo.erase(fifo.begin(), fifo.begin() + bytes);  // tuketileni FIFO'dan sil
                totalRendered += framesToWrite;
            }
        }

        // Durum satiri (ayni satiri gunceller).
        wcout << L"\rAktariliyor ";
        PrintLevelBar(peak);
        wcout << L"  FIFO: " << framesInFifo << L" frame   "
              << L"capture: " << totalCaptured << L"  render: " << totalRendered << L"      ";
        wcout.flush();
    }

    // 7) Durdur.
    pCaptureAudioClient->Stop();
    pRenderAudioClient->Stop();
    if (_kbhit()) _getch();

    wcout << L"\n\nPassthrough durduruldu.\n";
    wcout << L"Toplam yakalanan: " << totalCaptured << L" frame, gonderilen: "
          << totalRendered << L" frame\n";

Cleanup:
    if (fmt)                CoTaskMemFree(fmt);
    if (pCaptureClient)     pCaptureClient->Release();
    if (pCaptureAudioClient)pCaptureAudioClient->Release();
    if (pRenderClient)      pRenderClient->Release();
    if (pRenderAudioClient) pRenderAudioClient->Release();
    if (pMicDevice)         pMicDevice->Release();
    if (pCableDevice)       pCableDevice->Release();
    if (pEnumerator)        pEnumerator->Release();
    CoUninitialize();

    wcout << L"\nCikmak icin Enter'a basin...";
    cin.get();
    return 0;
}
