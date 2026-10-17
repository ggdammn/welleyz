![Uploading welleyz cover.png…]()



# About

[![License: AGPLv3](https://img.shields.io/badge/License-AGPLv3-blue.svg)](LICENSE)

welleyz is a chiptune synthesizer, which mainly focuses on constrained randomization.
It runs as a VST3 instrument or a standalone application.

## Features

- Three oscillators: Sine, Square, Saw, Triangle, Pulse and Noise; 1–8 unison
  voices per oscillator.
- Amp envelope, filter, tube/lo-fi processing, vibrato, arpeggiator and
  Off/Always/Legato portamento. Legato glide requires overlapping MIDI notes.
- FX rack: FILTER, HAZE, TAPS delay and SPACE reverb. Nine selectable
  background-noise textures have separate level and power controls.
- RANDOM generates sounds by category. SETTINGS selects which parameters it
  may change; BACK recalls up to ten previous sounds in the current session.
- Factory and user `.wlyz` presets; name search, category filters and Likes.
- On-screen keyboard, computer-key input, MIDI Pitch Bend and CC1. Standalone
  offers audio/MIDI device selection, a 40–240 BPM metronome and WAV recording.

## Install

Download the Windows x64 package from [Releases](https://github.com/ggdammn/welleyz/releases)
and run the included setup executable. The VST3 directory is selectable
(default is `%ProgramFiles%\Common Files\VST3`); standalone installs under
`%ProgramFiles%\shadowave\welleyz`.

User presets are stored in `Documents\welleyz presets`; standalone WAV files
default to `Documents\welleyz recordings`. Both folders survive uninstall.

## Build

Windows: CMake 3.25+, Visual Studio 2022 with a C++20 toolchain. CMake fetches
JUCE 8.0.8 during configuration.

```cmd
cmake --preset windows-release
cmake --build --preset windows-release
```

For a tested ZIP and installer, install Inno Setup 6 and run:

```cmd
python scripts/release.py
```

The release script builds and runs the test executables, then writes the
package to `local\distributions\release\`. Update checks in the plugin are
disabled unless `WELLEYZ_UPDATE_URL` is set to a GitHub Releases API endpoint
at build time.

## License

[AGPLv3](LICENSE). JUCE is used under its AGPLv3 license option.
