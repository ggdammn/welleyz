# welleyz — engineering standards

Apply the reusable principles of the Thamy Composer standards to this plugin:

1. Build the required structure in its final location; use plain functions and values instead of speculative abstractions. When a second workaround patches the previous fix's side effects, replace the mechanism.
2. C++20, UTF-8 without BOM, `#pragma once`, RAII and explicit ownership. PascalCase types/files, camelCase functions and enum values, `m`-prefixed fields, `k` constants and `s` statics. Stable JUCE entry points (`createPluginFilter`, `processBlock`, `paint`) retain their framework spelling.
3. Comments explain only non-obvious reasons. No prompt, phase or agent metadata in source. Use explicit includes where practical. Use clang-format on touched code.
4. Audio callback: no allocations, mutex waits, blocking calls or IO. Reuse prepared buffers and read atomic parameters; publish data to the UI without blocking audio. UI paint/layout may only consume ready values, never perform file/network IO or wait for the backend.
5. Long-running network/file operations use workers; UI feedback arrives on the message thread. Preset file IO belongs to user actions or asynchronous refresh, not `paint` or `resized`.
6. Preset JSON `.wlyz`: `{ "format": "welleyz", "version": 1, "name": "...", "parameters": { ... } }`. New semantic APVTS IDs are the only parameter keys. Validate the structure before changing live state; a preset is sound-only. Host project UI state is separate; accent and zoom preferences stay in user settings.
7. UI implementation follows `docs/ui-system/`. JUCE `Component`, controls, LookAndFeel, accessibility and host-safe input are allowed. Maintain the current design and ensure each action has a keyboard path and discernible state.
8. Tests cover behavior and persistence, not a reimplementation of DSP or a pixel snapshot assertion. Build VST3/standalone and run all four release tests before completion. Review git diff/status; local rules, temporary converters, backups and user presets never enter git.
