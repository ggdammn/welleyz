# JUCE integration

- JUCE controls and LookAndFeel are first-class welleyz UI components. Use attachments for automatable sound parameters, and keep audio operations in the processor.
- A custom-painted control retains keyboard focus, an accessible label/tooltip and a visible state. Standard text editing/IME and host keyboard behavior must continue to work.
- When a JUCE control appears in multiple contexts, prefer shared styling and token values rather than duplicating painting constants.
