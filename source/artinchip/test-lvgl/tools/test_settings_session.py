#!/usr/bin/env python3
"""Regression for navigation reset dispatch. Actual LVGL lifecycle: test_settings_view.py."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
settings = (root / "un260/lv_core/page_06_settings.c").read_text()
manager = (root / "un260/lv_core/lv_page_manager.c").read_text()

def function(source, name):
    match = re.search(r"^\w[^\n]*\b" + name + r"\([^;]*?\)\s*\{", source, re.M)
    assert match, name
    end = source.index("{", match.start()) + 1
    depth = 1
    while depth:
        if source[end] == "{": depth += 1
        if source[end] == "}": depth -= 1
        end += 1
    return source[match.start():end]

registry = manager[manager.index("static const ui_page_registration_t g_page_registry"):]
entry = registry[registry.index("[UI_PAGE_SETTING]"):registry.index("[UI_PAGE_SET_PASSAGE]")]
assert ".reset_navigation = ui_page_06_settings_reset_navigation" in entry
for name in ("ui_manager_switch", "ui_manager_adopt_precreated_page"):
    assert "ui_manager_history_on_commit(page)" in function(manager, name)
assert "reset_navigation" not in function(settings, "ui_page_06_settings_suspend")

code = r'''
#include <stdbool.h>
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "un260/lv_core/lv_page_manager.h"
static int object, nodes[3];
static int *settings_page, *catalog, *scope;
static int scroll_positions[96];
static unsigned renders, other_resets;
static int *default_scope(void){return catalog;}
static void render(void){assert(scope==catalog);renders++;}
'''
code += function(settings, "ui_page_06_settings_reset_navigation")
code += r'''
static void reset_other(void){other_resets++;}
static const struct { void (*reset_navigation)(void); } g_page_registry[UI_PAGE_COUNT]={
    [UI_PAGE_SETTING]={ui_page_06_settings_reset_navigation},
    [UI_PAGE_MENU]={reset_other},
};
'''
code += function(manager, "ui_manager_reset_navigation_sessions")
code += r'''
static void dirty(void){scope=nodes+2;for(unsigned i=0;i<96;i++)scroll_positions[i]=200;}
static void check(void){assert(scope==catalog);for(unsigned i=0;i<96;i++)assert(!scroll_positions[i]);}
int main(void){
    dirty();ui_manager_reset_navigation_sessions();check();assert(!renders && other_resets==1);
    catalog=nodes;dirty();ui_manager_reset_navigation_sessions();check();assert(!renders);
    settings_page=&object;dirty();ui_manager_reset_navigation_sessions();check();
    assert(renders==1 && settings_page==&object);
    ui_manager_reset_navigation_sessions();check();assert(renders==2 && other_resets==4);
    settings_page=NULL;dirty();ui_manager_reset_navigation_sessions();check();assert(renders==2);
    puts("PASS: Settings stable scope and all scroll offsets reset; cache retained; no-cache and repeated Home safe");
}
'''
with tempfile.TemporaryDirectory(prefix="un260-settings-session-") as temporary:
    work = Path(temporary)
    (work / "test.c").write_text(code)
    subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=undefined", "-I"+str(root), str(work/"test.c"),
                    "-o", str(work/"test")], check=True)
    subprocess.run([str(work/"test")], check=True)
