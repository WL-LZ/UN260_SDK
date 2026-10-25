# 单币种 A 区累计改动与待定回撤边界

记录日期：2026-10-25。权威源码仓库：Linux `/home/pc/d213-100ask/d211`，分支 `main`。

## 用户指令与当前状态

用户要求记住 `fix: 统一累计计数基准并修复详情刷新和切换误拦截` 及其后的累计相关修改，并提交当前正式代码。将来可能改为主控直接发送各面额累计结果，届时再决定替换或回撤屏端算法；目前没有执行回撤的授权，也没有切换协议方案。

码盘问题仅确认和留存证据，暂不修复。本轮检查没有修改码盘相关生产代码，无需撤回码盘代码。仓库外的复现和报告不属于固件提交。早于本轮、已有的无钞故障恢复修复不能误当成本轮码盘修改撤掉。

## Git 边界

- 指定提交之前：`54d86b6cfe45d113a3912131b5c5e6d23563002a`，保留计算器／操作员拆分及现金账本移除。
- 用户指定提交：`bacfc7a0aeef99d4bd350ccb8452fd54b4ac1031`，标题为 `fix: 统一累计计数基准并修复详情刷新和切换误拦截`。
- 本次提交前，`HEAD` 就是指定提交，其后的修改全部在工作区：28 个已跟踪文件修改、9 个新增正式测试。
- 后续金额精度与逐张累计提交、保存标签见下方“本地保存结果”。

## 指定提交包含的行为

1. 单币种实时面额详情；主控重新计数与正常 ADD 续累共用基准，历史按累计组保存。
2. ADD 换组保留当前币种面额目录，仅清零张数、金额和分析基准；保留最终详情校正。
3. 取消触摸回复队列及已结束自检对 ADD 的误拦截。
4. 分离 Batch 本地保存和设备应用，合并提示，避免同步全屏布局。
5. 计数、历史、清空、生命周期和异常恢复回归。

它不是单纯的累计算法提交，还涉及 `app_command_runtime.c`、`app_fault_recovery.c`、`workspace_service.c`、`platform_app.c`、Main/Menu 和历史存储。将来只替换累计来源时，不能因此整体撤掉客户操作恢复、Batch 或布局性能修复。

## 指定提交之后的实现与归属

以下路径相对于 `source/artinchip/test-lvgl/un260/`。

| 职责 | 文件／入口 | 当前行为 |
| --- | --- | --- |
| 逐张实时数据源 | `counting/counting_reject_sn_reply.c`，`counting_sn_push_handle` | 每条有效 `0x49` 的明确面额计一张；相同冠字号／整帧也独立计数；空冠字号保留原始 NO 位置。 |
| ADD 基准和本把计数 | `counting/counting_report_sync.c`，`counting_report_begin`、`observe_note`、`observe_total`、`denom_restart` | 旧基准与本把面额表分开；不再用 `0x0E` 金额差猜面额。首个正总数低于旧基准时重建累计组；首个总数相同的歧义由最终表确认。 |
| 最终面额校正 | `counting/counting_denom_reply.c`，`counting_report_denom_snapshot` | 完整最终 `0x0B` 替换实时面额表，不重复叠加；最终表到达后停止本把逐张累加。 |
| 完成摘要与历史一致 | `counting/counting_info_reply.c`，`counting_info_reply_reconcile_denoms` | 正张数 STOP 使用明确结束值；零张数 STOP 保留最近实时值。最终面额表同步当前总额、完成结果及待保存历史；CNT 空面额表不抹掉计数。 |
| 业务编排和显示 | `app_service/app_counting_runtime.c` | 同步历史换组；`0x49` 通过现有帧合并机制刷新 A；最终校正更新本把完成数字和分析，不重启完成动画或延长停留。 |
| 金额精度 | `counting_data_types.h`、`counting_session_state.h`、`counting_data_store.h`、`counting_ui_runtime.h` 及调用方 | 金额接口使用 `double` 精确承载协议整数范围；面额金额按 `uint64_t(value) * pcs` 计算，避免旧 float 缓存舍入。 |
| 显示／导出／历史 | Main、List、smart_island、`ui_export_data.c`、`ui_history_data.h`、`ui_history_data_fs.c` | 精确金额传递、格式化、显示和存储；历史文件格式不变。 |

正式测试位于 `tools/`：新增 `test_denom_accuracy`、`test_denom_capture`、`test_denom_history`、`test_denom_result_view` 的 C/Python 文件及 `test_denom_amount.py`；同步既有 local_denom、live_a_refresh 和接口夹具。

## 将来切换为主控面额累计协议时

