# Machine fault guide (LVGL 8)

The guide is a presentation of controller reports. `Confirm` acknowledges a record; it never sends a command or marks the device repaired. `machine_state/machine_fault.c` owns the active records independently from the popup object's lifetime.

## Integration

- Report accepted `0x0A/02/code` through `fault_popup_report_start_fault()`.
- No notes (`0x0A/01/02`) uses `fault_popup_report_start_no_note()`, which clears the start source and posts an information notice.
- Known nonzero `0x0F` codes use `fault_popup_report_runtime_fault()`. Unknown codes may be recorded quietly with `fault_popup_record_runtime_notice()` and remain inspectable. `fault_popup_clear_runtime()` clears only start/runtime sources.
- Record each accepted `0x37/step/result` with `fault_popup_record_boot_result()`, then show the complete result set with `fault_popup_show_pending_now()`. A successful step clears only that step.
- `fault_popup_report_sensor_mask()` accepts the complete parsed `0x02/01` mask, retaining acknowledged bits and removing bits absent from the new snapshot. New bits are unread; removed bits do not restart another guide.
- Repeated active keys preserve acknowledgement and the current step. A recovered fault that returns is unread again. Manual reopening uses `fault_popup_show_pending_now()`.
- When automatic guides are disabled, new reported faults post a short `machine.fault` top notice. Opening the guide removes this notice; true recovery removes a notice referring to that recovered record. Quiet unknown-runtime recording leaves its notification policy to the app service.

Raw protocol parsing belongs to `protocol/machine_fault_reply.c` and app services. The guide does not interpret `0x14` busy/idle bits as faults and does not synthesize recovery from a timeout, toast dismissal, or counting-end event.

## Adding a fault

Add concise titles, locations and steps in `fault_guide_catalog.c`. Keep the raw code available in `fault_guide_format_code()`. A protocol without a precise location must use a whole-machine highlight; it must not invent a service operation. Each selected step loops independently, and changes only when the user selects another step. Add new localized characters to the scoped message font subset through its generator.

## Assets and motion

`machine_fault_assets.h` describes cropped constant BGRA layers and their positions in a 510 x 273 stage. The generated source is `aic_ui/generated_fault_guide/machine_fault_assets.c`, built from `tools/icon_sources/fault_guide/` by `tools/gen_machine_fault_assets.py`. Keep each lid frame's position with its pixels; the hinge will drift if only the image pointer changes. The lower rear face slides outward, while its tray is clipped at the opening.

`machine_fault_view.c` draws highlights, arrows, notes and a brush with LVGL primitives. It owns one 50 ms timer; frames select constant images or change positions, without per-frame allocation, file access or image decode. Pausing stops the timer. Destruction releases it; hidden roots skip frame updates. Popup lifetime suspends top notifications using the independent `UI_NOTICE_SUSPEND_FAULT` blocker.

## Verification

- `tools/tests/test_machine_fault.c`: source isolation, acknowledgement, repeat/new reports, sensor snapshots, catalog mapping and raw codes.
- `tools/test_fault_view.py`: actual LVGL software rendering, every English/Chinese guide step's bounds, animation controls, multiple records, recovery, external deletion/recreation, ASan/UBSan. Set `LVGL_SOURCE` to an LVGL 8 source directory.
- `tools/test_island_warning.c` and `tools/test_smart_island_count_end.py`: top-notice forwarding preserves counting/result animation, touch host lifecycle and the persistent unresolved-fault entry.

Host raster and sanitizer tests do not validate board display timing or hardware touch. The firmware build and upgrade package must be generated from the same code and assets.
