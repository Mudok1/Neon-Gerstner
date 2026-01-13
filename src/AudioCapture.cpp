#include "AudioCapture.h"
#include <complex>
#include <iostream>
#include <vector>

const double PI = 3.141592653589793238460;

// Simple FFT implementation
typedef std::complex<double> Complex;
typedef std::vector<Complex> CArray;

void fft(CArray &x) {
  const size_t N = x.size();
  if (N <= 1)
    return;

  // slice not supports std::vector directly, manual split
  CArray even(N / 2);
  CArray odd(N / 2);

  for (size_t i = 0; i < N / 2; ++i) {
    even[i] = x[2 * i];
    odd[i] = x[2 * i + 1];
  }

  fft(even);
  fft(odd);

  for (size_t k = 0; k < N / 2; ++k) {
    Complex t = std::polar(1.0, -2 * PI * k / N) * odd[k];
    x[k] = even[k] + t;
    x[k + N / 2] = even[k] - t;
  }
}

AudioCapture::AudioCapture() {}

AudioCapture::~AudioCapture() { stop(); }

bool AudioCapture::initialize() {
  // La inicialización real ocurre en el thread de captura para COM
  return true;
}

void AudioCapture::start() {
  if (running)
    return;
  running = true;
  captureThread = std::thread(&AudioCapture::captureLoop, this);
}

void AudioCapture::stop() {
  running = false;
  if (captureThread.joinable()) {
    captureThread.join();
  }
}

float AudioCapture::getBass() const {
  std::lock_guard<std::mutex> lock(dataMutex);
  return smoothBass;
}

float AudioCapture::getMids() const {
  std::lock_guard<std::mutex> lock(dataMutex);
  return smoothMids;
}

float AudioCapture::getTreble() const {
  std::lock_guard<std::mutex> lock(dataMutex);
  return smoothTreble;
}

void AudioCapture::captureLoop() {
  HRESULT hr = CoInitialize(nullptr);
  if (FAILED(hr))
    return;

  hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                        __uuidof(IMMDeviceEnumerator),
                        (void **)&deviceEnumerator);

  if (SUCCEEDED(hr)) {
    hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
  }

  if (SUCCEEDED(hr)) {
    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                          (void **)&audioClient);
  }

  if (SUCCEEDED(hr)) {
    hr = audioClient->GetMixFormat(&waveFormat);
  }

  if (SUCCEEDED(hr)) {
    // Inicializar en modo LOOPBACK
    hr = audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                 AUDCLNT_STREAMFLAGS_LOOPBACK, 10000000, 0,
                                 waveFormat, nullptr);
  }

  if (SUCCEEDED(hr)) {
    hr = audioClient->GetService(__uuidof(IAudioCaptureClient),
                                 (void **)&captureClient);
  }

  if (SUCCEEDED(hr)) {
    audioClient->Start();
  } else {
    std::cerr << "Failed to initialize WASAPI loopback: " << hr << std::endl;
    running = false;
    CoUninitialize();
    return;
  }

  UINT32 packetLength = 0;
  while (running) {
    hr = captureClient->GetNextPacketSize(&packetLength);

    while (packetLength != 0) {
      BYTE *pData;
      UINT32 numFramesAvailable;
      DWORD flags;

      hr = captureClient->GetBuffer(&pData, &numFramesAvailable, &flags,
                                    nullptr, nullptr);
      if (FAILED(hr))
        break;

      if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
        // Silencio
      } else {
        // Asumimos formato float estéreo (es lo común en loopback compartido)
        // waveFormat->nChannels canales
        float *pFloatData = (float *)pData;
        size_t samplesToProcess = 1024; // Tamaño ventana FFT

        // Buffer temporal para FFT
        static std::vector<float> fftBuffer;

        // Mezclar canales a mono y acumular
        int channels = waveFormat->nChannels;
        for (UINT32 i = 0; i < numFramesAvailable; i++) {
          float sample = 0;
          for (int c = 0; c < channels; c++) {
            sample += pFloatData[i * channels + c];
          }
          sample /= channels;
          fftBuffer.push_back(sample);

          if (fftBuffer.size() >= samplesToProcess) {
            applyFFT(fftBuffer.data(), samplesToProcess);
            fftBuffer.clear();
          }
        }
      }

      captureClient->ReleaseBuffer(numFramesAvailable);
      captureClient->GetNextPacketSize(&packetLength);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  audioClient->Stop();
  if (waveFormat)
    CoTaskMemFree(waveFormat);
  if (captureClient)
    captureClient->Release();
  if (audioClient)
    audioClient->Release();
  if (device)
    device->Release();
  if (deviceEnumerator)
    deviceEnumerator->Release();

  CoUninitialize();
}

void AudioCapture::applyFFT(const float *data, size_t samples) {
  if (samples == 0)
    return;

  // Preparar datos complejos para FFT
  CArray complexData(samples);
  for (size_t i = 0; i < samples; ++i) {
    complexData[i] = Complex(data[i], 0);
  }

  // Ejecutar FFT
  fft(complexData);

  // Analizar bandas
  // Sample rate típico 44100 o 48000 Hz
  // FreqIndex = Index * SampleRate / N
  // Con N=1024, SampleRate=48000:
  // Bin width = 46.8 Hz

  float currentBass = 0.0f;
  float currentMids = 0.0f;
  float currentTreble = 0.0f;

  // Rangos aproximados bins
  // Bass: 0 - 5 (0 - ~250Hz)
  // Mids: 6 - 40 (~250Hz - ~2000Hz)
  // Treble: 41 - 250 (~2000Hz - ~12000Hz)

  for (size_t i = 0; i < samples / 2; ++i) {
    float magnitude = std::abs(complexData[i]);

    if (i > 0 && i <= 5)
      currentBass += magnitude;
    else if (i > 5 && i <= 40)
      currentMids += magnitude;
    else if (i > 40 && i < 250)
      currentTreble += magnitude;
  }

  // Normalizar un poco (valores empíricos para visualización)
  currentBass /= 100.0f;
  currentMids /= 200.0f;
  currentTreble /= 300.0f;

  // Clamp 0-1
  currentBass = std::min(1.0f, currentBass);
  currentMids = std::min(1.0f, currentMids);
  currentTreble = std::min(1.0f, currentTreble);

  // Update thread-safe values with smoothing
  std::lock_guard<std::mutex> lock(dataMutex);

  // Attack rápido, decay lento
  if (currentBass > smoothBass)
    smoothBass = currentBass;
  else
    smoothBass += (currentBass - smoothBass) * SMOOTHING;

  if (currentMids > smoothMids)
    smoothMids = currentMids;
  else
    smoothMids += (currentMids - smoothMids) * SMOOTHING;

  if (currentTreble > smoothTreble)
    smoothTreble = currentTreble;
  else
    smoothTreble += (currentTreble - smoothTreble) * SMOOTHING;
}
