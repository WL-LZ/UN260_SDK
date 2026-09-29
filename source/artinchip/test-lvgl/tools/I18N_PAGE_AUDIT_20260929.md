# 页面文本接入审计（2026-09-29）

本记录覆盖 `un260/lv_core` 页面、Menu `.inc`、Settings 目录/详情公共视图、开机页面与 `un260/innovation`。语言注册表、通用组件、协议/业务消息、导出器和资源字体另有对应模块审计。本轮接入文本源，不新增语言，也不把已有中文/韩文草稿视为可发布语言。

## 分类与显示边界

| 分类 | 本轮规则 | 典型位置 |
| --- | --- | --- |
| 页面标题、按钮、字段、导航、空态 | 显示调用前 `ui_tr()`；已有 `ui_text_get()` 保留 | Main、List、Menu、Settings、History、Innovation |
| 静态目录、选项、步骤、帮助说明 | 定义 `UI_N_()`，取值显示时 `ui_tr()` | settings_catalog、升级/校准步骤、拒钞原因说明 |
| 参数化消息 | 翻译完整模板后格式化；参数保持原始数据 | 版本、计数、通道进度、文件名、金额 |
| 单计数可复数模板 | `ui_trn()`；英语 one/other 放入 pages.en.json | 张数、币种数、待审核数、分钟数等 13 个模板 |
| 通知 | 存源键或 `ui_message_t`，不存当前译文快照 | 页面 post_text/post_message、保存结果、Innovation 64 位金额通知 |
| 阻塞原因和异步状态 | 服务返回源键；UI 显示时翻译 | action_block、采集状态、模式切换、升级错误 |
| 用户输入 | 保持原文，禁止自动查词翻译 | 操作员名、配置名、票据抬头、文件名、键盘草稿 |
| 业务数据 | 保持原值 | 冠字号、金额、NO、面额、ISO 币种码、版本号 |
| 协议/诊断标识 | 保持原值 | FD DF 帧、PS1/PS5L、UV/MG/CIS、MDC/SDC/CNT、硬件通道 |
| 图标/资源/内部标识 | 不翻译 | MICON/ICON、对象名、目录 id/parent/group、通知合并键、路径 |
| 数字与符号 | 保持格式；单位语词接翻译 | 百分比、HEX、RGB 通道、1280 x 400、电压 V |
| 诊断日志 | 串口、性能分类、导出结构 token 不翻译；屏幕中的人类可读错误说明翻译 | uart_debug_printf、perf_profile、Debug 屏幕错误说明 |
| 品牌/特殊字形 | UN260 等品牌保持；Welcome 非英语走普通可翻译 label | boot C/D，英文保留原烘焙字形 |
| 语言显示名 | 从语言注册表 native `name` 读取 | Menu / Settings 摘要 |

静态检查包含 LVGL sink 与私有 `label/small/text/heading/caption/button/control/toggle/segments` 调用栈；同时检查静态数组取值、`snprintf`、通知参数、日志误包装、纯小写单词漏接。工具候选数量不等于实际可见控件数量，不据此宣称覆盖率百分比。测试用 SN001/SN002 和 Innovation 性能日志的误包装已撤销。

## 生命周期与交互

- 页面创建/重建读取当前语言，已有动态刷新函数保留；语言保存由页面管理器的受控刷新入口处理，避免任意语言事件销毁正在编辑的页面。
- 页面层通知改用不可变消息键或深拷贝描述符。异步完成后仍能按当前语言渲染；用户字符串只作为参数。
- Debug HEX 键盘 Delete / Clear 按逻辑键索引触发，不依赖显示文字。普通键盘保留字母、HEX 和内部 SHIFT/BACK/CLEAR action token。
- Standby 按 action ID 选择箭头；日期拼接有界且只复制完整 UTF-8 字符。头像首字符、开机逐字动画也保持 UTF-8 完整性。
- 本轮不改协议发送、ACK、超时、保存草稿或导航语义。格式模板接口不承担业务动作。

## 测试边界

`test_i18n_support.py` 给页面 host fixtures 链接真实语言注册表、官方生成目录、持久化模块与消息描述符；硬件动作仍通过已有测试边界模拟。`UI_STATE_DIR` 指向独立 `/tmp/un260-i18n-tests`，不会改设备配置。可用 `LVGL_SOURCE` 指定完整 LVGL 8.3.2 源目录。

Menu、Settings、Standby、普通参数、诊断、升级和 History 七套实际 LVGL 页面测试已通过；另通过 Innovation transition 生命周期测试。新增检查包括：翻译后的 Debug 按钮标签不改变 Delete/Clear 行为；过长多字节日期不越界、不产生半个 UTF-8 字符；其余沿用正常、超时、失败重试、离开恢复与反复创建/销毁场景。测试使用隔离 `/tmp/un260-i18n-pages` 副本，没有写入 Linux 权威源树。

主机测试和交叉编译不等于实机验证。新增语言开放前仍须补齐译文、匹配字体字形、检验长文本/断行和屏幕布局；阿拉伯文、乌尔都文及印地语还必须通过方向/塑形/输入能力验证。早期 native boot 的 ASCII/烘焙资源有独立 renderer 能力门槛，不能仅凭页面接入语言系统宣称全启动链路支持。
