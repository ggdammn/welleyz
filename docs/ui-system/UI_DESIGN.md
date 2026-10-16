# welleyz — current UI contract

This is an adaptation of the Thamy UI-system concepts for the existing JUCE plugin. The current editor is the visual reference: preserve its actual colors, fonts, layered studio surfaces, configurable accent, shapes and effects. Do not apply Thamy's fixed periwinkle palette, Inter font, piano-roll layout or opaque-only/no-glow rules. Do not use the untracked wavesynth design drafts as a redesign mandate.

## Product and layout

RANDOM is the entry action: SOUND TYPE on the left, RANDOMIZE with BACK/RESET/SAVE/LOAD in the center, and searchable PRESETS on the right. The header has RANDOM/OSC/FX; pitch and mod wheels, keyboard and master volume remain in the performance area. OSC and FX retain their current structure. Keep functionality and placement across 50–200% zoom and HiDPI.

## Tokens and surfaces

`Source/DesignTokens.h` defines base colors/spacing/radii; `Source/ThemeManager.h` derives accent states and owns preferences; `Source/Theme.h` contains shared painting for the current studio surfaces. Use these instead of scattering new colors and sizes across controls. Keep user accent distinct from semantic power/on/off indications. Do not introduce a new accent palette or turn an existing effect off to satisfy an unrelated product's design guideline. State must be distinguishable beyond hue alone; verify readable text and focus with light and dark accents.

## JUCE boundary

Standard JUCE Button/Slider/ComboBox/TextEditor/MidiKeyboardComponent and LookAndFeel are supported. Keep custom painting consistent with the present editor and token values. Audio and MIDI stay in the processor; the editor uses APVTS attachments or documented atomic/message-thread state.

## Interaction and verification

Keyboard navigation must reach mouse actions; Escape closes overlays where applicable, Enter/Space activates controls, arrow keys move lists. The computer-keyboard-to-MIDI mapping must not steal text entry or DAW shortcuts. Give icon-only controls names/tooltips, keep visible focus and at least a 24×24 logical hit area. Animations must remain feedback, not the only indicator; respect reduced-motion where the platform exposes it. Avoid blocking IO in paint/resized and audio. Verify RANDOM, OSC, FX and overlay states with `EditorSmokeTest` and deterministic screenshots, not a new `--ui-gallery` mode.

The adapted contracts in `standards/` provide the inspection checklist. Thamy-specific pattern chips, ghost notes, ruler and playhead rules do not apply to this synth.
