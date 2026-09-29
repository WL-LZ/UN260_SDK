# 顶部消息调用链迁移记录

本文件记录页面、系统工具与协议结果之间的通知所有权。机器状态故障引导由 `lv_fault_popup` 独立负责；无需用户决策的通知使用 `ui_notice`。调用者传入明确类型，不通过文本关键词判断成功或失败。

## 类型与业务语义

| 类型 | 显示语义 | 典型触发 |
| --- | --- | --- |
| SUCCESS | 绿色完成结果 | 控制器匹配 ACK、本地保存完成、导出文件提交完成 |
| ERROR | 红色明确失败 | 请求发送失败、明确拒绝、存储/导出失败 |
| WARNING | 橙色需注意 | 缺少前置条件、请求超时且结果未知、数据不支持当前输出 |
| PROGRESS | 蓝色正在处理 | 控制器请求已发送、异步本地保存/导出进行中 |
| INFO | 灰色信息 | 只确认打印请求已发送、录制开始/停止请求、主动取消、操作冷却 |

同一操作的进度和结果复用同一个 key。点击通知只触发收起，不确认硬件状态、不取消请求、不释放 busy 或模式占用、不发送 UART 指令。PROGRESS 由业务完成事件替换；不能用通知自动隐藏推断业务结束。协议超时仅表示未收到结果，不宣称已失败或机器已停止。

应用整套设置时，只有当前等待命令的子项结果归 profile 所有：明确拒绝立即终止后续步骤并显示一次 ERROR；超时显示 WARNING（未确认）。其他设置事务仍显示自己的结果，不被 profile 的进行中状态吞掉。

## 调用者、文本与触发

| 入口/所有者 | key / 主要文案 | 触发和协议关联 |
| --- | --- | --- |
| Main/Menu Print (`lv_page_event.c`) | `print.request`；Print request sent / not sent；Count first、MULTI 不支持 | 保留原 0x3C 九字节负载。协议未明确可匹配本次事务的完成结果，只发 INFO；无数据/MULTI 为 WARNING |
| Main/Menu QR | `export.qr`；保留各数据校验的本地化原因 | 无串口命令；阻止不完整、运行中和不支持的结果 |
| Menu (`page_03_menu.c` 与 menu includes) | `menu.operation` 为即时提示；`workspace.store/workspace.batch/records.store/workspace.apply` 为任务 | 页面登记已接受的工作空间保存、照片、批量周期、记录及 profile 任务；全局 app_ui_runtime 观察服务终态，页面隐藏后仍收到结果，完成类型取服务明确结果 |
| Receipt (`page_20_set_print.c`) | `settings.receipt`；Saving receipt settings、Receipt settings saved、Save incomplete | 0x41，多字段逐条保存由页面拥有。`is_saving()` 让协议层抑制子字段重复结果。超时 WARNING，保留已确认字段和未完成草稿 |
| Double note / Reject pocket / Serial / Flap | `settings.double_note/reject_pocket/serial_number/flap` | 页面发发送失败及 PROGRESS；匹配结果/超时由 app_setting_notice 统一发布，页面仅刷新控件 |
| Currency (`page_07_curr.c`) | `settings.currency`；Changing currency、Currency changed / rejected / unconfirmed / not sent | 0x03；仅 ACK 更新币种。发送失败取消请求不伪造拒绝 ACK，超时保留最后确认币种 |
| CFD (`page_27_set_cfd_level.c`) | `settings.cfd`；Detection levels saved；No reply | 页面发写操作结果，普通读查询不产生保存成功。关闭页面不取消已经发送的写事务 |
| Aging (`page_26_set_aging.c`) | `settings.aging`；Aging test running / complete / rejected / unconfirmed | 现有 request / ACK / timeout 生命周期；点击消息不释放诊断占用 |
| Factory (`page_30_set_factory.c`) | `settings.factory`；Factory reset、Reset not sent / rejected / unconfirmed、Restarting | 发送进度/失败顶部显示。超时独立 `on_timeout()`，不触发重启。成功后的 Restart 按钮仍是真实操作对话框 |
| Password (`page_29_set_password.c`) | `settings.password`；字段校验、PIN 验证、保存结果 | 仅通知迁移；密码键盘、草稿、确认按钮语义保留 |
| Brightness (`page_33_set_brightness.c`) | `settings.brightness`；保存/恢复/应用失败 | 保留 Keep/自动回退业务计时。点击通知不确认亮度、不停止回退 |
| Date & time / Language | `settings.time/language`；Date & time updated / Language changed | 现有本地设置应用后 SUCCESS；不虚构 RTC 写入或控制器 ACK |
| Standby (`page_34_standby.c`) | `settings.standby`；Saving、Saved、Save failed；颜色/时长校验 | 保存终态依据 `standby_store_last_success()`；在页内 poll 或全局 poll 的单一消费路径显示。移除旧底部 note 和 2.5 秒私有生命周期 |
| History (`page_19_history.c`) | `history.storage/metadata/feedback`；Saving records、History saved、Storage/save failed、容量上限/未知日期 | 存储状态变化时发，防止 render 重复；预热不弹消息。详情/筛选/删除确认保留。导出终态只由 export 服务发，页面不再重复错误 |
| 当前导出 (`ui_export_data.c`) | `export.current`；Exporting records、Export complete/failed、No data、USB unavailable | CSV/文本文件成对提交后才 SUCCESS，失败回滚。冷却 INFO，不能显示假 Exporting |
| 历史导出 (`ui_history_export_data.c`) | `export.history`；History export + 本地化细节 | 选择稳定 record ID 后快照导出；缺 USB/无记录 WARNING；实际事务成功/失败归服务所有 |
| Screenshot / Recording | 各工具自己的 key；本地化保存/USB/录制状态 | 截图实际落盘后 SUCCESS；录制 Start/Stop 是 INFO，最终文件完成才 SUCCESS/ERROR。收起消息不影响录制 |
| Image/Wave capture (`page_28/31`) | `capture.image/wave`；开始、完整接收、错误、结果超时 | 指令、内存/尺寸、流完成条件不变。每包行数留在页面，避免每包更新通知。owner 在 destroy 清理被页面取消的接收任务消息 |
| Motor/Calibration/Debug | 各诊断操作 key；不可发送/阻挡/导出原因 | 主状态面板和实际传感器数据继续在页面展示；被动说明转顶部，Prepare Manual 的重试动作保留 |
| Controller/Image upgrade | `upgrade.controller`；开始、失败、结果、未确认 | 页内升级状态/真实进度保留，无虚构百分比；离开未确认任务仍需用户决策 |
| UI upgrade (`page_16_ui_upgrade.c`) | `upgrade.ui`；UI update not started / package unavailable | 开始失败顶部显示；最终结果走统一 lv_upgrade_popup 适配。真正重启/离开操作保留 |
| Multi pass (`innovation/page_32_innovation.c`) | 通知 key；准备、ADD 要求、分配失败、已记录 | View report、冲突选择保留；无需选择的已有信息框转顶部 |

