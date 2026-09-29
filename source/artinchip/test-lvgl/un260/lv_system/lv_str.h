#ifndef LV_STR_H
#define LV_STR_H
#include "ui_lang.h"
/* Compatibility for legacy resource label metadata. New code uses ui_tr/UI_N_. */
const char *get_str_by_name(const char *name);
#endif
