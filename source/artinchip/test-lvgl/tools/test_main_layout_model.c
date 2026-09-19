#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "un260/lv_system/ui_main_layout.h"
#include "un260/lv_system/ui_state_store.h"

static void permutations(ui_main_layout_t layout,unsigned group,unsigned slot,unsigned *count)
{
    unsigned size=group==0?4:3;
    if (slot==size) {
        ui_main_layout_t normalized=layout;ui_main_layout_normalize(&normalized);
        assert(ui_main_layout_equal(&layout,&normalized));++*count;return;
    }
    for (unsigned i=slot;i<size;++i) {
        ui_main_layout_t next=layout;
        if (i!=slot) assert(ui_main_layout_swap(&next,group,slot,i));
        permutations(next,group,slot+1,count);
    }
}
int main(void)
{
    ui_main_layout_t a,b;ui_main_layout_default(&a);
    unsigned left=0,right=0;permutations(a,0,0,&left);permutations(a,1,0,&right);
    assert(left==24 && right==6);
    for(unsigned g=0;g<4;++g) for(unsigned i=0;i<(g==0?4:g==1?3:2);++i)
        for(unsigned j=0;j<(g==0?4:g==1?3:2);++j) {
            b=a;
            assert(ui_main_layout_swap(&b,g,i,j)==(i!=j));
            assert(ui_main_layout_swap(&b,g,i,j)==(i!=j));
            assert(ui_main_layout_equal(&a,&b));
        }
    assert(!ui_main_layout_swap(&a,4,0,1));assert(!ui_main_layout_swap(&a,0,0,4));
    assert(!ui_main_layout_swap(&a,3,0,2));
    b=a;b.left[0]=3;b.right[1]=255;b.mirrored=255;b.footer_swapped=255;ui_main_layout_normalize(&b);
    assert(ui_main_layout_equal(&a,&b));
    b=a;ui_main_layout_swap(&b,1,0,2);b.left[0]=255;ui_main_layout_normalize(&b);
    assert(b.right[0]==2 && b.left[0]==0); /* Corruption repaired per group. */
    b.footer_swapped=1;
    ui_persist_state_t state={0},loaded={0};state.page01.layout=b;
    state.page01.detail_section=2;state.page06.reserved06_enable=1;
    assert(ui_state_store_save(&state));assert(ui_state_store_load(&loaded));
    assert(ui_main_layout_equal(&b,&loaded.page01.layout) && loaded.page01.detail_section==2);
    FILE *file=fopen(UI_STATE_STORE_PATH,"w");assert(file);
    fprintf(file,"magic=%u\nversion=1\np01_detail_section=1\n",UI_STATE_STORE_MAGIC);fclose(file);
    memset(&loaded,0,sizeof(loaded));assert(ui_state_store_load(&loaded));
    assert(ui_main_layout_equal(&a,&loaded.page01.layout) && loaded.page01.detail_section==1);
    file=fopen(UI_STATE_STORE_PATH,"a");assert(file);fputs("p01_layout=0,0,2,3,2,1,0,1\n",file);fclose(file);
    assert(ui_state_store_load(&loaded));assert(loaded.page01.layout.left[1]==1);
    assert(loaded.page01.layout.right[0]==2 && loaded.page01.layout.mirrored==1);
    assert(loaded.page01.layout.footer_swapped==0); /* Old 8-value layout survives. */
    file=fopen(UI_STATE_STORE_PATH,"a");assert(file);fputs("p01_layout_footer=1\n",file);fclose(file);
    assert(ui_state_store_load(&loaded));assert(loaded.page01.layout.footer_swapped==1);
    assert(loaded.page01.layout.right[0]==2 && loaded.page01.layout.mirrored==1);
    file=fopen(UI_STATE_STORE_PATH,"a");assert(file);fputs("p01_layout_footer=2\n",file);fclose(file);
    assert(ui_state_store_load(&loaded));assert(loaded.page01.layout.footer_swapped==0);
    assert(loaded.page01.layout.right[0]==2 && loaded.page01.layout.mirrored==1);
    file=fopen(UI_STATE_STORE_PATH,"a");assert(file);fputs("p01_layout_footer=1bad\n",file);fclose(file);
    assert(ui_state_store_load(&loaded));assert(loaded.page01.layout.footer_swapped==0);
    puts("PASS layout permutations, bounds, per-group recovery, atomic save/load and old config migration");
}
