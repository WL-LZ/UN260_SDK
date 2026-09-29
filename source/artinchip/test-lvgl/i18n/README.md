# UN260 国际化资源

本轮迁移文本和切换基础设施，不新增发布语言。设置页仍只开放 English。
`zh-Hans` 和 `ko` 保存已有译文作为草稿；其完整性、全界面字体和排版尚未达到发布条件。新增源文案以英文回退，不自动生成或假定译文。

## 资源与运行时

- `locales.json`：稳定 ID、BCP 47 标签、原生显示名、方向、发布状态、译文／字体／塑形就绪状态。持久化只保存标签，不保存数组下标。
- `locales/*.json`：每语言一个词典，格式是 JSON，也属于 YAML 子集，可交给官方生成器。旧 `ui_text_get(enum)` 的稳定字符串 key 和新英文 msgid 共用这套词典。
- `fragments/*.json`：按模块维护的词条片段；同 key 的冲突译文会阻止构建。明确上下文不同的词用 `ui_trc(context, msgid)`，内部遵循 gettext 的 `context + EOT + msgid` 表达。
- `legacy_ids.json`：旧 enum 到稳定 key 的映射；增删语言无需改变 enum 或三列结构。
- `generated/`：入库的纯 C 资源、注册表和校验清单。固件不解析 JSON/YAML，不需要 Node，也不联网。
- `upstream.json`、`../tools/vendor/`：固定 `lv_i18n` 0.2.1 npm 发布包及 SHA-256、MIT 许可证。版本对应上游提交 `8bf1fd3ab37251667be11f27efcadb1ae8cd3e25`；不混用同版本号的后续 master 模板。

