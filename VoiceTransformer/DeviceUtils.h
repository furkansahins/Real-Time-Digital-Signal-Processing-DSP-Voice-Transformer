#pragma once

#include <windows.h>
#include <mmdeviceapi.h>                     
#include <functiondiscoverykeys_devpkey.h>   
#include <iostream>
#include <string>

using namespace std;


inline wstring GetDeviceName(IMMDevice* pDevice)
{
    wstring result = L"(Unknown Device)";
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


inline bool IsVBCableName(const wstring& name)
{
    return name.find(L"CABLE") != wstring::npos ||
           name.find(L"VB-Audio") != wstring::npos;
}

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
            found = pDev;
            break;
        }
        pDev->Release();
    }
    pCol->Release();
    return found;
}


inline IMMDevice* SelectCaptureDevice(IMMDeviceEnumerator* pEnum)
{
    IMMDeviceCollection* pCol = nullptr;
    if (FAILED(pEnum->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &pCol)))
        return nullptr;

    UINT count = 0;
    pCol->GetCount(&count);
    if (count == 0) { pCol->Release(); return nullptr; }

    wcout << L"\nAvailable microphones (select one as SOURCE):\n";
    int firstNonCable = -1;
    for (UINT i = 0; i < count; ++i) {
        IMMDevice* d = nullptr;
        if (FAILED(pCol->Item(i, &d))) continue;
        wstring name = GetDeviceName(d);
        wcout << L"  [" << i << L"] " << name;
        if (IsVBCableName(name))
            wcout << L"   <-- VB-CABLE DO NOT select as SOURCE! causes infinite echo";
        else if (firstNonCable < 0)
            firstNonCable = (int)i;
        wcout << L"\n";
        d->Release();
    }
    if (firstNonCable < 0) firstNonCable = 0;

    wcout << L"Enter source microphone index (Enter = " << firstNonCable << L"): ";
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
