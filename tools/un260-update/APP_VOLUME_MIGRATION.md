# Application volume v1

## Ownership

`rootfs_overlay/usr/local/bin/un260_app_storage` owns mtd11 enrollment, mount and migration state. The updater owns file replacement transactions; the startup script mounts before recovery and launching. No UI page, protocol or counting data ownership is changed.

- Logical package path: `usr/local/bin/test_lvgl`.
- Actual destination after enrollment: `/mnt/un260-app/test_lvgl`.
- Stable root launcher: `/usr/local/bin/test_lvgl` execs storage helper `--launch`.
- Private state: `/etc/un260/app-storage/{pending,active}`; forbidden package paths.
- Only the app is redirected. Libraries/resources remain on root; USB backup paths remain logical package paths.

## Enrollment

Bootstrap first installs `un260_storage_sync`, then the storage-aware updater, startup and helpers. It has no migration request and cannot format. Its empty additive SDK tree prevents the previous installer running unguarded resource cleanup during repair. R7 wraps the bridge, fingerprint-armed migration and full application into one public `UN260_UPDATE.upk` by default; these inner packages are not operator steps. Installing the outer package is the opt-in, followed by same-USB reboot continuation. The exact approved data-only SHA-256 is still mandatory before any legacy NAND initialization. See `FACTORY_FLEET.md` for unsupported-device and recovery boundaries.

Fresh factory IMG enrollment is a separate **offline build** operation: the UBIFS fakeroot copy receives the exact runtime launcher and active owner marker; a required second UBIFS contains the matching ELF and owner. The base target retains the ELF for UPK generation. Boot only attaches/mounts/verifies this prebuilt volume, never formats it and never needs a USB migration. Image packaging requires both components; metadata, geometry, release and payload readback are verified. Factory state rejects customer records and previous update journals. Flashing a full IMG onto an existing board is a reset/rework operation, not a data-preserving alternative to UPK.

The reviewed partition contains legacy filesystem data, not an empty filesystem. Operator consent and an offline preserved OOB backup preceded this implementation. Runtime checks still require exact 64 MiB mtd10 root / 38 MiB mtd11 ubisystem layout, 128 KiB erase/2 KiB write geometry and matching full data fingerprint. Unknown attachments/volume names are rejected.

Ordered state: USB backup -> durable `formatting` intent -> checked USB flush and direct readback of both backups -> create/mount owned volume -> durable `volume-ready` -> copy/hash/rename identical current app -> durable `app-ready` -> preserve original root inode -> launcher switch -> durable active marker -> unlink original -> clear pending. Checked held-fd `syncfs` barriers bracket publication of metadata and file renames. A failed direct read leaves the intent pending but does not initialize NAND; retry requires valid backups.

The Linux kernel implementation in `source/linux-5.10/fs/ubifs/sb.c` creates a default filesystem when mounting an empty newly created UBI volume. There is no dependency on target `mkfs.ubifs`. Only explicitly approved enrollment calls `ubiformat`/`ubimkvol`; boot never does. NAND backup uses a full data-only MTD read, not ambiguous vintage `nanddump -o` options. OOB archival backup remains separately preserved on the host.

If enrollment stops before `app-ready`, the original app remains bootable; resume requires the verified USB backup. An `app-ready` cut is recoverable at boot without formatting. Once active, unknown/missing storage fails closed; no automatic stale binary fallback.

## Transactions and capacity

All updater paths for the application (plan, checksums/mode, delta baseline/target, install, hardlinks, rollback, cleanup) resolve through `destination_for`. Root launcher is never compared to the package ELF. Migration must complete before a normal app transaction; schema-1 enrollment packages may only contain the five bootstrap files (plus the empty additive SDK tree).

`test_lvgl.un260-{old,new,restore}` live on the same UBIFS as the app so hardlinks and rename remain valid. The journal stays on root, persists logical paths, and mounts the app volume before recovery. Checked `syncfs` barriers cover both filesystems. Root library/resource changes and app replacement share the same transaction commit. A local writeback error stops automatic cleanup and retains recovery evidence.

The C guard retains directory descriptors opened before writes, checks path identity/read-only state, and checks Linux 5.10 writeback errors via `syncfs`. USB backups are read with `O_DIRECT` and hashed by the existing target `/usr/bin/sha256sum`; unsupported direct I/O is a hard failure, never a page-cache fallback. Runtime logs/status remain in RAM; final USB log/result copies are flushed before the installed-package marker and terminal success. No USB writes occur after success. The UI masks premature success until the updater child has actually exited. This cannot repair an unreliable USB device or prove behavior under physical power cuts.

Preflight separately measures root staged bytes + 2048 KiB, app staged bytes + 2048 KiB, and USB originals + 4096 KiB. Initial enrollment requires enough empty app-volume capacity for two copies of the current executable plus 4096 KiB. No UBIFS compression discounts are assumed. Reboot releases old executable mappings; no unlimited free-space promise is made.

Managed devices reject pre-v1 full packages and loose legacy updates. Full packages must contain storage-aware startup/updater/helper; deltas cannot remove those core components or replace them with pre-v1 implementations. Manually overwriting the launcher outside the supported updater is detected at boot.

## Validation

Host suites `test_app_storage.py` and `test_app_volume_updater.py` run under dash and BusyBox ash, with filesystem and UBI fixtures. Physical NAND/UBIFS power-cut behavior still requires a recoverable test device. Normal full SDK make must follow startup/script changes; check installed modes and package byte hashes against target output.
