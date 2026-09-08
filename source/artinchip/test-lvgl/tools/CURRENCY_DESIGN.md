# Currency: motion, rendering and ownership

This change integrates the accepted carousel interaction, rounded card faces,
neutral progress track and R4 silver/graphite focus treatment. The structural
cleanup does not change the approved motion parameters or controller protocol.

## Placement rules

| Layer | Responsibility | Must not own |
|---|---|---|
| `lv_components/ui_scroll_physics` | Allocation-free one-dimensional drag, velocity, deceleration, spring and bounds math | LVGL objects, page identity, IO, currency selection |
| `lv_components/lv_card_surface` | Reusable inset card skin and decorative focus cue | Currency catalog, scrolling, cache lifetime, controller commands |
| `lv_core/page_07_curr/page_07_curr_carousel` | Single LVGL input/timer owner; direct-drag projection callback; click suppression; diagnostic action boundaries | Card contents, favorites persistence, selecting currency on focus |
| `lv_core/page_07_curr/page_07_curr_card_render` | Page-owned ordinary/focused surfaces, versioned cache identity, nearby references, live fallback | Timers, navigation, protocol sends, global cache budget |
| `lv_core/page_07_curr/page_07_curr_view` and private layout header | Shared Currency image treatment and declarative page geometry | Cross-page policy or a second animation owner |
| `lv_core/page_07_curr.c` | Page lifecycle, object composition, callbacks, visible projection and idle scheduling | Duplicate motion algorithms or inline cache implementation |
| `page_07_curr_model` | Existing catalog/favorites/view state | Treating visual focus as controller confirmation |

New page-private helpers stay inside `page_07_curr/`; they are not added to the
public `page_07_curr.h`. Generic components remain reusable without importing
Currency. The existing page context is the owner; splitting files must not add
another global instance or extra background timer.

## Accepted behavior

- Screen 1280x400; left summary width 288; browser width 992.
- Cards 200x265, stride 228, gap 28, focus at display x632.
- Fixed geometry, four rounded corners, radius 14 and 1px internal border.
- Ordinary face #F7F8FA / border #DEDFE1; focused face white / #BFC7CF edge.
- Focus raises the card by at most 8px and shows an inset 28x3 #4D5965 capsule.
- No dynamic scaling, rotation, blur, outer glow or animated group opacity.
- The 176x4 neutral track follows clamped position; it is not another input owner.
- Browsing focus does not send a currency command or rewrite controller selection.
  Tap, favorites, AUTO/manual and success/failure replies retain existing semantics.

Drag uses the pointer position directly. Velocity uses recent 100ms samples,
discarding samples before a direction reversal; a 90ms hold cancels stale fling
velocity. Maximum release velocity is 3600px/s. Coast uses v(t)=v0*exp(-6.5t),
predicting x+v/6.5 and snapping to a legal stride. The spring is omega=20/s and
zeta=0.86, with at most 8ms integration steps and an exact idle landing. Both
edges use the same bounded rubber-band formula, capped asymptotically at 56px.

The model is informed by public UIScrollView target/velocity semantics and
Embla's separation of tracking, motion and bounds; it does not transplant their
runtimes or claim to reproduce Apple's private implementation.

## Cache and input invariants

- Normal and focus surfaces share geometry but have distinct `CURR_CAROUSEL_V4`
  identities including currency code and displayed sequence.
- Only the nearby working set is pinned: at most seven normal and three focus
  surfaces. The global budget is not increased by this commit.
- Unbind an image before releasing its reference to the LRU. Missing surfaces
  render the same live object tree, rather than displaying stale imagery.
- Snapshot production is an idle-only operation, not part of motion projection.
- The existing page scheduler owns prewarm timing; a hidden or moving page does
  not synchronously generate card snapshots.
- Suspend, rebuild and destroy terminate motion ownership. A caught fling,
  vertical cancellation or drag across a favorite must not become a currency tap.
- A snapshot allocation/decode failure still falls back to the live tree. This
  refactor does not change the existing first-missing-item retry order.

## Diagnostics and dependency boundary

Action-boundary `[PERF_CAROUSEL]` logs use the existing opt-in UART route.
`flush_frames` now explicitly means the difference in the existing
`perf_profile_frame_sequence()` counter (reported flush samples), not a promise
that each sample reached scanout. It replaces the earlier ambiguous `frames`
field tied to `fbdev_present_sequence()` from an uncommitted display experiment.
`elapsed_ms`/`steps` describe released motion, while `project_calls` and
`project_us=total/max` also include direct dragging. High-resolution project
timing is skipped when performance diagnostics are disabled.

No new dependency on `lv_port_disp`, `present_damage`, `render_scratch`,
`ui_frame_commit`, storage workers or Innovation changes belongs in this commit.

## Verification

```sh
python3 tools/test_scroll_physics.py
python3 tools/test_currency_carousel.py
python3 tools/test_card_surface.py
python3 tools/test_card_surface.py --lvgl-dir /path/to/lvgl-8.3.2
```

The integration host test compiles production functions at O0 and O2 with a
deliberately small LVGL event/cache model. It does not replace a real cross-link.
For acceptance, export HEAD plus the exact staged Currency tree and cross-build
it with the SDK toolchain. This catches missing headers/symbols that test shims
can mask. Keep unrelated staged/working modifications outside that snapshot.

Raster tests check the actual LVGL software card edges, alpha and marker. They
are not measurements of board GE/DMA FPS or touch latency. Structural cleanup
alone is not claimed as a frame-rate improvement.