- 先确认新报文是本把累计还是整个 ADD 组累计，以及开始、清空、换组、结束与乱序／重复的约定。收到累计快照应替换对应结果，不能再把累计值当作单张增量相加。
- 优先在协议／计数公共层替换数据源，Main/List 继续消费同一结果投影。不要把算法拆进页面。
- `0x49` 仍可能承担冠字号、NO 和 B 区来源。去掉它对 A 的累加，不代表可以删除整个详情解析路径。
- 保留面额目录、金额精度、历史分组、最终快照校正、ADD 正常切换及动画／刷新机制中仍有价值的修复。
- 若用户后来明确要求全量回到指定提交之前，候选基线是 `54d86b6cf`；若只撤后续实现，候选边界是 `bacfc7a0a`。这只是定位，不代表可以直接 reset 或覆盖届时的新改动。
- 公共结构体、函数签名、调用方和测试必须配套处理；不能只恢复 `counting_report_sync.c`。任何实际回撤先保存届时差异，再按明确范围实施、验证并完整构建匹配升级包。

## 验证证据与已知边界

- 既有应用重建与完整 make 成功；当前 688 个生产输入（`un260/` 和 `CMakeLists.txt`）与构建指纹一致，402 个包文件重新核验通过。此指纹不覆盖整个 SDK/toolchain；此次沿用相同源码的构建验证，不把 Git 保存描述成一次新的构建或板测。
- 精度测试 29 项中 28 项业务断言通过，另 1 项为协议语义诊断；400 把随机场景、7429 次逐张核查；历史真实保存和新进程重读通过。Main/List 原生面额与金额、晚到最终表的完成数字和动画生命周期有独立验证。
- 最新 16:23 日志的 27 把完整 STOP＋最终 `0x0B`，逐张 `0x49`／ADD 基准与最终面额、总数和金额全部一致；存在 17 次张数先到、同张数金额后补。另有 478 张的一把未见 STOP／最终表，不能算完整通过。
- 不将 A 的结果一致扩展为所有历史详情均无问题：日志中超过 250 张的 `0x0D` 符合“最近 250 张”窗口。真实模块复现显示，若完整连续 250 条报告被发布，会替换当前 B，历史摘要可为 400 而 B 为 250、NO 从 1 重排。本次只记录，没有修改此问题。日志自身有半帧打印，不能据此断言设备现场执行了同一发布路径。
- 码盘 RX 已确认来自主控。故障后的 UI 忙状态残留有主机复现，但未修改，且不据此断言机械已经停止。
- 既有夹具失败保留在交付说明中；实机触摸、跑钞与显示时序仍待板验。

## 归档与配套交付

- 提交前 37 个文件与差异：`E:/codex-task-artifacts/UN260/20261025-accumulation-commit/`。`source-before.tar.gz` SHA256：`3a911848577e913328e5429038b696e0da5c9f63e8dc2bf82edb87525c71933a`，归档逐文件回读核验。
- 完整构建／回归：`E:/codex-task-artifacts/UN260/20261025-a-accuracy/`。
- 最新日志核查：`E:/codex-task-artifacts/UN260/20261025-log-1623/`。
- 当前已交付包：`E:/codex/UN260/deliveries/20261025_a_note_accuracy/UN260_UPDATE.upk`，11,389,664 字节，SHA256 `7dcb3a3542b23813b53f4ba20abea3ccb3c2f93fb2f0aaa36555b54db3545045`。
- 本次保留既有交付目录和配套清单；构建包、日志、临时复现及密钥不进入正式源码提交。
- 包内仍保留实际构建时的 `1.0.0-bacfc7a0aeef-dirty-5a36f8b987d9`，本次通过生产输入哈希关联新提交，不改标成新提交重新构建的包。

## 本地保存结果

| 本地提交 | 内容 |
| --- | --- |
| `bea537f1cfaae6dadb52e6f8f2716ee85b404c90` | `fix: 保持点钞金额在界面历史和导出中的整数精度`，24 个文件。 |
| `dfbe92272bd04f9dfe015ec7a651c20bf0bbfeb7` | `fix: 按逐张详情统一实时面额统计与累计校正`，18 个文件；共享文件按功能差异拆分。 |

两笔提交作为最终配套树验证，不宣称中间提交独立通过整包构建。提交检查仅清理 4 个新增测试末尾多余空行，测试逻辑和全部生产源码未变；原始文件与前后哈希保留在 `test-whitespace-cleanup.json`。

本地保存标签：

- `codex/baseline-before-local-a-accumulation-20261025` → `54d86b6cfe45d113a3912131b5c5e6d23563002a`。
- `codex/local-a-accumulation-saved-20261025` → `dfbe92272bd04f9dfe015ec7a651c20bf0bbfeb7`。

文档另外提交；标签锁定的是代码边界。没有改写旧提交、没有推送、没有执行回撤。
