# UN260 U-disk update package

The new update flow uses one file on the U disk:

```text
update/UN260_UPDATE.upk
```

The first firmware image containing this updater must be burned normally. After
that bootstrap, later UI releases can be installed from the single package.

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

The default package version combines the image version and current Git revision.
Set an explicit release version when required:

```sh
UN260_UPDATE_VERSION=1.2.0 make -j8
```

The builder can still be run independently after building the SDK:

```sh
./tools/un260-update/build_un260_update.sh \
  --version 1.0.0 \
  --output /home/pc/Desktop/UN260_UPDATE.upk
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
  --output /home/pc/Desktop/UN260_UPDATE.upk \
  --extra-root /home/pc/Desktop/un260-extra-root
```

Only allowlisted UN260 application, library, resource, and configuration paths
are accepted. Symbolic links and path traversal are rejected.

## Device behavior

The device validates package format, product, schema, path policy, archive entry
types, package identifier, and the SHA-256 checksum of every payload file before
installing. Existing targets are backed up on the U disk. File replacement is
atomic and a failed transaction is rolled back.

The legacy `update/test_lvgl` plus optional `update/lvgl_data` and
`update/liblvgl.so` layout remains supported for service compatibility.

This package provides integrity checking, not cryptographic publisher
authentication. A production signing key and signature verifier can be added in
a later schema revision.
