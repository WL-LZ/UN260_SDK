UN260 新增语言维护流程
====================

权威源码在 Linux：/home/pc/d213-100ask/d211/source/artinchip/test-lvgl。
以下命令均从该目录执行。Windows 镜像不是固件构建仓库。
修改前保存现有差异；不要覆盖其他人的翻译和业务修改。

1. 新增语言配置
---------------
先在 i18n/font_profiles.json 配置有使用许可、覆盖目标语言的字体源。
TTC 文件必须指定对应地区的 face_index；已有中文/韩文配置用了 face_indices。
例如法语可沿用 latin 配置：

  python3 tools/language_workflow.py add fr --name Français --code FR --direction ltr --font-profile latin --plural-forms one,other

plural-forms 必须与本项目固定版本的 lv_i18n 复数规则对应，不能照抄别的语言。
add 自动追加稳定 ID，创建空翻译，并保持入口关闭。不要重排、复用旧 ID。
阿拉伯语需要 rtl 和 zero,one,two,few,many,other；中日韩为 other。
其他文字系统不是选一个字体即可完成：必须核对 LVGL 的塑形和双向排版支持。

2. 导出翻译模板
---------------
  python3 tools/language_workflow.py export fr /tmp/un260-fr.json

模板包含全部合并后的英文源文案，不仅是 locales/en.json。
交给译员修改 entries 下的 translation；不要修改 key、source、locale、source_sha256。
复数文案的每个类别都要填写。%u、%s、%lu 等参数的类型和数量必须保留。
允许用正确的参数编号调整顺序，禁止新增 %n。
点钞语境要区分：清点、收入、支出、退钞、预置张数、冠字号、撤销、更正。
操作员名称、备注、冠字号、币种代码是用户数据，不应翻译。
英语字母键盘保持英语；不把键盘方向随阿拉伯语整块翻转。

3. 导入翻译
-----------
  python3 tools/language_workflow.py import fr /tmp/un260-fr.json

会拒绝过期源文案、错语言、缺少键、空翻译、参数不匹配、缺少复数形式。
已有翻译仍写回原来的 fragment，不建立冲突的重复译文；其他语言保持原样。
翻译改变后，该语言入口重新关闭，必须重新验收。
如果提示源文案变了，重新导出并合并旧译文，不要伪造 source_sha256。

4. 自动生成字体并检查
--------------------
需要 Python 3、fontTools、Pillow、Node、lv_font_conv、主机 C 编译器及本项目 LVGL 源码。
使用团队固定的 lv_font_conv 版本，把实际路径传给 --converter。

  python3 tools/language_workflow.py check --converter /实际路径/lv_font_conv.js --output /tmp/un260-fr-qa

检查包含：源文案、占位参数、字库字符覆盖、字形塑形、字形高度、已登记控件宽度、
资源损坏/加载失败回退、设置预览、保存、现金/操作员页面及 Menu 子页的实际 LVGL 渲染。
输出目录保存检查图片和字体检查报告；检查失败时不会继续构建。
大字号状态文字使用 i18n/font_contracts.json 指定文案子集，避免每个大字号都装入整套译文。
如果新增大字号文字，必须同步登记其文案键，再生成字体并检查实际页面。

自动检测不是所有动态数据和硬件触摸的证明。需要检查图片中的文字相互遮挡、
边框留白、数值和币种顺序，并覆盖弹窗、长名称、空列表、错误状态和页面返回。
长文案优先准确缩短、合理分配宽度；说明允许换行，标题和按钮保持一行。
不要靠随意缩小字号、隐藏必要说明或让金额省略显示来通过检查。

5. 开放语言并构建匹配升级包
---------------------------
完成该语言的专业术语、页面、保存/恢复验收后，在 i18n/locales.json 将该语言的
translations_ready、font_ready、shaping_ready、enabled 设为 true。
新文字系统未获得塑形验证时不能设置 shaping_ready=true。
运行：

  python3 tools/language_workflow.py build --converter /实际路径/lv_font_conv.js --output /tmp/un260-release-qa

此命令先完整执行上述检查，再依次重建共享 LVGL、应用和完整 make。
SDK 根目录、板型配置必须正确；使用 make -j2 BR2_JLEVEL=2 避免构建内存峰值。
按 SDK 输出核对 UN260_UPDATE.upk、应用、共享库、资源和 SHA-256。
不要单独拷贝新应用搭配旧共享库，也不要从 Windows 原型目录构建固件。
升级屏幕板后还需验证：语言保存、应用重启/断电恢复、页面往返、真实触摸、反复切换。
屏幕板没有主控时出现握手/自检超时属于该测试环境，不代表语言切换失败。

常见问题
--------
缺字：先看实际控件是否经过 ui_message_font，再检查字体源和对应字号子集。
首次打开卡顿：语言列表只能准备名称使用的字号，不得为了列出语言而加载所有语言的完整字库。
字体解析必须使用字体专用读取缓存；不要改变图片/DMA 的文件读取策略。
主机上的 stdio 缓冲可能掩盖板端逐字节 POSIX 读取的成本，必须测板上首次加载。
不得通过延长保护超时或伪造主循环心跳来通过语言加载测试。
切回页面变成旧语言：检查页面语言代次、缓存失效和恢复路径，不要只刷新当前标题。
字体资源损坏：保持原设置；重启无法加载保存语言时回到英语，不应显示方框后继续保存。
新语言入口不出现：检查四个 readiness/enabled 标志及是否重新生成和构建。
字符正常但重叠：检查实际布局与字形高度；不能只以翻译字符串长度判断。

旧 four_language_terms.tsv 是首批翻译整理材料；日常维护以 locales/*.json 与
fragments/*.json 的合并结果为准，不要再用旧 TSV 覆盖已导入的维护译文。
