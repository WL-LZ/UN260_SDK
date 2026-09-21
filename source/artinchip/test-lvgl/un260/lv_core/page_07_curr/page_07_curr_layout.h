#ifndef PAGE_07_CURR_LAYOUT_H
#define PAGE_07_CURR_LAYOUT_H

/* Build-time rollback to the original all-live card tree. */
#ifndef PAGE07_CURR_SPLIT_FACE_CACHE
#define PAGE07_CURR_SPLIT_FACE_CACHE 1
#endif

/* Currency-private geometry and palette. Shared only by this page's view
 * builders and card renderer, not application-wide theme state. */
#define CURR_SEL_W               288
#define CURR_SEL_H               400
#define CURR_VIEW_X              288
#define CURR_VIEW_Y              0
#define CURR_VIEW_W              992
#define CURR_VIEW_H              400

#define CURR_CARD_W              200
#define CURR_CARD_H              265
#define CURR_CARD_GAP            28
#define CURR_CARD_STRIDE         (CURR_CARD_W + CURR_CARD_GAP)
#define CURR_CARD_FOCUS_X        344 /* x=632 on the full display */
#define CURR_CARD_FIRST_X        (CURR_CARD_FOCUS_X - CURR_CARD_W / 2)
#define CURR_CARD_Y              68
#define CURR_CARD_FOCUS_LIFT     8
#define CURR_CARD_RADIUS         21
#define CURR_CARD_BG             0xFFFFFF
#define CURR_CARD_NORMAL_BORDER  0xCBD5DE
#define CURR_CARD_FOCUS_BORDER   0x6499CA
/* Move only the actual card footprint, not the blank space above it. */
#define CURR_CARD_STRIP_Y        (CURR_CARD_Y - CURR_CARD_FOCUS_LIFT)
#define CURR_CARD_LOCAL_Y        CURR_CARD_FOCUS_LIFT
#define CURR_CARD_SCROLL_H       (CURR_CARD_H + CURR_CARD_FOCUS_LIFT + 1)

#define CURR_TRACK_Y             365
#define CURR_TRACK_H             4
#define CURR_TRACK_W             176
#define CURR_TRACK_X             (CURR_VIEW_W - CURR_TRACK_W - 76)
#define CURR_TRACK_MIN_THUMB     28

#define CURR_LEFT_BG_COLOR       0xEDF0F4
#define CURR_RIGHT_BG_COLOR      0xF4F5F7
#define CURR_TEXT_SEL            0xFC4000
#define CURR_TEXT_UNSEL          0xBEBFC0
#define CURR_IMG_UNSEL           0xCDCED0
#define CURR_TRACK_BG            0xE4E7EB
#define CURR_TRACK_FG            0x8D959F

#define CURR_BTN_W               70
#define CURR_BTN_H               36
#define CURR_BTN_Y               358
#define CURR_VIEW_BTN_X          18
#define CURR_FAV_BTN_X           113
#define CURR_BACK_BTN_X          207


#define CURR_LEFT_IMG_ALIGN_Y    100
#define CURR_LEFT_IMG_ALIGN_X    -46
#define CURR_LEFT_CODE_X         39
#define CURR_LEFT_CODE_Y         214
#define CURR_LEFT_CODE_DECOR_X   128
#define CURR_LEFT_CODE_DECOR_Y   228
#define CURR_LEFT_NO_X           39
#define CURR_LEFT_NO_Y           300

/* VIEW is an independent full-width workspace, not the Card right pane. */
#define CURR_GRID_COLS           8
#define CURR_GRID_CELL_W         152
#define CURR_GRID_CELL_H         66
#define CURR_GRID_ITEM_W         144
#define CURR_GRID_START_X        10
#define CURR_GRID_START_Y        6
#define CURR_GRID_ROW_STEP       72
#define CURR_GRID_VIEWPORT_X     24
#define CURR_GRID_VIEWPORT_Y     78
#define CURR_GRID_VIEWPORT_W     1232
#define CURR_GRID_VIEWPORT_H     294
#define CURR_GRID_FLAG_W         34
#define CURR_GRID_FAV_X          104
#define CURR_GRID_FAV_Y          3

#define CURR_FAV_BTN_IN_CARD_X   139
#define CURR_FAV_BTN_IN_CARD_Y   13
#define CURR_FAV_BTN_IN_CARD_W   49
#define CURR_FAV_BTN_IN_CARD_H   49

#define CURR_FLAG_TARGET_W       82
#define CURR_LEFT_FLAG_TARGET_W  100
#define CURR_FLAG_Y_IN_CARD      19

#endif
