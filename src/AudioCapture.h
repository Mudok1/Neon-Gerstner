#pragma once
/*
 * AudioCapture - Captura de audio del sistema (WASAPI Loopback)
 * Analiza frecuencias para reactividad visual
 */

#define NOMINMAX

#include <atomic>
#include <audioclient.h>
#include <cmath>
#include <mmdeviceapi.h>
#include <mutex>
#include <thread>
#include <windows.h>

class AudioCapture {
public:
  AudioCapture();
  ~AudioCapture();

  bool initialize();
  void start();
  void stop();

  // Valores normalizados 0.0 - 1.0, suavizados
  float getBass() const;
  float getMids() const;
  float getTreble() const;

private:
  void captureLoop();
  void processAudioData(const float *data, size_t samples);
  void applyFFT(const float *data, size_t samples);

  // WASAPI
  IMMDeviceEnumerator *deviceEnumerator = nullptr;
  IMMDevice *device = nullptr;
  IAudioClient *audioClient = nullptr;
  IAudioCaptureClient *captureClient = nullptr;
  WAVEFORMATEX *waveFormat = nullptr;

  // Thread
  std::thread captureThread;
  std::atomic<bool> running{false};

  // Audio analysis results (thread-safe)
  mutable std::mutex dataMutex;
  float bass = 0.0f;
  float mids = 0.0f;
  float treble = 0.0f;

  // Smoothing
  float smoothBass = 0.0f;
  float smoothMids = 0.0f;
  float smoothTreble = 0.0f;
  static constexpr float SMOOTHING =
      0.15f; // Slower decay for smoother fade-out
};