准确的完整本地化文本仍归 `ui_text_page.c` / `ui_text_widget.c`，业务错误说明归相应服务；本次不复制第二份静态文本表。扩展通知应使用组件 API 并提供稳定 key、明确 kind 和简短 title/detail。

## 保留的对话框与页面内容

保留删除、丢弃未保存内容、恢复出厂设置、重启、应用资料、营业记录确认/归档、开始老化、未确认运行任务的离开提示；这些按钮会改变业务。Software versions、报告、帮助等主动打开的详情也保留。没有按 `SETTINGS_DIALOG_INFO` 枚举批量替换，以免误删有回调的业务决策。

实时传感器、校准状态、马达运行状态、接收进度、升级百分比、未保存草稿标识属于页面内容，不反复生成顶部消息。Menu/History/Standby 的旧通知容器、定时隐藏状态、原 toast 配置重复代码已经移除。

## 生命周期和去重

- 共享卡片复制文本，调用者局部字符串生命周期不外泄；不存在每帧字符串格式化/文件读写。
- Menu 隐藏时取消未完成 profile 后续步骤、收起即时 Menu notice，保留既有 Leave 确认流程。存储任务已移交全局 owner，页面隐藏不会丢失成功/失败。Receipt、CFD、Aging、Factory 等协议任务不因顶部消息关闭而取消。
- Receipt 多字段事务、History 导出、图像/波形接收错误必须由一个 owner 发终态；协议公共提示不得再次重复发。
- UI notice 与机器故障/真实决策对话框的遮挡由共享层及全局运行时管理，避免各页面互相解除同一个遮挡标志。
- 新增 `UI_TEXT_NOTICE_NOW` 和 `UI_TEXT_NOTICE_ONGOING`，含英/中/韩。Instrument Sans 本身无 CJK 字形，组件需要通知专用 CJK 子集回退；不能只声明本地化文本存在便视为可显示。

## 验证记录

Windows GCC 主机实际通过：`test_menu_print_feedback.py`、`test_history_export.py`、`test_currency_modes.py`（O0/O2）、`test_batch_reply.py`、`test_settings_communication.py`（O0/O2）、`test_settings_calibration.py`（O0/O2）、`test_multi_result_safety.py`（O0/O2）。最后一个测试保留既有 counting_cashbook 数组地址检查 warning；Windows 测试命令只将此 warning 从 error 降级，未改业务代码。

LVGL 页面测试夹具已迁移为 `test_notice_sink.h`，分别断言类型、文字、同任务 key 与业务状态，组件动画由独立组件测试覆盖。Linux 实际 LVGL 页面套件已通过 Menu、History（ASan）、Standby、Settings、普通参数、诊断和升级页面；Menu 全局任务 owner 后续修改由组件集成任务另行复跑。诊断套件将旧 capture footer 断言改成通知 type/key/detail 断言，并保留发送、流完整性及销毁行为断言。History 原页面的中韩字体仍存在既有缺字 warning，不能将通知字体修复等同于全应用多语言字体修复。交叉编译、完整 make 和包匹配由集成任务统一执行。主机通过不等于板上显示/触摸/性能已经验证。

板上重点：1280×400 卡片不偏移；长英文/中文/韩文无缺字且截断正确；按住/点击收起不透传；进度点击后业务仍持续，真实终态会重新出现；故障/键盘/确认框遮挡恢复；SD/USB 失败、超时、迟到 ACK、切页/销毁后的结果不重复。
