# Interaction R2：退出生命周期、交互动画意图与 Currency 运动

> 历史设计记录：本文解释 R2 的问题证据和实现决策，不是新的板上验收结论。
> Currency 已经完成 R3/R4 及职责整理，并独立提交为 `f8c036522`；最终缓存渲染/布局/视图归属以 `CURRENCY_DESIGN.md` 为准。
> 本次归档中的待提交实现是 Innovation 生命周期与 Main 动画意图修复，不能据本文把已提交 Currency 重复算作新改动。

## 证据与边界

用户 2026-09-08 两份日志分别为 `286ad835-8980-43ac-9e8f-81c457c1c4b8` 和 `aa3087d0-9ca3-49ec-aa24-694a34d2d414`。两条路径均记录了 `INNOVATION_BACK event=end`，分别为221ms/234ms、10次显示提交，之后停在 `NAV_BACK to=1`；这一段的 `submit_error=0`。日志将故障范围收窄到退出收尾，但内存错误的因果证据来自代码及真实 LVGL 定时器回归，不是仅凭日志末行猜测。

### Innovation 根因

旧调用链：返回异步回调 → pop page → suspend → cancel 同一个正在执行的异步回调。项目 LVGL 8.3.2 的 async trampoline 在业务回调返回后还会释放回调信息，因而发生重复释放。双指 HOME 走不同的收尾路径，所以可以正常工作。

新增 `lv_core/ui_deferred_action`，为主线程延后操作提供一个可取消的单槽 owner。派发前先清空 owner、移除 timer，才执行业务回调；回调后不再读取 owner/timer，允许回调中取消、重新排队或销毁 owner。Innovation 的展开、取消、退出统一接入，不修改 LVGL 分配器或显示驱动来掩盖错误。

进入动画不改；退出仍是完整页面快照整体上移，180ms缓出。准备快照和首帧提交另有耗时，180ms不是点击到完成的总延迟。

### Main 根因

上一轮帧合并只合并数据dirty位，未保存 `anim_en=true`。例如速度成功路径先要求动画刷新，随后普通刷新；合并后只执行普通刷新，因此原160ms文字滑入未启动。

保留现有按钮接口，用内部动画意图随dirty请求进行 OR 合并。一批中任何交互要求动画都不会被后续普通刷新覆盖，消费一次后清除；隐藏/导航不补播旧交互，同文本刷新不取消刚启动的动画。没有关闭可见内容统一提交。

## Currency：研究后选择的模型

参考的是成熟实现的职责划分，不移植 Web/Swift 运行时，也不声称复刻 Apple 未公开的内部参数：

- [Apple UIScrollViewDelegate](https://developer.apple.com/documentation/uikit/uiscrollviewdelegate/scrollviewwillenddragging(_:withvelocity:targetcontentoffset:))：释放速度与预期停点分开，允许调整最终目标。
- [Embla 8.6 ScrollBody](https://github.com/davidjerleke/embla-carousel/blob/v8.6.0/packages/embla-carousel/src/components/ScrollBody.ts)、[DragTracker](https://github.com/davidjerleke/embla-carousel/blob/v8.6.0/packages/embla-carousel/src/components/DragTracker.ts)、[ScrollBounds](https://github.com/davidjerleke/embla-carousel/blob/v8.6.0/packages/embla-carousel/src/components/ScrollBounds.ts)：把速度、目标吸引、拖动采样及边界约束分离。
- [LVGL 8.3 Scroll](https://lvgl.io/docs/open/8.3/overview/scroll)：原生已具备惯性、弹性、吸附，但本项目使用手动拖动与全局手势抢占；再叠加原生momentum会形成两个运动控制者。因此保留LVGL对象/事件/绘制，仅替换旧页面运动算法。

### 1. 布局与焦点

保持1280×400、左侧288px摘要、右侧992px浏览区。卡片200×265，步长228（间隔28）；Focus X为浏览区344，即全屏632，接近屏幕视觉中心。

中部停稳时卡面左坐标约为 `16 / 244 / 472 / 700 / 928`：四张完整卡及下一张64px（约1/3）。首尾为了让第一/最后一张也到达焦点，允许必要留白；不是无限循环列表。卡片数量由实际货币目录决定，不写死为10张。

浏览焦点只读运动位置。左摘要和实际货币保持原业务确认语义；单击卡片仍请求切币，AUTO/manual模式切换与主控成功/失败回复链不变，不改为滑到哪里就自动发切币命令。

### 2. 拖动与释放速度

跟手期间直接投影内容位置，不加Ease动画。按最近100ms的位移/时间估算释放速度，并在快速反向时舍弃反向之前的采样；手指停住90ms后释放不再使用旧速度。速度限幅±3600px/s。

松手进入统一状态机 `DRAG → COAST → SPRING → IDLE`；再次触摸会截停当前运动并从当前视觉位置续拖。点击截停惯性不会误当成切币点击。

### 3. 惯性与Snap

自由减速取 `v(t)=v0·exp(-6.5t)`，t单位秒。预测停点为 `x + v/6.5`，限制在有效范围后就近取228px间隔的吸附点。快慢同位移因此可以得到不同落点，不再按位移强制跳1–2格，也不再停500ms后启动另一段吸附。

进入目标附近后带着当前速度接入阻尼弹簧：质量归一为1，`ω=20/s`、`ζ=0.86`，即 `a=400·(target-x)-34.4·v`。内部按最多8ms小步处理真实经过时间；低帧率或100/200ms长帧不丢失运动时间。位置误差<0.35px且速度<4px/s时精确落点并停timer；超过2.5s的异常中断按合法目标收尾。

### 4. 边界

越界位移d投影为 `0.42d / (1 + 0.42|d|/56)`，左右使用相同公式，视觉越界渐近56px。松手回到合法首末吸附点，不无限滑出，不分别维护左右两套动画。

### 5. Focus反馈

按卡片中心距离计算0–1平滑权重（smoothstep），连续最多上移8px，并使用简单薄边框提升层级。卡间距不因焦点切换而改变。

本轮选择1:1卡面，不做动态缩放、组透明度、旋转或动画阴影：旧NORMAL和SELECTED快照尺寸/内容坐标不同，切换本身会引入约10%尺寸跳变；连续微缩还会增加目标尺寸缓存开销。采用版本化单卡面缓存、固定几何与live fallback，先保证运动连续性。

### 6. 项目归属与生命周期

- `lv_components/ui_scroll_physics`：无LVGL、无分配、无I/O的通用一维物理状态。
- `page_07_curr/page_07_curr_carousel`：LVGL事件、唯一运动timer、点击/拖动仲裁与诊断。
- `page_07_curr.c`：货币业务、现有页面构建、可见卡面/焦点投影。

全区域包含卡片间空白及收藏子按钮，都走同一拖动阈值。侧边/多指仍由现有全局原始触摸观察者优先处理；不覆盖该单槽观察者。PRESS_LOST、切CARD/GRID、过滤重建、suspend/destroy都会收尾运动，避免隐藏页timer持续运行。

### 7. 性能与验收

不逐帧创建对象/生成快照，不每帧切图或遍历改整棵样式树；固定卡面使用现有DMA缓存，缺缓存保留live绘制，不扩大现有预算。运动只统一平移卡片条及更新邻近卡片的小幅抬升。

纯物理测试验证不同采样/刷新周期、反向、停住后松手、边界镜像、时间回绕、0/1/34项和最终落点；LVGL适配测试验证点击、收藏、空白拖动、PRESS_LOST、timer分配失败和销毁。主机测试不等于板上帧率/触感验证，不能预报固定FPS或提升百分比。
