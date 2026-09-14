#!/usr/bin/env python3
"""Test actual selection function without opening host input devices."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root/'lv_port_indev.c').read_text()
function = source.split('static bool evdev_open_runtime_device(void)', 1)[1].split('/**********************', 1)[0]
fixture = r'''
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glob.h>
#define EVDEV_NAME "/dev/input/event0"
static unsigned opened, freed;
static bool missing;
static bool evdev_is_touch_device(const char*p) { return !missing && !strcmp(p,"/dev/input/event20"); }
static bool evdev_set_file(const char*p) { assert(strcmp(p,EVDEV_NAME)); opened++; return true; }
static int test_glob(const char*p,int f,int(*e)(const char*,int),glob_t*g) {
    (void)p;(void)f;(void)e;
    static char *paths[]={"/dev/input/event0","/dev/input/event20"};
    g->gl_pathc=2; g->gl_pathv=paths; return 0;
}
static void test_free(glob_t*g) { (void)g; freed++; }
#define glob test_glob
#define globfree test_free
static bool evdev_open_runtime_device(void)
'''+function+r'''
int main(void) {
    unsetenv("LVGL_EVDEV_DEVICE"); unsetenv("TSLIB_TSDEVICE");
    assert(evdev_open_runtime_device() && opened==1 && freed==1);
    missing=true;
    assert(!evdev_open_runtime_device() && opened==1 && freed==2);
    setenv("LVGL_EVDEV_DEVICE","/dev/input/event7",1);
    assert(evdev_open_runtime_device() && opened==2 && freed==2);
    puts("PASS input: rejects keyboard fallback, discovers event20, frees scan, preserves explicit selection");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-input-selection-') as directory:
    tmp=Path(directory); src=tmp/'test.c'; exe=tmp/'test'
    src.write_text(fixture)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(src),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
