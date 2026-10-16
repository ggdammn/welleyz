# Visual verification

- Run `EditorSmokeTest` and capture a PNG for RANDOM, OSC and FX at baseline zoom, then inspect the results. Exercise zoom, accents, selected/disabled/focus and empty search states in functional checks.
- The standalone editor is the viewing surface; no separate `--ui-gallery` entry point is required for this plugin. Screenshots and backups stay local and outside git.
- Runtime images support visual findings; C++ and tests support behavior findings. Record which states were actually inspected.
