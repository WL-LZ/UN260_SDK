#ifndef GESTURE_GUIDE_H
#define GESTURE_GUIDE_H

#include <stdbool.h>

void gesture_guide_show(void);
void gesture_guide_close(bool animated);
bool gesture_guide_is_open(void);

#endif
