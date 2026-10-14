# 升级兼容性测试夹具

`legacy-updater-before-boot-bundle.sh` 是 2026-10-09 引入启动资源包之前的生产升级器原件，仅用于主机隔离测试，不安装到设备。来源为此前已核验归档中的 `work/boot_bundle_20261009/update_backup/updater-before.tar`。

SHA256：`4fb0c53d1cc92d1027482d539d17792665a02980b905b07435fdafe4b348ef22`。保留完整原件，验证旧升级器会在复制前拒绝新存储保护标识；不再依赖未跟踪的工作目录备份。
