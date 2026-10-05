#!/usr/bin/env python3
"""Compile actual manager data publication and its quick-host registrations.

Rendering callbacks are observed behind a tiny adapter; the publication,
visible-owner selection and frame coalescing run their production code.
"""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'un260/lv_core/lv_page_manager.c').read_text()

def function(name):
    match = re.search(r'^(?:static )?[\w *]+\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
    assert match, name
    brace = source.index('{', match.start())
    depth, end = 1, brace + 1
    while depth:
        if source[end] == '{': depth += 1
        elif source[end] == '}': depth -= 1
        end += 1
    return source[match.start():end]

registry = source[source.index('static const ui_page_registration_t g_page_registry'):]
registrations = []
for page in ('UI_PAGE_MAIN', 'UI_PAGE_PURE'):
    match = re.search(r'\[' + page + r'\]\s*=\s*\{([^}]+)\}', registry)
    assert match, page
    fields = re.findall(r'\.(?:data_topics|refresh_data)\s*=\s*[^,]+,', match.group(1))
    registrations.append('[' + page + ']={' + ''.join(fields) + '}')

code = r'''
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/ui_frame_commit.h"
#include <assert.h>
#include <stdio.h>
#include <stddef.h>
typedef struct {ui_data_topic_t data_topics;void (*refresh_data)(ui_data_topic_t);} ui_page_registration_t;
static struct {ui_page_t current;} g_page_manager;
static bool g_page_cache_ready[UI_PAGE_COUNT];
static ui_data_topic_t g_page_data_dirty[UI_PAGE_COUNT];
static unsigned refreshes[UI_PAGE_COUNT];
static void page_01_main_quick_refresh_data(uint32_t topics){
 assert(topics==UI_DATA_TOPIC_DEVICE_VERSION);
 refreshes[g_page_manager.current]++;
}
'''
code += 'static const ui_page_registration_t g_page_registry[UI_PAGE_COUNT]={' + ','.join(registrations) + '};\n'
code += function('ui_manager_commit_visible_data') + '\n' + function('ui_manager_publish_data_changed')
code += r'''
int main(void){
 g_page_cache_ready[UI_PAGE_MAIN]=g_page_cache_ready[UI_PAGE_PURE]=true;
 g_page_manager.current=UI_PAGE_PURE;
 ui_manager_publish_data_changed(UI_DATA_TOPIC_DEVICE_VERSION);
 assert(refreshes[UI_PAGE_PURE]==1&&refreshes[UI_PAGE_MAIN]==0);
 assert(g_page_data_dirty[UI_PAGE_MAIN]==UI_DATA_TOPIC_DEVICE_VERSION);
 assert(g_page_data_dirty[UI_PAGE_PURE]==0);
 /* Repeated 0x17 replies in one tick commit once to the active theme. */
 ui_frame_commit_begin_batch();
 ui_manager_publish_data_changed(UI_DATA_TOPIC_DEVICE_VERSION);
 ui_manager_publish_data_changed(UI_DATA_TOPIC_DEVICE_VERSION);
 assert(refreshes[UI_PAGE_PURE]==1&&ui_frame_commit_pending());
 ui_frame_commit_end_batch();ui_frame_commit_flush();
 assert(refreshes[UI_PAGE_PURE]==2&&refreshes[UI_PAGE_MAIN]==0);
 /* A theme change before the deferred commit refreshes its current owner,
  * retaining the outgoing host's dirty topic for a later activation. */
 ui_frame_commit_begin_batch();ui_manager_publish_data_changed(UI_DATA_TOPIC_DEVICE_VERSION);
 g_page_manager.current=UI_PAGE_MAIN;ui_frame_commit_end_batch();ui_frame_commit_flush();
 assert(refreshes[UI_PAGE_MAIN]==1&&refreshes[UI_PAGE_PURE]==2);
 assert(g_page_data_dirty[UI_PAGE_MAIN]==0&&g_page_data_dirty[UI_PAGE_PURE]==UI_DATA_TOPIC_DEVICE_VERSION);
 g_page_manager.current=UI_PAGE_STANDBY;
 ui_manager_publish_data_changed(UI_DATA_TOPIC_DEVICE_VERSION);
 assert(refreshes[UI_PAGE_MAIN]==1&&refreshes[UI_PAGE_PURE]==2);
 assert(g_page_data_dirty[UI_PAGE_MAIN]==UI_DATA_TOPIC_DEVICE_VERSION);
 g_page_manager.current=UI_PAGE_PURE;g_page_cache_ready[UI_PAGE_PURE]=false;
 ui_manager_commit_visible_data(NULL,0);assert(refreshes[UI_PAGE_PURE]==2);
 g_page_cache_ready[UI_PAGE_PURE]=true;ui_manager_commit_visible_data(NULL,0);
 assert(refreshes[UI_PAGE_PURE]==3&&g_page_data_dirty[UI_PAGE_PURE]==0);
 ui_manager_publish_data_changed(UI_DATA_TOPIC_NONE);
 ui_manager_publish_data_changed(UI_DATA_TOPIC_DIAGNOSTICS);
 assert(refreshes[UI_PAGE_PURE]==3);
 puts("PASS quick device versions: PURE and Main subscriptions, current-owner commit, same-frame merge, hidden/uncreated host retention and irrelevant-topic isolation");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-quick-data-') as temporary:
    work = Path(temporary)
    (work / 'test.c').write_text(code)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-g',
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-no-pie',
                    '-I' + str(root), str(work / 'test.c'),
                    str(root / 'un260/lv_core/ui_frame_commit.c'), '-o', str(work / 'test')], check=True)
    subprocess.run([str(work / 'test')], check=True)
