# 开机和自检主题切换

当前默认：**主题 A**。主题选择在编译时生效，界面不增加设置项。

| 选择 | 开机画面和自检页面 |
| --- | --- |
| A | 客户动画移植前的原版，保留原版欢迎、自检卡片、圆环和退场 |
| B | 已确认的客户定制版，包含 Logo、五种手势、文字变色和大小动画 |

## 修改位置

固件权威仓库中的配置文件：

`/home/pc/d213-100ask/d211/source/artinchip/test-lvgl/un260/lv_core/boot_anim/boot_theme_config.h`

只修改 `#ifndef UI_BOOT_VARIANT` 下的这一行：

```c
#define UI_BOOT_VARIANT UI_BOOT_VARIANT_A
```

改成 `UI_BOOT_VARIANT_B` 即选择客户版。保留上方历史渲染器选择 `UI_BOOT_ANIM_THEME_D`；历史宏 `UI_BOOT_ANIM_THEME_A/B` 是旧实验主题，不是本次产品 A/B。

同一配置同时选择早期开机资源、LVGL 欢迎页和自检页。CMake 检查应用选择与资源选择是否一致；这是构建时检查，不增加客户操作等待、协议确认或弹窗。

## 构建

在已配置好的 Linux 固件仓库执行：

```sh
cd /home/pc/d213-100ask/d211
make -j2 BR2_JLEVEL=2 test-lvgl-rebuild
make -j2
```

完整升级包输出：`output/d211_d213_devkitf/images/UN260_UPDATE.upk`。切换后使用新生成的完整升级包，不混用另一主题的应用或资源。恢复主题 A 时同样修改这一行并重新完整构建。

## 范围与验证

两个版本共用既有真实自检业务流程，各自保留原有页面动画、生命周期和退场。主题 A 不在应用中链接或解压客户版图片。点钞累计、历史、Main/List 布局、全局刷新周期和触摸操作不属于本次修改范围。

原生页面模拟、交叉编译、升级包一致性核对与实板验证分别记录在交付说明中。主机验证不能证明实际面板帧率、开机耗时或实机触摸效果。
