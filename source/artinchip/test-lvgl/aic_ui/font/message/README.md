# Message and machine-fault glyph fallback

`UN260MessageCJKSubset.otf` is a renamed subset of the locally installed
Noto Sans CJK SC Regular face from `fonts-noto-cjk`. It contains the Chinese,
Korean and non-ASCII punctuation used by the message catalogues. Copyright
and the complete SIL Open Font License 1.1 are retained in `OFL.txt` and the
font metadata. The source TTC and subset SHA-256 values are in `manifest.json`.

Only `ui_message_font()` consumers use this fallback. Instrument Sans remains
the primary face with its original English metrics; no existing global font
object is modified. Generated C bitmaps are 4 bpp at 12/14/16/18/20/22/24/28 px.
They are loaded as constant font data, without per-frame file reads or decoding.

To fit the factory application partition, each size contains its actual text
roles: 12 px contains the fault view/safety/count labels; 14 px contains fault
locations, step tabs and Chinese/Korean notification metadata; 16/28 px contain
fault titles; 24 px contains the fault catalogue. General notification text at
18/20/22 px retains the complete multilingual message character set. No global
LVGL font-compression setting is changed. `codepoints_by_size` records each
generated face; the build check and real-LVGL label tests enforce coverage.

Normal firmware builds use the checked-in generated C files. Check after changing
translated notice or fault text:

```sh
python3 tools/gen_message_fonts.py --check
```

To regenerate, provide an already installed converter; the script does not fetch
network packages. The default source and copyright paths are those supplied by
Debian/Ubuntu `fonts-noto-cjk`, and fontTools is also required:

```sh
python3 tools/gen_message_fonts.py --converter /path/to/lv_font_conv.js
```

The generated derivative remains under SIL OFL 1.1. A new character requires
regeneration rather than allowing LVGL's missing-glyph box into the UI.
