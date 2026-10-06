# Preferences 布局修正

版本：`1.0.0-preferences-layout-20261006-r1`。

- Shortcut order：快捷项名称左对齐，独立浅色序号块，统一 32px 行高、边距、细边框和选中底色；长名称单行省略。继续使用原快捷项交换和保存流程。
- Display & sound / Machine sound：白色背景内增加左右 14px、顶部 7px 的内容布局，文字、On/Off 状态与开关不再贴边；协议确认流程不变。
- Display 标题左侧：紫色电脑图标换为无底座的全面屏轮廓，使用页面私有 LVGL 图元，无新增位图资源，不影响 Device 等其他屏幕图标。

生产变更限于 `un260/lv_core/menu_studio_views.inc`；主机测试因快捷项标题不再包含序号，调整按文本查找的范围，避免把右侧 Top popup 快捷项误当左侧开关。

真实 LVGL Menu 渲染和 ASan/UBSan 交互回归通过，已检查普通/选中快捷项及 Display & sound 截图。完整应用重建、make 和升级包一致性校验通过。仍需板上触摸及实际显示复测。

保留此前用户修改、通知和动画修复，以及松手立即发起退出的手势语义。未提交、推送或刷机。
