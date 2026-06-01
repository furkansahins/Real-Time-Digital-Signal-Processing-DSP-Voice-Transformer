// ============================================================================
//  Real-Time DSP Voice Transformer
//  Adim 2: Capture (Sesi Yakalama)
//
//  Varsayilan mikrofondan WASAPI (Shared Mode) ile ham PCM ses verisini okur.
//  Her ses paketi geldiginde "Ses paketi alindi: <Hz>, <frame>" yazar ve
//  basit bir ses seviyesi (level meter) cubugu cizer; boylece mikrofonun
//  gercekten ses yakaladigini gozle goruruz.
//
//  Mod: Shared Mode (kolay/guvenli baslangic). Akis kusursuz olunca Adim
//  ilerisinde Exclusive Mode'a gecilecek (raporun nihai hedefi).
//
//  Durdurmak icin herhangi bir tusa bas.
// ============================================================================

#include <windows.h>
#include <mmdeviceapi.h>      // IMMDeviceEnumerator, IMMDevice
#include <audioclient.h>      // IAudioClient, IAudioCaptureClient
#include <functiondiscoverykeys_devpkey.h>  // PKEY_Device_FriendlyName
#include <iostream>
#include <string>
#include <conio.h>           // _kbhit, _getch (tusla durdurma)

using namespace std;

// 1 saniye = 10.000.000 "reference time" birimi (100 ns).
#define REFTIMES_PER_SEC 10000000

// Hata kontrol makrosu: HRESULT basarisizsa mesaj basip Cleanup'a atla.
#define CHECK_HR(hr, msg)                                              \
    if (FAILED(hr)) {                                                  \
        wcerr << L"[HATA] " << msg << L" (HRESULT=0x"                  \
              << hex << (hr) << L")\n";                                \
        goto Cleanup;                                                  \
    }

// Verilen cihazin dostane (friendly) ismini dondurur.
wstring GetDeviceName(IMMDevice* pDevice)
{
    wstring result = L"(bilinmeyen cihaz)";
    IPropertyStore* pProps = nullptr;
    if (SUCCEEDED(pDevice->OpenPropertyStore(STGM_READ, &pProps))) {
        PROPVARIANT varName;
        PropVariantInit(&varName);
        if (SUCCEEDED(pProps->GetValue(PKEY_Device_FriendlyName, &varName)) && varName.pwszVal) {
            result = varName.pwszVal;
        }
        PropVariantClear(&varName);
        pProps->Release();
    }
    return result;
}

// Ses paketindeki en yuksek genligi (0.0 - 1.0) hesaplar. Mikrofonun ses
// alip almadigini gormek icin kullanilir. Shared Mode'da mix format genelde
// 32-bit float'tir; 16-bit PCM ihtimaline karsi onu da destekliyoruz.
float ComputePeak(const BYTE* data, UINT32 numFrames, const WAVEFORMATEX* fmt)
{
    float peak = 0.0f;
    UINT32 sampleCount = numFrames * fmt->nChannels;

    if (fmt->wBitsPerSample == 32) {
        // 32-bit IEEE float (-1.0 .. +1.0)
        const float* samples = reinterpret_cast<const float*>(data);
        for (UINT32 i = 0; i < sampleCount; ++i) {
            float a = samples[i] < 0 ? -samples[i] : samples[i];
            if (a > peak) peak = a;
        }
    }
    else if (fmt->wBitsPerSample == 16) {
        // 16-bit signed PCM (-32768 .. +32767)
        const short* samples = reinterpret_cast<const short*>(data);
        for (UINT32 i = 0; i < sampleCount; ++i) {
            float a = (samples[i] < 0 ? -(float)samples[i] : (float)samples[i]) / 32768.0f;
            if (a > peak) peak = a;
        }
    }
    return peak;
}

