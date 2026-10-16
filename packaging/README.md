# welleyz 0.4.0 — Windows x64

This package installs both the VST3 instrument and the Standalone application.

## Install

1. Close FL Studio and any other DAW.
2. Run `welleyz-0.4.0-setup.exe` and approve the Windows administrator prompt. Choose the directory scanned by your DAW for VST3 plugins (default: `%ProgramFiles%\Common Files\VST3`). The installer also installs Standalone and creates a Start Menu shortcut.
3. Open FL Studio and run **Options → Manage plugins → Find installed plugins**.
4. Add **welleyz** to an instrument channel. MIDI/audio playback is controlled by the host's selected audio device.

To use the standalone app, open **Start Menu → shadowave → welleyz**. Choose an audio and MIDI device in its device settings. Do not run Standalone alongside a DAW when diagnosing audio output: they are separate synth instances and can use different audio devices at the same time.

On smaller displays, the plugin opens at 75% scale to leave room for the DAW. Use **ZOOM** to choose and remember another size. **BACK** restores up to ten previous sounds after RANDOM or preset selection (until the plugin instance closes). To play with a computer keyboard, focus the plugin and use `A W S E D F T G Y H U J K O L P ;` (C4–E5); some DAWs require enabling keyboard focus for the plugin window.

## Uninstall

Use **Windows Settings → Apps → Installed apps → welleyz → Uninstall**, or run **Uninstall welleyz** from the Start Menu. The standalone `welleyz-uninstall.exe` in this package can also launch the graphical uninstaller after installation. User presets in `Documents\welleyz presets` and WAV recordings in `Documents\welleyz recordings` are kept.

## Included

- `welleyz-0.4.0-setup.exe` — installer (with matching uninstall icon)
- `welleyz-uninstall.exe` — standalone graphical uninstaller launcher
- `welleyz-update.exe` — graphical updater (also installed in the Start Menu)
- `VST3\welleyz.vst3` — plugin for FL Studio and other VST3 hosts
- `Standalone\welleyz.exe` — standalone synth
- `Setup.bat` — convenience launcher for the graphical installer
- `release.json` — version, package hashes, and update-check configuration
- `Manual.md` — user guide
- `THIRD-PARTY-NOTICES.md` — bundled font attribution and license

## Update notifications

Update checking is off until `WELLEYZ_UPDATE_URL` points to the 0.x release line. To enable it, rebuild with:

```cmd
cmake -S . -B build -DWELLEYZ_UPDATE_URL="https://api.github.com/repos/OWNER/REPO/releases/latest"
```

The standalone updater can remember a GitHub Releases API URL. When a newer release includes a `welleyz-...-setup.exe` asset, **Download and install** downloads and runs the installer. Close your DAW before installing. The in-plugin check uses the build-time endpoint.

Create GitHub Releases with version tags such as `v0.4.0`. The update notification links to the release page; it does not silently download or install files.

## Support information

- Product: welleyz
- Publisher: shadowave
- Version: see `release.json`
- Architecture: Windows x64
- User presets: `Documents\welleyz presets` (`.wlyz` JSON)
