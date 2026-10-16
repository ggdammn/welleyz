# Reusable controls and pages

- Share a control after its second use when the semantics and behavior match. Keep context-specific composition inside RANDOM, OSC or FX sections.
- JUCE hierarchy owns clipping, event dispatch and focus; do not encode sound parameters in drawing code. Shared controls accept context/size through explicit properties rather than divergent copies.
- Do not create a parallel widget framework solely to satisfy the source project's kit architecture.
