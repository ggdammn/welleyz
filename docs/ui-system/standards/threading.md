# Threading

- `processBlock` never allocates, locks, waits or performs IO; parameter reads and UI wheel updates use atomics/prepared data.
- `paint`/`resized` never scan presets, access the network or wait on the processor. Prepare data in user actions or workers, then publish on the message thread.
- The updater runs on a worker and presents its result asynchronously. Any new background task needs cancellation/lifetime safety when the editor closes.
