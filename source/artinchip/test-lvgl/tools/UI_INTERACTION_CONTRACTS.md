# UI interaction ownership

This project keeps reusable interactions in their owning modules. Do not copy
an existing page's widget tree or bypass input/navigation policy in a new page.

## Four-digit PIN editing

- `un260/lv_components/lv_pin_input.*`: bounded ASCII PIN state; no LVGL, storage,
  or authentication dependency. Leading zeroes are significant.
- `un260/lv_components/lv_pin_keypad.*`: shared 1120 x 320 keypad, four dots,
  blinking current-position underline, shared damped buttons, and explicit
  instance lifecycle. `show` receives display text and confirm/cancel callbacks.
- `page_05_set_password.c`: authentication and routing only.
- `page_29_set_password.c`: Current/New/Confirm field drafts, validation, and
  `user_cfg` persistence only. Confirm applies a field; Save changes the stored
  password. Cancelling a field discards only that editor's draft.

Hide/reset/delete through the component API. Its timer must stop while hidden,
and plaintext plus callbacks must not outlive the page. Never log PIN contents.
This UI change does not reset an existing saved password.

The eye button is owned by the same component. It reveals entered digits only;
empty slots remain gray dots, and hiding/clearing the editor also clears the
numeric label strings. `digits_visible` and `save_visibility` in the keypad
configuration keep filesystem access out of the widget layer. Both password
pages connect these to `user_cfg_password_visibility_enabled/save`.

The preference is stored separately as `0` or `1` in
`/etc/ui_state/password_visibility.cfg`, using the existing temporary-file,
file-sync and rename helper. It must not be written to `password.cfg`, included
in a firmware payload, or contain the PIN itself. Missing/malformed preference
defaults to masked. A save failure leaves visibility unchanged; same-value
valid saves skip writes. `user_cfg` owns storage and reads lazily, never per
frame. This remembers the user's choice across restarts, including a choice
to reveal future PIN entries.

## Cached page navigation sessions

Widget caching is not a request to remember navigation forever. The page
registry's optional `reset_navigation` callback runs when Main/Home commits.
It clears page-local navigation state without destroying cached resources or
changing device settings. Register new session-scoped pages here instead of
adding per-button Home/ESC cleanup.

Settings opts in: reset its saved model **and** retained sidebar, content page,
selection and internal back history. Back from a detail page to Settings does
not end the session, so it preserves the current category. Re-entry after Home
starts at SYSTEM with no old selected option. Cache eviction still uses the
normal destroy path independently.

## Pointer ownership

`lv_port_indev_set_drag_obj(obj, true)` registers both the driver's drag marker
and LVGL PRESS_LOCK. Custom handles use this API to retain contact beyond their
initial hit box. Ordinary buttons must not be registered just to avoid a lost
press: their slide-out cancellation prevents accidental activation.

Only RELEASED represents completion. PRESS_LOST represents cancellation,
including capture by global edge/multi-finger gestures, and must never commit
the local action. Cancellation may arrive without an active LVGL input device.

Innovation uses one opaque transition surface during drag and keeps the live
tree parked. Do not restore independent child animations; if surface creation
fails, its existing stationary atomic entry fallback remains in effect.

## Regression checks

Run on the Linux build host from the application root:

```sh
python3 tools/test_pin_keypad.py
python3 tools/test_password_visibility_config.py
python3 tools/test_navigation_history.py
python3 tools/test_settings_session.py
python3 tools/test_pulldown_capture.py
python3 tools/test_nav_button.py
python3 tools/test_edge_hint.py
sh tools/test_services.sh
```

Host state/lifetime tests do not replace board verification. Check all three
password fields (confirm/cancel/back/save), fresh Settings entry after Home,
detail Back context, slow held pull beyond 44 and 90 px, release/reversal, fast
pull, ordinary scrolling, and edge/multi-finger capture. Enable existing PERF
diagnostics only for investigation: `PULLDOWN event=release` records distance,
elapsed time and open/cancel decision; `event=cancel reason=press_lost` records
ownership cancellation. These do not print the user's PIN or run every frame.
