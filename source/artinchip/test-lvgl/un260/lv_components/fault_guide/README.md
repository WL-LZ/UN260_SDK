# Machine fault guide (LVGL 8)

The guide is a presentation of controller reports. `Confirm` acknowledges a record and calls an application-owned handler. The component itself never sends commands or marks the device repaired. The application routes self-test Confirm to the sensor page, and counting-condition Confirm to `app_fault_recovery`. `machine_state/machine_fault.c` owns the active records independently from the popup object's lifetime.

## Integration

- Report accepted `0x0A/02/code` through `fault_popup_report_start_fault()`.
- No notes (`0x0A/01/02`) uses `fault_popup_report_start_no_note()`, which records a typed yellow attention message with a feeder-loading guide. Clicking the island opens that exact record.
- Known nonzero `0x0F` codes use `fault_popup_report_runtime_fault()`. Unknown codes may be recorded quietly with `fault_popup_record_runtime_notice()` and remain inspectable. `fault_popup_report_runtime_fault(0)` clears only runtime faults. Accepted count start uses `fault_popup_clear_runtime()` to retire start/runtime/batch records.
- Record each accepted `0x37/step/result` with `fault_popup_record_boot_result()`, then show the complete result set with `fault_popup_show_pending_now()`. A successful step clears only that step.
- `fault_popup_report_sensor_mask()` accepts the complete parsed `0x02/01` mask, retaining acknowledged bits and removing bits absent from the new snapshot. New bits are unread; removed bits do not restart another guide.
- Repeated unread keys preserve the current step. A new start/runtime/batch report after Confirm becomes unread and may reopen the guide; unchanged boot/sensor snapshots retain acknowledgement. Manual reopening uses `fault_popup_show_pending_now()`.
- `0x06/04` is a batch-full notification, separate from pending setting results. It uses `fault_popup_report_batch_full()` and the stacker-removal guide.
- When automatic guides are disabled, reports use the typed Smart Island warning. The actual warning identity owns click-to-open. An active fault repeats its warning until its record is retired; read acknowledgement and count-end are not recovery. Unrelated generic warnings cannot replace it.
- Clearing controller latches belongs to `app_service/app_fault_recovery.c`. Confirm, genuine-pocket removal and explicit START recovery feed one serialized `0x3D/01` request. Fault reports, popup settings, elapsed time and animation completion never authorize a clear. Only a positive reply completes it; errors/timeouts retain recovery intent and never fabricate START. Animation completion has no protocol side effects. START/RUNTIME/BATCH recovery sources coexist, so runtime-normal cannot consume a pending batch removal route. A removal that arrives during an earlier request can queue one new attempt if that request fails; failures alone never schedule retries.

Raw protocol parsing belongs to `protocol/machine_fault_reply.c` and app services. The guide does not interpret `0x14` busy/idle bits as faults and does not synthesize recovery from a timeout, toast dismissal, or counting-end event.

## Adding a fault

Add concise titles, locations and steps in `fault_guide_catalog.c`. Keep the raw code available in `fault_guide_format_code()`. A protocol without a precise location must use a whole-machine highlight; it must not invent a service operation. Each selected step loops independently, and changes only when the user selects another step. Add new localized characters to the scoped message font subset through its generator.

## Assets and motion

`machine_fault_assets.h` describes cropped constant BGRA layers and their positions in a 510 x 273 stage. The generated source is `aic_ui/generated_fault_guide/machine_fault_assets.c`, built from `tools/icon_sources/fault_guide/` by `tools/gen_machine_fault_assets.py`. Keep each lid frame's position with its pixels; the hinge will drift if only the image pointer changes. The lower rear face slides outward, while its tray is clipped at the opening.

`machine_fault_view.c` draws highlights, arrows, notes and a brush with LVGL primitives. It owns one 50 ms timer; frames select constant images or change positions, without per-frame allocation, file access or image decode. Pausing stops the timer. Destruction releases it; hidden roots skip frame updates. Popup lifetime suspends top notifications using the independent `UI_NOTICE_SUSPEND_FAULT` blocker.

## Verification

- `tools/tests/test_machine_fault.c`: source isolation, acknowledgement, repeat/new reports, sensor snapshots, catalog mapping and raw codes.
- `tools/test_fault_view.py`: actual LVGL software rendering, every English/Chinese guide step's bounds, animation controls, multiple records, recovery, external deletion/recreation, ASan/UBSan. Set `LVGL_SOURCE` to an LVGL 8 source directory.
- `tools/test_fault_recovery.py`, `tools/test_fault_start_wire.py`, `tools/test_batch_full_wire.py` and `tools/test_fault_recovery_integration.c`: response ownership, failures/timeouts, explicit START sequencing, batch-setting isolation, island click identity and Confirm/removal recovery.

Host raster and sanitizer tests do not validate board display timing or hardware touch. The firmware build and upgrade package must be generated from the same code and assets.
