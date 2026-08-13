# Theme Layout

`Resources::installResources()` loads every `qss/*.qss` fragment in the order declared by the resource installer. Qt QSS has no import mechanism, so ordering is part of the theme contract.

- Put generic widget defaults in the lowest-numbered files.
- Put semantic role and state selectors after their defaults.
- Use dynamic properties such as `status`, `state`, and `messageState` for reusable states.
- Add an object-name selector only when Qt cannot express the role semantically and document that exception.
- Keep icons and images under stable `:/Resources/...` virtual paths.
- Do not call `Resources::installResources()` from a reusable library; the consuming application installs the theme once.

The current theme retains some consumer-specific object-name selectors for compatibility. Treat them as migration debt: replace them with neutral roles when the corresponding widgets can expose an equivalent semantic contract.
