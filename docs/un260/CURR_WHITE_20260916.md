# Currency 卡片统一白色

- 原先非高亮背景 #F7F8FA、高亮背景 #FFFFFF；均为不透明，并非透明度降低。
- 统一为 #FFFFFF，焦点变化仅改变边框颜色，删除重复背景设置。
- 缓存键升级 V7，缓存与实时回退使用相同配色。保留缓存窗口和预热策略。
- 不改变滚动速度、阻尼、浮起位置、选中/收藏标记或币种协议。
- 包含此前已完成的 Ready 文本组 360 ms 和自检提示修复。

验证：Currency 滚动/缓存/生命周期、AUT/MUL 协议和页面恢复主机回归通过；完整 make 通过，162 个载荷与构建 target 匹配。板上视觉和帧率待验证；未刷机、未提交 Git。

Linux 包：`/home/pc/d213-100ask/d211/output/d211_d213_devkitf/images/UN260_UPDATE.upk`

SHA256：`2ff3ca523d3674ad3b14da4f19cfd332d892b9f4ef262bb0000fca4ceea7f1ae`
