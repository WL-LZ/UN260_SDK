# UN260 公共性能管线 — 2026-09-08 第一轮

> 归档说明：本文保留 2026-09-08 的设计、基线和验收边界，不代表所有场景已经通过板上验证。
> 公共管线、Main 动画意图和 Innovation 生命周期修复按功能分别提交；主机回归及交叉编译记录与板上验收应分开看。
> Currency 后续定稿已独立提交为 `f8c036522`，其当前模块边界见 `CURRENCY_DESIGN.md`，不要把本文早期说明视作待提交的 Currency 改动。

本轮以前的工作区已提交：5917d7829（统一 PIN/显示偏好）、0386f8adc（Settings 导航会话）、1d96af9f6（慢速下拉触摸所有权）。不推送远端。

## 目标与边界

- 客户日常使用、所有 debug/录屏关闭是最终验收目标。带 PERF 的同条件日志只用于定位和 A/B，不把减少日志开销算作优化成果。
- 保持 GE NORMAL、应用 O2 / LVGL O1；不启用 CMDQ、CPU 超频、局部旋转，不改变触摸坐标或图片格式。
- 新公共机制有明确归属。页面只接入其可见数据投影，协议消息/警告/开始结束事件不得按“最后一个”丢弃。
- 编译、模拟故障和主机测试不能代替板上验证；此时不承诺实际 FPS 提升百分比。

## 五项落点

| 项目 | 公共归属 | 本轮实际范围 |
|---|---|---|
| 存储隔离/增量历史 | storage/storage_worker + lv_system/ui_history_data_fs | 不可变有界任务、状态与有序重试；v2 原子权威 index 保留，只重写变化的 slot/meta 镜像。不是改成不可靠追加日志，也不是取消 fsync。 |
| 调度/接收预算 | app_runtime_wakeup + protocol_frame_queue/RX + app_command_runtime | 256 字节批量读、逐帧保序、2ms/64帧预算、条件唤醒、最大10ms等待；有界队列背压，不承诺无限串口吞吐。 |
| 显示提交/缓冲复用 | lv_port_disp + aic_ui/present_damage | 成功 PAN/VSYNC 后才转移缓冲所有权；下一帧只补齐“上一帧变化且本帧不重画”的区域；碎片过多退回完整旧脏区复制，GE复制失败退回全屏重画。 |
| 可见数据统一提交 | lv_core/ui_frame_commit | 数据批次中按 callback/context 合并 dirty mask；Main设置/计数/冠字号行及注册页数据通知接入，隐藏页保留dirty。页面创建/导航/按钮反馈仍同步。 |
| 合成工作区复用 | aic_ui/render_scratch + lv_dma_snapshot_cache | 复用有界 CPU 合成暂存区，避免重复大块 malloc/free；保持完成且解码成功才发布到 DMA 的事务性，不复用视觉已过期的页面截图。 |

显示 `PERF out/mirror` 仍计入移到 render_start 的复制时间，不能通过移动计时位置制造收益。`PERF_PRESENT` 给出实际复制/跳过像素和回退数。

## Innovation 退出

基线多次明确记录 `INNOVATION_BACK event=no_first_draw fallback=atomic`，所以原退出并不是完整上移动画。

进入松手完成与退出统一为 180ms ease-out；取消下拉维持150ms。退出把整张不透明快照从 y=0 移到 -400。拖动进入的跟手阶段由手指速度决定，不是固定180ms。

首帧门闩改为不依赖 PERF 开关的成功显示提交序号，序号未变化时不提前开始动画。动画/异步分配失败保留原子的整页切换回退。BACK/ESC/侧边返回统一入口；进入未提交时返回取消预览。

不能因为业务数值没变就省略退出快照：按钮样式动画、Tab、滚动也会改变像素；当前没有完整子树视觉revision，所以退出仍强制捕获，避免旧内容/残影。

## 验收顺序

1. 冷启动、自检、图片、点击/按压反馈；关闭 debug 做一次日常操作。
2. Currency 左右来回、边缘停留；List/Main详情与冠字号上下滚动。
3. Innovation 慢下拉/快速下拉、取消、首次与重复进入，分别用 ESC/侧边退出，连续重复；检查完整背景、顶部残留和误触。
4. 连续点钞，开始/结束/错误/SN不漏；查看多笔历史，修改选择/删除，保存完成后重启确认。无主机时此项不能算通过。
5. 同上轮开同样 PERF 开关，不录屏，做相同行为；查看 PERF_COMMIT、PERF_PRESENT、合成缓存统计以及 MemAvailable/CmaFree。

普通升级仅使用 make 产出的 `UN260_UPDATE.upk`，无需手动再次打包，不改设备密码或其它 /etc/ui_state 配置。

### 基线

来源：用户 `99854ec4-ac9e-4d90-bb21-02904107ae86/pasted-text.txt`；SHA256 `b057dad4e5a1133252eaa5e96c12229518757cb28719975d15b138ee2048771b`。

工具 `perf_baseline.py` 按页面与活动分类，RENDER且n>=10只作为持续渲染代理，可能包含动画，并不自动认定是滑动。Currency该组19窗口632帧，窗口FPS均值32.53、范围15–45；其handler按帧加权23.71ms、显示提交13.02ms（PAN2.82、VSYNC8.53、mirror1.66ms）。不能将这些阶段上限相加当作实际FPS提升。

后测必须采用相同划分，区分冷进入/稳态/空闲；不能平均每窗口p95当总体p95，也不能用静止0FPS评价交互性能。
