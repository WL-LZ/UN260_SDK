# Currency visual R3: card geometry and bounded rendering

## Scope

Preserve the R2 drag, velocity, deceleration, spring, focus-lift and boundary
model. Currency protocol confirmation, AUTO, favorites and navigation retain
their existing meaning: visual focus is not a controller currency commit.

## Root cause and correction

The old 220x292 unselected PNG was zoomed to approximately 200px wide around
LVGL's default center pivot, then clipped by a 200x265 root. Its transformed
origin was inset and its bottom/right rounded edge exceeded the root. The
separate zero-radius orange-red input outline described the root, not the
visible transformed face. The V2 cache also only contained the unselected
appearance, so focusing a card could never recover full-color flags/black text.

`lv_components/lv_card_surface` now owns the opaque card background and inside
border using exact object geometry. Currency owns its palette and content:

- Both states: 200x265, radius14, border1, no shadow/outline/animated scaling.
- Normal: background F7F8FA, border DEDFE1 (RGB x0.9 rounded), muted content.
- Focus: white, border1677FF, original flag colors and text16181B.
- Input shell: same rectangle and radius; no independently drawn border.
- Global normal y68, focused y60. The strip is anchored at y60 and contains
  only274px height. The old312px strip included unused space above the cards.
- Position indicator: centered176x4 neutral rail and rounded gray thumb. It
  follows actual position and clamps at both boundaries, not the target index.

## Cache policy

The original full-color flag images stay unchanged. The rounded background is
code-rendered, and complete static faces use the existing DMA snapshot cache.
V3 keys distinguish normal/focus and cannot reuse malformed V2 faces.

At most7 nearby normal and3 nearby focus faces are referenced (about2MiB).
Other faces are detached before releasing their references and can be evicted
by the existing global8MiB LRU. No budget increase and no whole-catalog double
allocation. Active motion only acquires existing entries; a miss shows the
correct live object tree. Idle prewarming generates the focused face first,
then nearby faces. A failed capture keeps live rendering rather than No data.
An appearance change never captures synchronously in the motion callback.

During lift, only the visible visual node and input shell move. The hidden
fallback subtree is repositioned when needed for capture/live use, not on each
animation tick. Stable focus and cache state do not rewrite styles or images.

## Performance evidence and limits

The previous12:36 log's middle23 windows were33-40FPS (mean36.70). Its like-filter
mean34.64 was lower than the two immediate earlier means44.13/40.07, although
catalog and gestures differed. Mean active-handler cost increased from about19
to25ms and output from9.4 to13.1ms; increased VSYNC wait is a symptom, not proof
that the driver became slower. Crossing a refresh deadline remains a hypothesis.

The strip footprint shrinks312->274px (about12.2%); this is not a promised FPS
gain. We do not remove VSYNC, change framebuffer ownership, kernel configuration,
CPU frequency or the motion physics to hide the signal.

The carousel action diagnostics now use the existing UART logging interface,
not stdout. With the original performance switch on, RELEASE/SETTLED/CANCEL
include project_calls and project_us=total/max. Projection counters include
dragging; elapsed_ms/steps/frames retain their post-release meaning. Diagnostics
are silent and skip high-resolution timing when profiling is disabled.

## Verification boundary

Host ownership, live/cache state and LVGL software pixel tests are separate
from hardware GE/DMA quality and FPS. Check the final delivery README for the
actual run receipts and remaining board tests. Do not infer hardware success
from a host raster image or build success.
