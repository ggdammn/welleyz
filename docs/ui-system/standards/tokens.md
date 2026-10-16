# Token discipline

- Base colors/spacing/radii: `Source/DesignTokens.h`; user accent and derived states: `Source/ThemeManager.h`; shared painted materials: `Source/Theme.h`.
- Introduce named tokens for repeated UI values and documented component roles. Coordinate math, waveform paths and rendering geometry may use local numbers.
- Existing surfaces and fonts are the visual baseline. Avoid palette/radius/font migration disguised as code cleanup.
- Zoom is editor-wide; do not apply a second DPI correction inside an individual widget. Verify 50%, 75%, 100%, 150%, 200%.
