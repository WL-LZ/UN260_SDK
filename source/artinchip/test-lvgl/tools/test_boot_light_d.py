#!/usr/bin/env python3
"""Reuse real lease/vsync/memory guard tests with the fourth-theme scalar oracle."""
from pathlib import Path
s=Path(__file__).with_name('test_boot_light.py').read_text()
s=s.replace('#define UI_BOOT_ANIM_THEME 3','#define UI_BOOT_ANIM_THEME 4')
s=s.replace("boot_theme_c/boot-light.bin","boot_theme_d/boot-light.bin")
s=s.replace('p=progress(elapsed,BOOT_WELCOME_START,BOOT_WELCOME_DURATION);q=1-p;', 'p=progress(elapsed,BOOT_WELCOME_START,BOOT_WELCOME_DURATION);q=0;')
s=s.replace('  float alpha=ease(progress(elapsed,BOOT_DOT_START+i*120,450))', '  if(elapsed<BOOT_DOT_START+i*160U || elapsed-BOOT_DOT_START-i*160U>=3U*BOOT_DOT_PERIOD)jump=0;\n  float alpha=ease(progress(elapsed,BOOT_DOT_START+i*120,450))')
s=s.replace('const uint8_t color[4]={0x20,0x58,0xf8,255}', 'const uint8_t color[4]={0x9e,0x87,0x7c,255}')
s=s.replace('un260-boot-light-20260914','un260-boot-light-d')
s=s.replace('float alpha=ease(progress(elapsed,BOOT_DOT_START+i*120,450))', 'jump=boot_d_dot(elapsed,i);\n  float alpha=ease(progress(elapsed,2300U,800U))')
s=s.replace('BOOT_WELCOME_MOTION_SCALE*6*jump','BOOT_WELCOME_MOTION_SCALE*5*jump')
exec(compile(s,str(Path(__file__)), 'exec'))
