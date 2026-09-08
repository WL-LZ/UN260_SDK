# List retained-result view — 2026-09-08

## Ownership

- `lv_core/page_02_list.c`: 1280x400 layout, three panels, action wiring,
  localization, current-result projection and retained-page lifecycle.
- `lv_core/page_02_list_data.[ch]`: page-owned denomination/serial index maps;
  no duplicate business result store. Rebuild on notification; bind visible
  serial rows in O(1), not repeated scans through up to 10,000 slots per drag.
- `lv_components/ui_list_window.[ch]`: device-independent logical offsets,
  ranges, independent scroll/page modes, clamping and paging.
- `lv_components/lv_recycled_list.[ch]`: fixed row pool, pointer movement,
  time-based coast, scrollbar gutter and timer ownership. The owner registers
  device-specific drag capture through the existing input port API.
- Existing counting/protocol/storage services still own the result, packet
  sequencing, currency, clearing, printing and persistence.

## View and input contract

The old top header/background bitmap/transparent page-hit areas are removed.
Panels occupy y=12..387; each has seven complete 36px rows and a fixed footer.
The 4px scroll thumb is centered in the 24px details-to-panel gutter. No image
decoder or snapshot is needed for List decoration. Main/Menu/Currency designs
are not changed by this feature.

A always has DENOM/PCS/AMOUNT, irrespective of CNT/MDC/SDC. A cleared result
keeps controller-confirmed denominations with zero PCS/amount. Before a valid
denomination table exists, show a single 0/0/0 row and zero totals; do not invent
currency tables from simulator defaults. Noncontiguous valid denominations
map correctly to their original data slots.

B and C independently toggle SCROLL/PAGES by their footer button. SCROLL is
continuous; PAGES uses previous/next plus page number, clamps at the ends and
does not wrap. Switching keeps the currently inspected row visible where
possible. No query or controller state mutation occurs when navigating.
Mode choices last for the retained-page lifetime, not across a reboot.

Each viewport owns only eight rows. Logical position uses 32-bit arithmetic;
objects stay within local viewport coordinates, so 10,000 records do not wrap
LVGL's 16-bit coordinates. Idle/hidden viewports have no active motion timer.
Late growing/shrinking results preserve an active drag and rebase its anchor
when clamped. Raw edge/two-finger capture can still cancel the drag.

## Protocol and lifecycle

List is always the current result; HISTORY still opens the separate history
page. PRINT and MAIN reuse their existing callbacks. Print ACK handling and
zero-data eligibility are unchanged. A reject PCS count is not a denomination.
Serial order follows valid sequence slots, not arrival order.

Reset/new-result notifications reset all anchors. Serial end does not reset
all sections. Denomination, serial start/item/end, reject start/item/end and
summary notifications mark the appropriate visible projection dirty; UI
batching coalesces them. Hidden pages record dirtiness only. Reentry refreshes
without losing modes/anchors. Hide/destroy stop motion; destroy cancels queued
UI work before freeing objects and page state. Partial construction is unwound.

No receiving-details UI is introduced. Optional details arriving after count
completion can still update the result. The wire format has no reliable epoch
identifier for rejecting every cross-session late packet; no speculative
packet-drop rule was added and parsers remain unchanged.

## Localization

All visible labels, format strings and reject descriptions go through the
existing `ui_text` system. EN/CN/KR resources are registered. Domain/log/export
`counting_reject_reason_get` strings are unchanged; localization is a view
mapping. Only English is currently offered by language settings, and the
existing Instrument Sans fonts do not supply a full CJK glyph set. Resource
registration alone is not a claim that new CJK fonts have been implemented.

## Regressions

```
python3 tools/test_list_window.py
python3 tools/test_list_data.py
python3 tools/test_list_i18n.py
python3 tools/test_list_invalidation.py
python3 tools/test_list_view.py --lvgl-dir ../../third-party/lvgl-8.3.2 --output-dir /tmp/list-raster
```

Tests use the real model/store and, in the last command, the actual LVGL source,
production List/component/font/button code. Hardware callbacks are isolated.
The simulated display flush is copied to BMP, avoiding artificial full-screen
snapshot allocations in the test heap. Tests include 10,000 rows, >255 pages,
partial last page, empty data, sparse indexes, mode/anchor preservation, numeric
fit, callbacks, reset/hide/destroy, press-time changes and construction failure.

Board acceptance is still required: all three modes with real results, late
details/clear/currency change, independent A/B/C drag, B/C pagination, print,
history/back, side and two-finger gestures, repeated entry, no tearing, and
memory/FPS under normal use. Host raster/cross-build is not a GE or FPS result.