采用 [LVGL 官方 lv_i18n](https://github.com/lvgl/lv_i18n) 的 locale、英文回退和 CLDR 复数机制。针对嵌入式热路径，生成后对上游 C 做小范围适配：译表 `const`、生成表长、按 UTF-8 key 二分查询；同时修复 `INT32_MIN` 复数绝对值溢出。新增显式 locale 的只读查询支持报告导出的语言快照，不改全局语言。补丁过程集中在 `tools/gen_i18n.py`，生成文件禁止手改。

## 调用约定

```c
lv_label_set_text(label, ui_tr("Save"));
static const char *const labels[] = { UI_N_("Save"), UI_N_("Cancel") };
lv_label_set_text(label, ui_tr(labels[index]));
snprintf(buffer, sizeof(buffer), ui_tr("Saved %u records"), count);
snprintf(buffer, sizeof(buffer), ui_trn("%u notes", count), count);
```

`UI_N_` 只是源文案标记。普通显示控件仍需 `ui_tr`；通知或消息描述符的 key 接口则保存源 key，在展示时翻译。不要把已翻译字符串存成业务状态，也不要二次翻译用户姓名、备注、冠字号、文件名、币种代码、版本、协议码或机器返回的原始数据。

复数资源例如：

```json
{"en": {"%u notes": {"one": "%u note", "other": "%u notes"}}}
```

参数应通过完整格式模板翻译；不要拼接半句。生成检查校验 `%s`、整数位宽、星号精度参数和位置参数的类型对应，拒绝 `%n`。`ui_message` 保存模板及已复制参数，后台任务和待展示通知不用保存翻译后的快照。

词条所属页面／控件／说明／状态／协议说明／导出等类别用于管理翻译。通知 `UI_NOTICE_SUCCESS/ERROR/WARNING/PROGRESS/INFO` 属于业务结果的呈现类型；不得按翻译后的字符串猜测成功、失败或严重性。

## 切换与持久化

`ui_lang_save(id)` 仅接受已发布且资源就绪的 locale。先由 `storage/ui_locale_store` 原子保存 `/etc/ui_state/language.cfg` 的标签，并同步文件和目录，再切换 UI 语言、增加 generation。写入失败不切换运行中的语言；若 rename 成功而目录 fsync 失败，磁盘可能已有新值，但其持久性未确认，因此界面报告未确认，不报告成功。

提前显示的启动路径仍由 worker 读取标签、UI 线程应用；没有在提前亮屏前新增磁盘 I/O。缺失、损坏、未知、已删除或未发布的标签回退 English。低层 `ui_lang_set` 保留为测试／草稿预览接口，不落盘；生产业务不得在后台线程调用它。

正式语言切换必须在语言设置页 page21 的 Save 成功边界统一让页面管理器处理缓存，再离开语言页；低层 `ui_lang_set` 不能代替这套事务，也不会为调用者擅自销毁当前页。后台操作状态、页面草稿、故障当前步骤、通知剩余时间和队列独立于语言，不以销毁业务状态实现刷新。`ui_lang_generation()` 是 UI 线程观察切换的唯一代际。报告导出可在任务创建时保存 `language_t`，用 `ui_tr_for` 读取这份语言的不可变词典，不干扰当前界面。

## 离线维护

```sh
python3 tools/gen_i18n.py --extract --generate --node node
python3 tools/gen_i18n.py --check
python3 tools/test_i18n.py --sanitize
```

`--extract` 只补英文 key。`--generate` 临时解开经哈希验证的官方包，使用本机 Node，保留其依赖及许可证，不执行安装脚本。正常 make 只执行 `--check`：检查词典冲突、printf 参数、发布语言完整性、上游包哈希及生成文件新鲜度。不要在设备运行时扫描词典或解码字体。

新增或删除语言的步骤：

1. 添加或移除独立 locale 文件和注册表项；保留已有 ID，不重排为持久化标识，不删除默认 English。
2. 添加经过审核的译文、所需复数形式；新增语言先 `enabled: false`。未译词条在研发预览中回退英文，发布检查不允许把缺词条的语言标为启用。
3. 当前 `font_profile` 是能力元数据，页面仍有显式字体指针，尚未形成全项目字体角色解析器。发布新语言前优先统一 font role/resolver，并覆盖所有真实字号、字体风格、控件与提前启动／维护 renderer；不能只把 `font_ready` 改成 true。为该文字系统提供字体 profile、字形覆盖和容量验证。文本较长时测试所有真实尺寸的截断、换行、按钮和列表；不能仅检查字符存在。
4. 对 RTL 和复杂塑形完成混合数字、货币、协议码、标点、输入与导出测试，再声明相应 readiness；最后启用语言。语言设置页自动读取可用注册表项。
5. 重新生成，跑主机测试、完整交叉构建和升级包一致性检查，再做实机显示与切换验证。

注册表不是固定三语上限，语言数量受字体、应用空间、实际内存和验证范围限制。当前上游每个词典最多 65,534 条，本项目在生成阶段显式检查；不以添加一个枚举值等同于支持一种语言。

## 未来目标语言的条件

用户列出的目标可规划为十种语言：简体中文、英语、法语、土耳其语、阿拉伯语、日语、韩语、乌尔都语、印地语、俄语。埃及与迪拜可先共用现代标准阿拉伯语；有地区文案差异再分 `ar-EG`、`ar-AE`。巴基斯坦先评估乌尔都语；印度并非单一语言市场，印地语是候选，目标语言需产品确认。本轮没有新增这些包。

法语、土耳其语、俄语需要各自完整字符及大小写／换行验证；日语需要假名和汉字。阿拉伯语、乌尔都语需要 RTL 与连接字形，印地语需要天城文复杂塑形，均不能仅换字库。目前 LVGL 8.3 的默认配置未打开 BiDi／Arabic-Persian 开关。官方 8.3 对 Arabic/Persian 的处理还有静态文本和输入控件限制，见 [LVGL 8.3 字体与双向文本说明](https://lvgl.io/docs/open/8.3/overview/font)。复杂文字须先验证相应塑形方案和性能，不能把 UTF-8 编码支持宣称为完整语言支持。

## 提前开机与维护画面的边界

`boot_light.c` 在 LVGL 和设置读取之前用预烘焙位图显示 Welcome；`tools/build_boot_light_assets.py` 从现有英文欢迎字形生成资源。本轮保留该英文启动资源、尺寸和时序，不在提前亮屏路径增加语言文件读取、字体解码或新的 I/O。LVGL 接管后的 Welcome 文案已有语言入口；今后发布非英语包前，必须同时明确本地化启动资源与接管画面的配套策略。

独立维护画面 `upgrade_display.h` 的原生字形仅支持 A–Z、0–9，明确使用 `ui_tr_for(LANGUAGE_EN, ...)` 和英文升级状态模板；它共享源词条分类和纯 C 查询，但不声明支持 CJK、RTL 或塑形，也不依赖 LVGL。升级协议、外部状态原文与日志仍保持稳定，未知诊断按数据显示，不作为 printf 格式执行。新语言发布不能绕过这两个 renderer 的资源能力限制。
