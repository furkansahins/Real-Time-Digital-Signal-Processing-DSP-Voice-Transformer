# Real-Time DSP Voice Transformer

Windows üzerinde çalışan, düşük gecikmeli (low-latency), çok iş parçacıklı (multi-threaded)
gerçek zamanlı ses değiştirme yazılımı. Fiziksel mikrofondan gelen sesi WASAPI ile yakalar,
DSP algoritmalarıyla (pitch shifting, ring modulation, filtreleme) anında değiştirir ve
VB-Audio Virtual Cable üzerinden Discord/OBS/Skype gibi uygulamalara aktarır.

**Arel Üniversitesi — Bilgisayar Mühendisliği**
Ege Aksoy (230303013) · Furkan Şahin (230303009)

## Teknoloji
- **Dil:** C++ (manuel bellek yönetimi — gerçek zamanlı sesde GC duraksamaları kabul edilemez)
- **API:** WASAPI (Windows Audio Session API), Exclusive Mode
- **Ses yönlendirme:** VB-Audio Virtual Cable
- **Derleyici:** Visual Studio 2022 (v143, C++17, x64)

## Mimari (UML)
- `WASAPIManager` — mikrofon bağlantısını ve yakalamayı yönetir
- `AudioBuffer` — ham PCM ses verisini kısa süreli tutan dairesel tampon
- `DSPEngine` — aktif efekti (`Effect*`) tutar ve ses tamponunu işler (Strategy Pattern)
- `Effect` (taban) → `PitchFilter`, `RingModulator` türetilmiş efektler
- `VirtualAudioDriver` — işlenmiş sesi sanal cihaza (VB-Cable) gönderir

## Yol Haritası (adım adım)
- [x] **Adım 0** — Visual Studio Console App (C++) projesi + Git reposu
- [ ] **Adım 1** — Device Enumeration: tüm mikrofon/hoparlör/VB-Cable cihazlarını listele
- [ ] **Adım 2** — Capture: seçilen mikrofondan WASAPI Exclusive Mode ile ham PCM oku
- [ ] **Adım 3** — Render: yakalanan sesi VB-Cable sahte hoparlörüne kayıpsız yolla
- [ ] **Adım 4** — DSP: pitch shifting / ring modulation / filtreleme ekle

## Derleme & Çalıştırma
1. `VoiceTransformer.sln` dosyasını Visual Studio 2022 ile aç.
2. Konfigürasyonu **Debug | x64** seç.
3. **Ctrl+F5** (Start Without Debugging) ile çalıştır.

> Adım 1 çıktısında "CABLE" içeren cihazları göremiyorsan
> [VB-Audio Virtual Cable](https://vb-audio.com/Cable/) kurulu olmalı.
