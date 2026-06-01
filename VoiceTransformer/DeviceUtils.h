// ============================================================================
//  DeviceUtils.h
//  WASAPI ses cihazlariyla ilgili kucuk yardimci (free) fonksiyonlar.
//  Hicbir sinifa ait degildir; hem main hem de diger siniflar kullanabilsin
//  diye burada toplanmistir.
// ============================================================================
#pragma once

#include <windows.h>
#include <mmdeviceapi.h>                     // IMMDevice, IMMDeviceEnumerator
#include <functiondiscoverykeys_devpkey.h>   // PKEY_Device_FriendlyName
#include <iostream>
#include <string>

using namespace std;

// ----------------------------------------------------------------------------
// Bir ses cihazinin dostane (insan-okur) ismini dondurur.
// Ornek: "Microphone (Realtek(R) Audio)"
// ----------------------------------------------------------------------------
inline wstring GetDeviceName(IMMDevice* pDevice)
{
    wstring result = L"(bilinmeyen cihaz)";
    IPropertyStore* pProps = nullptr;
    if (SUCCEEDED(pDevice->OpenPropertyStore(STGM_READ, &pProps))) {
        PROPVARIANT v;
        PropVariantInit(&v);
        if (SUCCEEDED(pProps->GetValue(PKEY_Device_FriendlyName, &v)) && v.pwszVal)
            result = v.pwszVal;
        PropVariantClear(&v);
        pProps->Release();
    }
    return result;
}

// ----------------------------------------------------------------------------
// Bir ismin VB-Cable sanal cihazina ait olup olmadigini soyler.
// (Isimde "CABLE" veya "VB-Audio" geciyorsa sanal cihazdir.)
// ----------------------------------------------------------------------------
inline bool IsVBCableName(const wstring& name)
{
    return name.find(L"CABLE") != wstring::npos ||
           name.find(L"VB-Audio") != wstring::npos;
}

// ----------------------------------------------------------------------------
// CIKIS (render) cihazlari arasinda ismi 'substr' iceren ILK cihazi bulur.
// Ornek kullanim: FindRenderDeviceByName(enum, L"CABLE Input")
// Donen cihazin sahipligi cagirana aittir (Release etmek cagiranin gorevi).
// ----------------------------------------------------------------------------
inline IMMDevice* FindRenderDeviceByName(IMMDeviceEnumerator* pEnum, const wchar_t* substr)
{
    IMMDeviceCollection* pCol = nullptr;
    if (FAILED(pEnum->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCol)))
        return nullptr;

    UINT count = 0;
    pCol->GetCount(&count);

    IMMDevice* found = nullptr;
    for (UINT i = 0; i < count; ++i) {
        IMMDevice* pDev = nullptr;
        if (FAILED(pCol->Item(i, &pDev))) continue;
        if (GetDeviceName(pDev).find(substr) != wstring::npos) {
            found = pDev;   // bulundu; sahipligi cagirana birakiyoruz
            break;
        }
        pDev->Release();
    }
    pCol->Release();
    return found;
}

// ----------------------------------------------------------------------------
// KAYNAK mikrofonu kullaniciya sectirir.
//
// NEDEN: Windows'un varsayilan mikrofonu "CABLE Output" yapilmis olabilir.
// Eger varsayilani kullanirsak program CABLE'dan okuyup CABLE'a yazar ve
// dogrudan SONSUZ DIJITAL GERI BESLEME (yanki) olusur. Bu yuzden fiziksel
// mikrofonu kullaniciya acikca sectiriyor, CABLE cihazlarini uyariyoruz.
//
// Donen cihazin sahipligi cagirana aittir.
// ----------------------------------------------------------------------------
inline IMMDevice* SelectCaptureDevice(IMMDeviceEnumerator* pEnum)
{
    IMMDeviceCollection* pCol = nullptr;
    if (FAILED(pEnum->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &pCol)))
        return nullptr;

    UINT count = 0;
    pCol->GetCount(&count);
    if (count == 0) { pCol->Release(); return nullptr; }

    wcout << L"\nKullanilabilir mikrofonlar (KAYNAK olarak secin):\n";
    int firstNonCable = -1;
    for (UINT i = 0; i < count; ++i) {
        IMMDevice* d = nullptr;
        if (FAILED(pCol->Item(i, &d))) continue;
        wstring name = GetDeviceName(d);
        wcout << L"  [" << i << L"] " << name;
        if (IsVBCableName(name))
            wcout << L"   <-- VB-CABLE (KAYNAK SECMEYIN! sonsuz yanki olur)";
        else if (firstNonCable < 0)
            firstNonCable = (int)i;   // ilk fiziksel mikrofonu varsayilan oneri yap
        wcout << L"\n";
        d->Release();
    }
    if (firstNonCable < 0) firstNonCable = 0;

    wcout << L"Kaynak mikrofon index girin (Enter = " << firstNonCable << L"): ";
    int sel = firstNonCable;
    wstring line;
    getline(wcin, line);
    if (!line.empty()) { try { sel = stoi(line); } catch (...) { sel = firstNonCable; } }
    if (sel < 0 || (UINT)sel >= count) sel = firstNonCable;

    IMMDevice* chosen = nullptr;
    pCol->Item((UINT)sel, &chosen);
    pCol->Release();
    return chosen;
}