// peak (0..1) degerini "#####-----" gibi bir cubuga cevirip basar.
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
    wcout << L"Real-Time DSP Voice Transformer - Adim 2: Capture (Sesi Yakalama)\n";
    wcout << L"================================================================\n";

    // TUM yerel degiskenleri en uste bildiriyoruz. Sebep: CHECK_HR icindeki
    // "goto Cleanup", aralarda baslatilan degiskenlerin uzerinden atlarsa
    // C++ "bypasses initialization" derleme hatasi verir.
    IMMDeviceEnumerator* pEnumerator = nullptr;
    IMMDevice* pDevice = nullptr;
    IAudioClient* pAudioClient = nullptr;
    IAudioCaptureClient* pCaptureClient = nullptr;
    WAVEFORMATEX* pwfx = nullptr;

    UINT32 bufferFrameCount = 0;
    DWORD  sleepMs = 1;
    unsigned long long totalPackets = 0;
    unsigned long long totalFrames = 0;

    // 1) COM baslat.
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    CHECK_HR(hr, L"CoInitializeEx basarisiz");

    // 2) Enumerator olustur.
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    CHECK_HR(hr, L"MMDeviceEnumerator olusturulamadi");

    // 3) Varsayilan GIRIS (mikrofon) cihazini al. eCapture = giris, eConsole = genel kullanim.
    hr = pEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &pDevice);
    CHECK_HR(hr, L"Varsayilan mikrofon bulunamadi");
    wcout << L"Kaynak mikrofon: " << GetDeviceName(pDevice) << L"\n";

    // 4) Cihazdan IAudioClient'i etkinlestir (donanima konusan ana arayuz).
    hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient);
    CHECK_HR(hr, L"IAudioClient etkinlestirilemedi");

    // 5) Cihazin mix formatini al (Shared Mode bu formati dayatir).
    hr = pAudioClient->GetMixFormat(&pwfx);
    CHECK_HR(hr, L"GetMixFormat basarisiz");

    wcout << L"Format: " << pwfx->nSamplesPerSec << L" Hz, "
          << pwfx->nChannels << L" kanal, "
          << pwfx->wBitsPerSample << L" bit\n";

    // 6) AudioClient'i Shared Mode'da baslat. 1 saniyelik tampon istiyoruz.
    hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0,
                                  REFTIMES_PER_SEC, 0, pwfx, nullptr);
    CHECK_HR(hr, L"IAudioClient.Initialize basarisiz");

    // 7) Yakalama (capture) servisini al.
    hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);
    CHECK_HR(hr, L"IAudioCaptureClient alinamadi");

    // Tampon kac frame? Buradan polling icin bekleme suresini hesaplayacagiz.
    pAudioClient->GetBufferSize(&bufferFrameCount);
    // Tamponun yarisi kadar uyuyup paketleri toplayacagiz (yarim = guvenli pay).
    sleepMs = (DWORD)((double)bufferFrameCount / pwfx->nSamplesPerSec * 1000.0 / 2.0);
    if (sleepMs < 1) sleepMs = 1;

    // 8) Yakalamayi baslat.
    hr = pAudioClient->Start();
    CHECK_HR(hr, L"IAudioClient.Start basarisiz");

    wcout << L"\nYakalama basladi. Konusun! (Durdurmak icin bir tusa basin)\n\n";

    // 9) Ana yakalama dongusu (polling). Bir tusa basilana kadar surer.
    while (!_kbhit()) {
        Sleep(sleepMs);

        UINT32 packetLength = 0;
        hr = pCaptureClient->GetNextPacketSize(&packetLength);
        if (FAILED(hr)) break;

        // Bekleyen tum paketleri sirayla bosalt.
        while (packetLength != 0) {
            BYTE* pData = nullptr;
            UINT32 numFrames = 0;
            DWORD flags = 0;

            hr = pCaptureClient->GetBuffer(&pData, &numFrames, &flags, nullptr, nullptr);
            if (FAILED(hr)) break;

            // SILENT bayragi: cihaz sessizlik gonderiyor (pData gecersiz olabilir).
            float peak = 0.0f;
            if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT) && pData) {
                peak = ComputePeak(pData, numFrames, pwfx);
            }

            totalPackets++;
            totalFrames += numFrames;

            // Bilgi satiri: ayni satiri guncelleyerek konsolu temiz tutuyoruz.
            wcout << L"\rSes paketi alindi: " << pwfx->nSamplesPerSec << L" Hz, "
                  << numFrames << L" frame  ";
            PrintLevelBar(peak);
            wcout << L"  (toplam paket: " << totalPackets << L")     ";
            wcout.flush();

            // ÖNEMLI: aldigimiz frame'leri serbest birak, yoksa tampon dolar.
            pCaptureClient->ReleaseBuffer(numFrames);

            pCaptureClient->GetNextPacketSize(&packetLength);
        }
    }

    // 10) Durdur.
    pAudioClient->Stop();
    if (_kbhit()) _getch();  // basilan tusu tampondan temizle

    wcout << L"\n\nYakalama durduruldu.\n";
    wcout << L"Toplam paket: " << totalPackets << L", toplam frame: " << totalFrames << L"\n";

Cleanup:
    if (pwfx)           CoTaskMemFree(pwfx);
    if (pCaptureClient) pCaptureClient->Release();
    if (pAudioClient)   pAudioClient->Release();
    if (pDevice)        pDevice->Release();
    if (pEnumerator)    pEnumerator->Release();
    CoUninitialize();

    wcout << L"\nCikmak icin Enter'a basin...";
    cin.get();
    return 0;
}
