#include "un260/lv_system/backlight_service.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    backlight_service_init();
    assert(backlight_service_max()==10);
    assert(backlight_service_level()==5);
    assert(!backlight_service_set(0));
    assert(!backlight_service_set(11));
    assert(backlight_service_set(7));
    assert(backlight_service_level()==7);
    assert(backlight_service_save());
    assert(backlight_service_save());
    assert(backlight_service_set(4));
    backlight_service_init();
    assert(backlight_service_level()==7);
    /* Another process/panel may change the live node behind our cache. */
    char path[320];
    snprintf(path, sizeof(path), "%s/brightness", backlight_service_device());
    FILE *fp = fopen(path, "w");
    assert(fp);
    assert(fprintf(fp, "3\n") > 0);
    assert(fclose(fp) == 0);
    assert(backlight_service_level()==7);
    assert(backlight_service_set(7));
    fp = fopen(path, "r");
    int actual = -1;
    assert(fp && fscanf(fp, "%d", &actual) == 1);
    assert(fclose(fp) == 0 && actual == 7);
    puts("backlight: PASS (safe range, readback, persistence, restore, stale-cache recovery)");
}
