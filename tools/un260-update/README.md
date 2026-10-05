# UN260 U-disk update package

The current R8 release has one operator package for supported legacy, enrolled,
factory-provisioned and retrying devices:

```text
update/UN260_UPDATE.upk
```

See [FACTORY_FLEET.md](FACTORY_FLEET.md) for data ownership, recovery and hardware
boundaries. Current updaters reuse an existing healthy application volume and
install the full application directly. Legacy enrollment installs the small
compatible bridge, then completes with the same USB package after restart.
Bridge readiness is pending; success belongs to verified final completion.

```sh
make test-lvgl-rebuild
UN260_UPDATE_VERSION=1.0.0-r8-unified-20261005 make
```

The public result is
`output/d211_d213_devkitf/images/UN260_UPDATE.upk`, with its SHA-256 file.
`images/un260_internal/` contains the full application, nested migration,
bootstrap and delta build artifacts. Operators do not switch between them.
The fixed archived R7 migration package is preserved, not published as current.

The full inner application includes `test_lvgl`, `liblvgl.so`, owned `lvgl_data`,
the updater, startup, storage/display helpers and version metadata. The builder
accepts allowlisted root-relative extras from `update_extra_root`; it rejects
symlinks, traversal and protected local state. Package checksums provide
integrity, not publisher authentication.

V3 migration preserves the unused historical mtd11 bytes, original application
and layout before initializing the supported 38 MiB application partition.
Direct readback, durable barriers, stable source checks and source/UUID-bound
recovery are mandatory. Boot alone never formats NAND. Older journal formats
remain supported. Existing active volumes are verified and reused.

Local records, settings and imported photos under `/etc/ui_state`, plus touch
calibration, are preserved. Full application resources converge transactionally
to the package inventory. Reinstalling the same package remains an explicit user
action and verifies actual files rather than trusting an installed fingerprint.

Do not remove migration markers, transactions or USB backups to force success.
Unreadable hardware, failed writeback, missing media, capacity failures and
unsupported layouts stop with diagnostics. Host tests and a full cross build
are followed by board validation; a UPK cannot repair an inaccessible boot path.
