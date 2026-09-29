# UN260 U-disk update package

The new update flow uses one file on the U disk:

```text
update/UN260_UPDATE.upk
```

R7 supports two deployment routes; see [FACTORY_FLEET.md](FACTORY_FLEET.md).
Fresh compatible boards use a factory IMG with a pre-enrolled application
volume, with no follow-up USB migration. **Routine updates on enrolled boards
now use a normal full UPK: clicking Upgrade installs the application immediately;
Reboot only starts the new version.** Same-USB boot continuation belongs only
to the separate first-migration package. An old IMG does not acquire
the new storage layout merely because it has an `.img` extension.

## Build a package

For the UN260 `d213_devkitf` target, a normal SDK build now creates the package
automatically:

```sh
make -j8
```

The result is written to:

```text
output/d211_d213_devkitf/images/UN260_UPDATE.upk
```

Internal application/bootstrap/delta archives and the pinned delta baseline are
stored under `images/un260_internal/`, not beside the public UPK. The current
`UN260_FIRST_MIGRATION.upk` in that directory is only for reviewed, not-yet-enrolled
devices (copy and rename to USB `update/UN260_UPDATE.upk` for that one operation).
It must not be distributed for routine upgrades. The redundant
`UN260_UNIFIED_UPDATE.upk` alias is no longer generated. An archived
`UN260_STORAGE_EXPANSION_R7_20260929.upk` is a fixed old-device migration fallback
(including its R7 application), never a normal build output. To use it, copy it
to the USB as `update/UN260_UPDATE.upk`. Keep an independent release backup:
removing the whole output directory also removes archives stored there.

The default package version combines the image version and current Git revision.
Set an explicit release version when required:

```sh
UN260_UPDATE_VERSION=1.2.0 make -j8
```

The full application builder can still run independently for development.
Full `make` validates matching IMG/resources and publishes these same full-package
bytes as the routine `UN260_UPDATE.upk`; it no longer publishes the migration wrapper:

```sh
./tools/un260-update/build_un260_update.sh \
  --version 1.0.0 \
  --output /home/pc/Desktop/UN260_APPLICATION.upk
```

The fixed core payload always contains `test_lvgl`, `liblvgl.so`, the complete
`lvgl_data` resource directory, `ui_update.sh`, `S00lvgl`, and a package version
marker.

Files placed under the following board directory are automatically added by a
normal `make` using their rootfs-relative paths:

```text
target/d211/d213_devkitf/update_extra_root/
```

For example,
`target/d211/d213_devkitf/update_extra_root/etc/un260/example.conf` is installed
as `/etc/un260/example.conf`. Only allowlisted paths are accepted.

Additional rootfs-relative files may be supplied using a controlled staging
directory:

```sh
./tools/un260-update/build_un260_update.sh \
  --version 1.0.1 \
  --output /home/pc/Desktop/UN260_APPLICATION.upk \
  --extra-root /home/pc/Desktop/un260-extra-root
```

Only allowlisted UN260 application, library, resource, and configuration paths
are accepted. Symbolic links and path traversal are rejected.

## Device behavior

The device validates package format, product, schema, path policy, archive entry
types, package identifier, and the SHA-256 checksum of every payload file before
installing. Existing targets are backed up on the U disk. File replacement is
atomic and a failed transaction is rolled back.

Loose legacy `test_lvgl`/library/resource updates are rejected on managed
application-volume devices. Do not distribute internal bootstrap/migration/
application/delta files as a sequence of operator upgrades.

This package provides integrity checking, not cryptographic publisher
authentication. A production signing key and signature verifier can be added in
a later schema revision.
