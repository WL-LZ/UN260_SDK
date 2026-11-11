# UN260 production U-disk updates

Production devices start from the common factory image containing
`UN260_UPDATER_PLATFORM_V1`. The once-only factory provisioning establishes the
application volume, fixed dispatcher and independent recovery entry. Earlier images and migration
packages belong to the service archive. The production builder emits only the
factory full package. A normal update never formats or repartitions NAND.

```sh
make
```

The normal build refreshes application inputs and publishes one full operator
package at `output/d211_d213_devkitf/images/UN260_UPDATE.upk`, its checksum, and the
matching factory IMG. Copy the UPK to `update/UN260_UPDATE.upk` on USB. Do not rename
an internal legacy or migration package into that file.

The full package carries a complete hash/mode/size target inventory. The engine
compares actual installed files and replaces only changed files. A small source
change does not require copying all unchanged assets. Deterministic archive
metadata and content-based build identity distinguish different dirty builds.

The fixed dispatcher validates a bounded envelope, snapshots the packaged
engine and its four matched helpers, and execs that engine with the same PID,
lock and filesystem descriptors. Updating the installed updater cannot leave a
running old shell responsible for interpreting a new package. Boot descriptors
and runtime/core hash descriptors are bound to the same payload.

During an upgrade reboot the runtime verifies the trial display helper against
its packaged digest. A compatible factory base can show the normal animated
welcome. An older rescue base gets a single verified frame before the
application launch check. The display path does not confirm the update.
Application launch reuses the pending transaction decision from locked
recovery and still validates the actual runtime files before execution.

File installation success requires reboot. The complete previous application,
library, startup runtime, static assets and boot descriptors remain recoverable
until the new application actually presents a frame and remains alive. A durable
confirm/rollback decision makes recovery repeatable after an interrupted cleanup.
The independent factory startup/watch/display path accepts USB repair when the
normal UI cannot start. Normal boot with a working UI does not automatically
install arbitrary inserted packages.

`/etc/ui_state`, touch calibration, records, language and user-imported content
remain outside package ownership. Normal updates do not repartition or format.
Checksums detect damage and mismatched content; they are not publisher signatures.

See [FACTORY_FLEET.md](FACTORY_FLEET.md) for operation, validation and boundaries.
[UNIFIED_UPDATE.md](UNIFIED_UPDATE.md) describes the archived nested migration
route. Use a matching archived software revision for earlier migration and delta tooling.

V1 的普通全量包按应用卷分离布局允许最多 32 MiB payload；旧显式测试包保持 24 MiB。单个 archive 成员仍最多 24 MiB、整个解包仍最多 48 MiB。总量上限不会代替 root、app、USB 的实际空间预检及旧文件回滚保留。

普通 V1 发布的应用 ELF 单独限制为 13.5 MiB（14,155,776 字节）。`factory_layout.py` 统一按 31 MiB 安全应用卷预算，保留当前与替换两个应用版本及 4 MiB 余量；工厂 IMG、UPK 构建与验包共用此检查。超限时应检查应用中的已编译资产，不能只提高数字或依赖压缩后的大小。
