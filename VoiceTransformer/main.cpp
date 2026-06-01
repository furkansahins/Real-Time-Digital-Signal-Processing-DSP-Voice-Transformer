// ============================================================================
//  Real-Time DSP Voice Transformer
//  Adim 1: Device Enumeration (Cihaz Kesfi)
//
//  Bu program WASAPI'nin IMMDeviceEnumerator arayuzunu kullanarak
//  sisteme bagli TUM aktif ses cihazlarini (mikrofonlar, hoparlorler ve
//  VB-Cable sanal cihazlari) bulup isimlerini konsola yazdirir.
//
//  Bir sonraki adim (Adim 2 - Capture) icin secilecek cihazi burada gormeliyiz.
// ============================================================================

#include <windows.h>
#include <mmdeviceapi.h>      // IMMDeviceEnumerator, IMMDevice, IMMDeviceCollection
#include <functiondiscoverykeys_devpkey.h>  // PKEY_Device_FriendlyName
#include <iostream>
#include <string>

using namespace std;

// Hata kontrolu icin basit yardimci makro: HRESULT basarisizsa mesaj basip cikar.
#define CHECK_HR(hr, msg)                                              \
    if (FAILED(hr)) {                                                  \
        wcerr << L"[HATA] " << msg << L" (HRESULT=0x"                  \
              << hex << (hr) << L")\n";                                \
        goto Cleanup;                                                  \
    }

// Bir cihaz koleksiyonundaki (eRender = hoparlorler, eCapture = mikrofonlar)
// tum cihazlarin dostane (friendly) isimlerini yazdirir.
void ListDevices(IMMDeviceEnumerator* pEnumerator, EDataFlow flow, const wchar_t* baslik)
{
    wcout << L"\n==== " << baslik << L" ====\n";

    IMMDeviceCollection* pCollection = nullptr;
    // Sadece AKTIF (DEVICE_STATE_ACTIVE) cihazlari listele.
    HRESULT hr = pEnumerator->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, &pCollection);
    if (FAILED(hr)) {
        wcerr << L"  [HATA] EnumAudioEndpoints basarisiz.\n";
        return;
    }

    UINT count = 0;
    pCollection->GetCount(&count);
    if (count == 0) {
        wcout << L"  (Aktif cihaz bulunamadi)\n";
    }

    for (UINT i = 0; i < count; ++i) {
        IMMDevice* pDevice = nullptr;
        if (FAILED(pCollection->Item(i, &pDevice))) continue;

        IPropertyStore* pProps = nullptr;
        if (SUCCEEDED(pDevice->OpenPropertyStore(STGM_READ, &pProps))) {
            PROPVARIANT varName;
            PropVariantInit(&varName);

            if (SUCCEEDED(pProps->GetValue(PKEY_Device_FriendlyName, &varName))) {
                wstring name = varName.pwszVal ? varName.pwszVal : L"(isimsiz)";

                // VB-Cable cihazlarini isaretle (isimlerinde "CABLE" gecer).
                bool isVBCable = (name.find(L"CABLE") != wstring::npos ||
                                  name.find(L"VB-Audio") != wstring::npos);

                wcout << L"  [" << i << L"] " << name;
                if (isVBCable) wcout << L"   <-- VB-CABLE (sanal cihaz)";
                wcout << L"\n";
            }
            PropVariantClear(&varName);
            pProps->Release();
        }
        pDevice->Release();
    }

    pCollection->Release();
}

int main()
{
    // Konsolun Turkce/Unicode karakterleri dogru gostermesi icin.
    SetConsoleOutputCP(CP_UTF8);

    wcout << L"Real-Time DSP Voice Transformer - Adim 1: Cihaz Kesfi\n";
    wcout << L"=====================================================\n";

    IMMDeviceEnumerator* pEnumerator = nullptr;

    // 1) COM kutuphanesini baslat (WASAPI bir COM API'sidir).
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    CHECK_HR(hr, L"CoInitializeEx basarisiz");

    // 2) Cihaz numaralandiricisini (enumerator) olustur.
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    CHECK_HR(hr, L"MMDeviceEnumerator olusturulamadi");

    // 3) Once cikis cihazlari (hoparlorler) - VB-Cable "Input" burada gorunur.
    ListDevices(pEnumerator, eRender, L"CIKIS CIHAZLARI (Hoparlorler / Playback)");

    // 4) Sonra giris cihazlari (mikrofonlar) - VB-Cable "Output" burada gorunur.
    ListDevices(pEnumerator, eCapture, L"GIRIS CIHAZLARI (Mikrofonlar / Recording)");

    wcout << L"\nKesif tamamlandi.\n";

Cleanup:
    if (pEnumerator) pEnumerator->Release();
    CoUninitialize();

    wcout << L"\nCikmak icin Enter'a basin...";
    cin.get();
    return 0;
}
