#include "ui_main_layout.h"
#include <string.h>

static void order_default(uint8_t *order, unsigned count)
{ for (unsigned i=0; i<count; ++i) order[i]=(uint8_t)i; }
static void order_normalize(uint8_t *order, unsigned count)
{
    unsigned seen=0;
    for (unsigned i=0; i<count; ++i) {
        if (order[i]>=count || (seen & (1U<<order[i]))) {
            order_default(order,count); return;
        }
        seen |= 1U<<order[i];
    }
}
void ui_main_layout_default(ui_main_layout_t *layout)
{
    memset(layout,0,sizeof(*layout));
    order_default(layout->left,4); order_default(layout->right,3);
}
void ui_main_layout_normalize(ui_main_layout_t *layout)
{
    order_normalize(layout->left,4); order_normalize(layout->right,3);
    if (layout->mirrored>1) layout->mirrored=0;
    if (layout->footer_swapped>1) layout->footer_swapped=0;
}
bool ui_main_layout_swap(ui_main_layout_t *layout, unsigned group,
                         unsigned source, unsigned target)
{
    unsigned count=group==0?4:group==1?3:group==2 || group==3?2:0;
    if (source>=count || target>=count || source==target) return false;
    if (group==2) layout->mirrored=!layout->mirrored;
    else if (group==3) layout->footer_swapped=!layout->footer_swapped;
    else {
        uint8_t *order=group==0?layout->left:layout->right;
        uint8_t old=order[source];order[source]=order[target];order[target]=old;
    }
    return true;
}
bool ui_main_layout_equal(const ui_main_layout_t *a, const ui_main_layout_t *b)
{
    return memcmp(a->left,b->left,4)==0 && memcmp(a->right,b->right,3)==0 &&
           a->mirrored==b->mirrored && a->footer_swapped==b->footer_swapped;
}
