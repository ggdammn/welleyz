# Changelog

## 0.4.0

- Reset the product version, synth identity, build targets and semantic parameter IDs. DAW projects saved with previous parameter IDs do not load into this plugin.
- Save `.wlyz` JSON presets. Embed factory JSON in `src/resources/` and keep user sounds in `Documents\welleyz presets`.
- Preserve the RANDOM, OSC and FX interface, selectable accent, zoom, sound engine and MIDI controls.
- Add an in-editor Manual button to VST3 and Standalone. Standalone gains stereo WAV recording with a save/discard prompt, an accented metronome, BPM entry and beat-synchronized TAPS delay.
- Allow selection of the VST3 install directory in Setup. Use VS Code CMake presets, Python release tooling, clang-format and clang-tidy; ship Markdown documentation.
- Embed new header SVG icons, use one tempo/metronome settings panel, keep the upper-right buttons uniform and pulse REC red during recording.

## 4.0.4

- Move the OSC controls down, enlarge and lower their waveform previews, and lower the OSC-tab power lights.
- Raise the MASTER + ARP dropdowns to leave breathing room under them.
- Add an independently switchable background-noise layer under TUBE + LO-FI with a level knob, texture selector and waveform display. Includes Vinyl, Cassette, Studio, Wire, Tape Hiss, Dust, Radio, Rain and Crackle; starts off for older projects and presets.
- Add Background noise to RANDOMIZE OPTIONS; disable it to keep the noise setting unchanged while generating sounds.
- Include an icon-branded updater that checks GitHub Releases and runs the newer official installer.

## 4.0.3

- Smooth legato pitch transitions between overlapping piano-roll MIDI notes without retriggering the voice. FL Studio's native slide-note flags are not sent to VST3 instruments; use overlapping regular notes with Legato/PORTA or automate channel pitch.
- Save SOUND TYPE as a host-visible parameter and migrate selections from older projects, so closing and reopening the VST3 editor retains the selection.
- Bring oscillator and tube knob labels closer to their controls, align power lamps and refine FX knob spacing.

## 4.0.2

- Added MIDI legato/portamento glide for overlapping notes (FL Studio slide notes remain native-only and are not sent to VST3).
- Persist the selected SOUND TYPE in the host project and BACK history.
- Added a Voices toggle to RANDOMIZE OPTIONS.
- Renamed the user preset folder to `welleyz presets`.
- Enlarged interface fonts and aligned OSC/tube labels and panel spacing.
- Bumped version to 4.0.2.

## 4.0.0

- Added independent ON/OFF lamps for OSC 1–3, envelope, filter and tube/lo-fi; old presets keep those sections enabled.
- Added a Likes preset filter, C5 keyboard viewport and black octave labels.
- Persist the selected UI zoom between editor openings and refine focus restoration after RANDOMIZE in VST3 hosts.
- Added a branded graphical uninstaller launcher and metallic wordmark.
- Renamed the instrument to welleyz by shadowave and added branded icons to the standalone application and installer.
- Added a one-turn loading animation to RANDOMIZE and a themed icon to every sound type.
- Restored editor keyboard focus after RANDOMIZE so host computer-keyboard MIDI continues without another click.
- Refreshed the glass interface and bundled Syne, Outfit and Sarina fonts.
- Preserved existing parameter IDs and preset compatibility.

## 3.0.0

- Complete redesign with a modern dark interface, user-selectable accent colour, and RANDOM / OSC / FX pages.
- Added searchable preset browsing, favourites, and configurable Porta / FX / Arp participation in RANDOMIZE.
- Added embedded SVG interface icons, on-screen Pitch Bend and Mod Wheel, and incoming MIDI Pitch Bend / CC1 support.

## 2.0.1

- Open at half-size on laptop-sized displays without covering the DAW; keep manually selected zoom levels.
- BACK restores up to ten previously generated or selected sounds.
- Play the on-screen keyboard from computer keys while the plugin has focus.

## 2.0.0

- First packaged beta release.
- Three oscillators with unison voices and detune.
- Serum-style portamento modes and time control.
- Tube warmth control, FX rack, preset browser, and Standalone application.
