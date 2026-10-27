# Dawsoftware

A C++ desktop digital audio workstation built with JUCE. The first release
provides multitrack audio playback, a sample library importer, per-track gain
and mute controls, and VST3 effect hosting.

## Build

Requirements: CMake 3.22+, a C++17 compiler, and the platform's audio and GUI
development libraries. On Ubuntu/Debian, install the system packages first:

```sh
sudo apt install libasound2-dev libfreetype6-dev libfontconfig1-dev libgl1-mesa-dev \
	libgtk-3-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev \
	libxcomposite-dev libxext-dev libxrender-dev
```

JUCE is fetched automatically during configuration.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Run the `Dawsoftware` executable from the build directory (on macOS, use the
generated app bundle). The sample library is stored under the user's
`Dawsoftware/Samples` application-data directory.

To load an effect, select a track, choose **Load VST3**, and select the plugin's
`.vst3` bundle directory. VST3 binaries must match the host architecture and
are subject to their own licensing terms.
