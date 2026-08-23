# Built-in alarm sounds

All built-in alarm files are mono, signed 16-bit PCM WAV. Firmware streams them
directly to I2S/ES8311 without an MP3 decoder. Most use 22.05 kHz;
`kanong.wav` uses 16 kHz and keeps the first 10 seconds of the original
43-second sound so the complete set fits in the SPIFFS partition.

PlatformIO packages this directory as the SPIFFS image; flash it with
`pio run -t uploadfs`.
