# Motion

- Motion responds to user action. Provide a static selected/pressed/disabled cue in addition to animation.
- Keep existing RANDOMIZE feedback and wheel/key response; an idle UI must stay calm. Never perform animation work in the audio callback.
- Cancel or supersede stale feedback rather than leaving a state stuck. When reduced-motion information is available, make transitions instant without hiding the state.
