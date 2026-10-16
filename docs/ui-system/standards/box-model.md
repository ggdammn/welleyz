# Layout ownership

- Parent panels own their insets and spacing between children. Controls own their internal drawing and requested size; they do not shift siblings by hand.
- Preserve existing editor geometry while fixing overlaps. Use shared spacing tokens for newly changed layout values and scale them once via the editor zoom.
- Verify all three pages and the bottom keyboard at supported zoom values. Removing a control must not leave an unexplained hole.
