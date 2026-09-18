# 撤回 union 上电 Logo（2026-09-18）

用户明确要求撤回本轮上电 Logo 修改，保留原有开机程序。

- 恢复 `target/d211/d213_devkitf/logo/boot_logo.png`，与当前 HEAD 的原厂资源完全一致。
- 原厂 PNG SHA-256：`92e5659fa46a3c306c173cfb7483cd6a0c78894630fcaf8c88976bcc66fd304c`。
- 本次未提交的 `tools/un260-startup/build_union_logo.py` 与横向输入 PNG 已移出源码目录，保留在仓库外历史交付目录，避免意外重新生成。
- 原有早期亮屏程序、Welcome 动画、自检、Ready、主页面、显示驱动和背光时序均未修改。
- HTML 设计稿保留；Windows 设计稿、密钥、实验目录、构建产物均不纳入固件提交。
- 原有未跟踪 `source/artinchip/test-lvgl/aic_ui/lvgl_data/boot_theme_d/boot-light.bin` 不是本次新增，保留且不提交。

此次 Logo 方案未曾提交，因此撤回后没有固件源码差异；本次 Git 提交仅记录撤回范围，既有业务修改此前已在 `9eb8e1f77` 提交。

恢复资源后执行完整 make，并核对 logo.itb 与两种 NAND 几何完整镜像包含恢复后的原厂资源。构建记录位于仓库外 `/home/pc/un260-deliveries/20260918_union_logo/rollback-make.log`，Windows 恢复交付位于 `deliveries/20260918_union_logo_rollback`。

未刷机；若设备已烧录新 union Logo，需要按实际 NAND 几何选择恢复版完整镜像或沿用既定 Logo 分区更新流程，普通 UI `.upk` 不更新该分区。不要继续使用旧 `20260918_union_logo` 目录中的已撤回镜像。
