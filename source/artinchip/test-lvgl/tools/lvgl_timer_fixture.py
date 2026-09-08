"""Compile the board's actual LVGL 8.3.2 list/timer/async implementations.

Only include directives are removed: their types and platform hooks are in
host_support.h. Sources were read from the production SDK without modification.
The allocator quarantines freed blocks and rejects double frees deterministically.
"""
from pathlib import Path
import re

FIXTURE = Path(__file__).resolve().parent / "fixtures/lvgl_8_3_2"


def code():
    result = (FIXTURE / "host_support.h").read_text(encoding="utf-8")
    for name in ("lv_ll.c", "lv_timer.c", "lv_async.c"):
        source = (FIXTURE / name).read_text(encoding="utf-8")
        result += "\n" + re.sub(r'^#include[^\n]*', '', source, flags=re.M)
    result += r'''
static void test_lvgl_reset(void) {
    while(lv_timer_get_next(NULL)) lv_timer_del(lv_timer_get_next(NULL));
    for(unsigned n=0;n<allocation_count;n++) {
        assert(allocations[n].freed);
        free(allocations[n].pointer);
    }
    allocation_count=double_frees=0;
    fail_timer=fail_async=expect_double_free=false;
    _lv_timer_core_init();
}
'''
    return result
