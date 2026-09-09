# LIST 原稿图标

这些图标来自设计稿已安装的 `@phosphor-icons/react` **2.1.10**，使用 `light` 权重，未重画、未加粗、未裁剪路径。上游项目：<https://github.com/phosphor-icons/react>。MIT 许可全文见 `LICENSE.phosphor`，分发这些 SVG、PNG 或生成的 C 资源时保留该许可。

## 来源

本项目 Windows 设计稿中的 `design_main_20260908/src/screens/ListScreen.jsx` 使用该库。保存的 SVG 保留上游 `IconBase` 的 `viewBox="0 0 256 256"`，`path d` 与下列已安装原文件的 `light` 项一致：

- `design_main_20260908/node_modules/@phosphor-icons/react/dist/defs/Receipt.es.js`
- `design_main_20260908/node_modules/@phosphor-icons/react/dist/defs/Barcode.es.js`
- `design_main_20260908/node_modules/@phosphor-icons/react/dist/defs/WarningCircle.es.js`

SVG 是长期重生源，后续使用不依赖 Windows 设计稿、React 或网络。透明 PNG 是运行时资产，放入 `aic_ui/lvgl_data/list_icons`；该目录另附一份 `LICENSE.phosphor`，随固件升级包分发版权许可，SVG 和生成工具不进入运行时资源目录。

## 交付规格

| PNG | 尺寸 | 填充颜色 | 用途 |
| --- | --- | --- | --- |
| `receipt_24.png` | 24 × 24 | `#8394A0` | 面额栏标题右侧 |
| `barcode_24.png` | 24 × 24 | `#8394A0` | 冠字号栏标题右侧 |
| `warning_circle_24.png` | 24 × 24 | `#8394A0` | 拒钞栏标题右侧 |
| `barcode_36.png` | 36 × 36 | `#879BA8` | 冠字号空态 |
| `warning_circle_36.png` | 36 × 36 | `#879BA8` | 拒钞空态 |

以上尺寸和颜色按本轮固件适配要求确定；原始 React 页面标题为 20px、空态为 28px，拒钞空态为 ShieldCheck。本次保留指定三种图标的原始 light 轮廓，以更大的原生像素尺寸导出，不依赖 LVGL 运行时缩放。PNG 使用 8-bit RGBA/sRGB、透明背景、抗锯齿 alpha。

## 可选手工重生

需 Node.js 与 `sharp`。本次使用 bundled runtime：`sharp 0.35.4`、`libvips 8.18.6`、`librsvg 2.62.91`；实际依赖版本也由脚本输出。在可解析 `sharp` 的 Node 环境运行：

```sh
node tools/icon_sources/list/generate-list-icons.cjs
```

Windows bundled runtime 示例，从固件镜像根目录运行：

```powershell
$env:NODE_PATH = 'C:/Users/Administrator/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules'
& 'C:/Users/Administrator/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node.exe' tools/icon_sources/list/generate-list-icons.cjs
```

脚本直接将 SVG 渲染为目标尺寸，不先放大再缩图；生成前验证尺寸、RGBA、透明背景、非空像素、抗锯齿和边界，并输出每张图 SHA-256。固定 SVG、脚本及 sharp/libvips/librsvg 版本可复现 PNG；更换渲染器版本后应重新检查像素。普通固件 `make` 只消费已纳入资产流程的 PNG 并经现有 converter 转 C，不需要安装 Node、sharp 或 SVG 渲染器。
