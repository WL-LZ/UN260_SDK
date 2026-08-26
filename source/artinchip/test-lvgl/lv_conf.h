#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_USE_PERF_MONITOR         0
#define LV_COLOR_DEPTH              32
#define LV_IMG_CACHE_DEF_SIZE 12
#define LV_USE_MEM_MONITOR 0
#define LV_INDEV_DEF_READ_PERIOD 10
#define LV_DISP_DEF_REFR_PERIOD 10

#define LV_TICK_CUSTOM 1
#if LV_TICK_CUSTOM
    #define LV_TICK_CUSTOM_INCLUDE <stdint.h>
    #define LV_TICK_CUSTOM_SYS_TIME_EXPR (custom_tick_get()) /*system time in ms*/
#endif   /*LV_TICK_CUSTOM*/

#define LV_USE_FS_POSIX 1
#if LV_USE_FS_POSIX
    #define LV_FS_POSIX_LETTER 'L'
    #define LV_FS_POSIX_PATH ""
    #define LV_FS_POSIX_CACHE_SIZE 0
#endif

#define LV_MEM_CUSTOM 1
#if LV_MEM_CUSTOM == 1
    #define LV_MEM_CUSTOM_INCLUDE <stdlib.h>   /*Header for the dynamic memory function*/
    #define LV_MEM_CUSTOM_ALLOC   malloc
    #define LV_MEM_CUSTOM_FREE    free
    #define LV_MEM_CUSTOM_REALLOC realloc
#endif     /*LV_MEM_CUSTOM*/
#endif

#define LV_SPRINTF_CUSTOM 1
#if LV_SPRINTF_CUSTOM
    #define LV_SPRINTF_INCLUDE <stdio.h>
    #define lv_snprintf  snprintf
    #define lv_vsnprintf vsnprintf
	#define LV_SPRINTF_USE_FLOAT 0
#endif  /*LV_SPRINTF_CUSTOM*/
#define LV_FONT_MONTSERRAT_8  0
#define LV_FONT_MONTSERRAT_10 1
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_22 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_26 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_30 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 1
#define LV_FONT_MONTSERRAT_42 1
#define LV_FONT_MONTSERRAT_44 1
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 1

#define LV_FONT_CUSTOM_DECLARE  \
LV_FONT_DECLARE(lv_font_manrope_bold_10); \
LV_FONT_DECLARE(lv_font_manrope_bold_12); \
LV_FONT_DECLARE(lv_font_manrope_bold_14); \
LV_FONT_DECLARE(lv_font_manrope_bold_16); \
LV_FONT_DECLARE(lv_font_manrope_bold_18); \
LV_FONT_DECLARE(lv_font_manrope_bold_20); \
LV_FONT_DECLARE(lv_font_manrope_bold_22); \
LV_FONT_DECLARE(lv_font_manrope_bold_24); \
LV_FONT_DECLARE(lv_font_manrope_bold_28); \
LV_FONT_DECLARE(lv_font_manrope_bold_30); \
LV_FONT_DECLARE(lv_font_manrope_bold_32); \
LV_FONT_DECLARE(lv_font_manrope_bold_40); \
LV_FONT_DECLARE(lv_font_manrope_bold_42); \
LV_FONT_DECLARE(lv_font_manrope_bold_44); \
LV_FONT_DECLARE(lv_font_manrope_bold_48); \
LV_FONT_DECLARE(lv_font_manrope_extrabold_10); \
LV_FONT_DECLARE(lv_font_manrope_extrabold_12); \
LV_FONT_DECLARE(lv_font_manrope_extrabold_14); \
LV_FONT_DECLARE(lv_font_manrope_extrabold_16); \
LV_FONT_DECLARE(lv_font_manrope_extrabold_18); \
LV_FONT_DECLARE(lv_font_manrope_extrabold_20); \
LV_FONT_DECLARE(lv_font_manrope_extrabold_22); \
LV_FONT_DECLARE(lv_font_manrope_extrabold_24); \
LV_FONT_DECLARE(lv_font_manrope_extrabold_28); \
LV_FONT_DECLARE(lv_font_manrope_extrabold_30); \
LV_FONT_DECLARE(lv_font_manrope_extrabold_32); \
LV_FONT_DECLARE(lv_font_manrope_extrabold_40); \
LV_FONT_DECLARE(lv_font_manrope_extrabold_42); \
LV_FONT_DECLARE(lv_font_manrope_extrabold_44); \
LV_FONT_DECLARE(lv_font_manrope_extrabold_48); \
LV_FONT_DECLARE(lv_font_instrument_sans_bold_10); \
LV_FONT_DECLARE(lv_font_instrument_sans_bold_12); \
LV_FONT_DECLARE(lv_font_instrument_sans_bold_14); \
LV_FONT_DECLARE(lv_font_instrument_sans_bold_16); \
LV_FONT_DECLARE(lv_font_instrument_sans_bold_18); \
LV_FONT_DECLARE(lv_font_instrument_sans_bold_20); \
LV_FONT_DECLARE(lv_font_instrument_sans_bold_22); \
LV_FONT_DECLARE(lv_font_instrument_sans_bold_24); \
LV_FONT_DECLARE(lv_font_instrument_sans_bold_28); \
LV_FONT_DECLARE(lv_font_instrument_sans_bold_30); \
LV_FONT_DECLARE(lv_font_instrument_sans_bold_32); \
LV_FONT_DECLARE(lv_font_instrument_sans_bold_40); \
LV_FONT_DECLARE(lv_font_instrument_sans_bold_42); \
LV_FONT_DECLARE(lv_font_instrument_sans_bold_44); \
LV_FONT_DECLARE(lv_font_instrument_sans_bold_48); \
LV_FONT_DECLARE(lv_font_instrument_sans_medium_10); \
LV_FONT_DECLARE(lv_font_instrument_sans_medium_12); \
LV_FONT_DECLARE(lv_font_instrument_sans_medium_14); \
LV_FONT_DECLARE(lv_font_instrument_sans_medium_16); \
LV_FONT_DECLARE(lv_font_instrument_sans_medium_18); \
LV_FONT_DECLARE(lv_font_instrument_sans_medium_20); \
LV_FONT_DECLARE(lv_font_instrument_sans_medium_22); \
LV_FONT_DECLARE(lv_font_instrument_sans_medium_24); \
LV_FONT_DECLARE(lv_font_instrument_sans_medium_28); \
LV_FONT_DECLARE(lv_font_instrument_sans_medium_30); \
LV_FONT_DECLARE(lv_font_instrument_sans_medium_32); \
LV_FONT_DECLARE(lv_font_instrument_sans_medium_40); \
LV_FONT_DECLARE(lv_font_instrument_sans_medium_42); \
LV_FONT_DECLARE(lv_font_instrument_sans_medium_44); \
LV_FONT_DECLARE(lv_font_instrument_sans_medium_48); \
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_10); \
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_12); \
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_14); \
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_16); \
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_18); \
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_20); \
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_22); \
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_24); \
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_28); \
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_30); \
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_32); \
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_40); \
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_42); \
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_44); \
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_48);
