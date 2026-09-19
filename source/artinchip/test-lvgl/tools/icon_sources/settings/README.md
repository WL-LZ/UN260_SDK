# Settings icon assets

Approved settings HTML uses Lucide vectors. These same definitions generate the firmware PNGs; no Unicode substitute icons are used by the new settings components.

From a Node.js environment with sharp installed, run: `node tools/icon_sources/settings/build.cjs`.

Output: `aic_ui/lvgl_data/settings_icons`, ten 24×24 RGBA PNGs, neutral #536B79, stroke 1.8, 4× supersampling. LVGL recolors the same alpha pixels to #1463CF for the selected state; duplicate active-color images are not shipped. The existing compiled-asset pipeline includes this directory automatically. Definitions and ISC/MIT notices are retained alongside this script.
