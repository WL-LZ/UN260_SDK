# Currency focus R4: quiet selection cue

Scope: replace the accepted R3 carousel's blue focus frame. Preserve its
geometry, card positions, content, normal palette, business selection and
drag/coast/spring parameters.

## Visual decision

- Focused face: retain white, full-color flag and dark text, replace the
  saturated blue edge with a 1px cool-silver edge `#BFC7CF`.
- Add a centered 28x3 round graphite capsule `#4D5965` inside the focused
  face. Normal faces have no marker or placeholder. Keep the existing 8px lift.
- Marker bottom alignment uses an 11px inset from the parent's content edge.
  For the current 200x265 face with a 1px border, its raster bounds are
  x86..113, y250..252, leaving 12px to the outer bottom edge.
- The number label starts at local y224 and its actual font line height is
  15px. The marker stays below this text region. It is over 50px above the
  page-level scroll indicator, and substantially darker, so it is visually
  part of the card rather than another progress bar.
- No outer glow, blur, large shadow, checkmark implying controller success,
  image scaling or new animation is added. This is a restrained design
  inspired by a layered mobile UI, not a claim to duplicate an Apple control.

## Ownership and performance

`lv_card_surface_focus_mark_apply()` lives beside the generic card skin.
It allocates no objects, keeps parent/events, and applies decorative geometry
and visibility to a non-clickable, non-scrollable child supplied by the owner.
Currency creates that child once with the existing card tree; deleting the
parent owns its cleanup. It is not a second touch target or selection command.

The marker is part of the same ordinary/focused snapshot. V4 keys prevent
reusing the old blue-frame V3 images. Missing snapshots render the same live
tree. No new cache class or registration, no budget increase, and no changed
working-set policy (7 ordinary + 3 focus faces). Cache-hit motion still draws
the same 200x265 images; there is no separate per-frame marker draw or timer.
Each card adds one small LVGL child for live fallback and snapshot generation.

The render-state guard still prevents repeated style/marker writes for the
same appearance. Production FPS is not inferred from these structural bounds;
first-pass/live-fallback behavior still requires hardware testing.

## Verification

Run `tools/test_currency_carousel.py` (O0 and O2 ownership, actual rendering,
cache misses, idle capture, favorites and unchanged physics), and
`tools/test_card_surface.py` both as interface checks and with
`--lvgl-dir /path/to/lvgl-8.3.2` for actual software pixel checks. The latter
checks normal/focus/normal round trips, corners, inside border, marker bounds,
no outside drawing footprint and retained input/event ownership.

The final delivery includes build/test/hash receipts. The small raster BMPs
are genuine LVGL software renders of the card surface only, not full Currency
screenshots and not board GE/DMA verification.
