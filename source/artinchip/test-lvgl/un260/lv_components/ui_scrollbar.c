#include "ui_scrollbar.h"
#include <math.h>
#include <string.h>

typedef struct { float viewport, content, offset; } metrics_t;
static lv_theme_t scrollbar_theme;

bool ui_scrollbar_measure(int length,float viewport,float content,float position,
                          ui_scrollbar_geometry_t *out)
{
    if(!out || length<10 || !isfinite(viewport) || !isfinite(content) ||
       !isfinite(position) || viewport<=0 || content<=viewport)return false;
    int arrow=LV_MIN(5,LV_MAX(1,length/8));
    int gap=LV_MIN(4,LV_MAX(1,length/12));
    int start=arrow+gap,track=length-2*start;
    float max=content-viewport,offset=fmaxf(0,fminf(max,position));
    int normal=LV_MIN(track,LV_MAX(LV_MIN(18,track),(int)(track*viewport/content)));
    int thumb=LV_MAX(LV_MIN(6,normal),(int)(normal/(1+fabsf(position-offset)/28)));
    int pos=start+(int)((track-normal)*offset/max);
    if(position>max)pos=start+track-thumb;
    *out=(ui_scrollbar_geometry_t){arrow,gap,pos,thumb};return true;
}

/* Both arrows, gaps and thumb fit inside the supplied rail. No child objects,
 * animations, timers or allocations occur during drawing or scrolling. */
static void paint(lv_draw_ctx_t *ctx, lv_area_t rail, const metrics_t *m)
{
    if(m->viewport<=0 || m->content<=m->viewport) return;
    bool vertical=lv_area_get_height(&rail)>=lv_area_get_width(&rail);
    int length=vertical?lv_area_get_height(&rail):lv_area_get_width(&rail);
    int cross=vertical?lv_area_get_width(&rail):lv_area_get_height(&rail);
    ui_scrollbar_geometry_t g;
    if(cross<2 || !ui_scrollbar_measure(length,m->viewport,m->content,m->offset,&g))return;
    int arrow=g.arrow,thumb=g.thumb_length,pos=g.thumb_start;
    lv_color_t color=lv_color_hex(0x91A5B2);
    lv_draw_line_dsc_t line;lv_draw_line_dsc_init(&line);
    line.color=color;line.width=1;line.opa=LV_OPA_COVER;
    int center=vertical?(rail.x1+rail.x2)/2:(rail.y1+rail.y2)/2;
    /* Filled triangles drawn as short horizontal/vertical strokes; no font. */
    for(int end=0;end<2;end++) for(int i=0;i<arrow;i++) {
        int axis=end?length-1-i:i;
        int half=arrow>1?i*(cross-1)/(2*(arrow-1)):0;
        lv_point_t a,b;
        if(vertical){a=(lv_point_t){center-half,rail.y1+axis};b=(lv_point_t){center+half,rail.y1+axis};}
        else {a=(lv_point_t){rail.x1+axis,center-half};b=(lv_point_t){rail.x1+axis,center+half};}
        lv_draw_line(ctx,&line,&a,&b);
    }
    lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);
    d.bg_color=color;d.bg_opa=LV_OPA_COVER;d.radius=LV_RADIUS_CIRCLE;
    lv_area_t a=rail;
    int width=LV_MIN(4,cross);
    if(vertical){a.x1=center-width/2;a.x2=a.x1+width-1;a.y1+=pos;a.y2=a.y1+thumb-1;}
    else {a.y1=center-width/2;a.y2=a.y1+width-1;a.x1+=pos;a.x2=a.x1+thumb-1;}
    lv_draw_rect(ctx,&d,&a);
}

static void native_event(lv_event_t *e)
{
    if(lv_event_get_target(e)!=lv_event_get_current_target(e)) return;
    lv_obj_t *obj=lv_event_get_target(e);
    lv_event_code_t code=lv_event_get_code(e);
    if(code==LV_EVENT_DRAW_PART_BEGIN) {
        lv_obj_draw_part_dsc_t *d=lv_event_get_draw_part_dsc(e);
        if(d && d->class_p==&lv_obj_class && d->part==LV_PART_SCROLLBAR && d->rect_dsc) {
            d->rect_dsc->bg_opa=d->rect_dsc->border_opa=d->rect_dsc->shadow_opa=LV_OPA_TRANSP;
        }
        return;
    }
    if(code!=LV_EVENT_DRAW_POST_END || !lv_obj_has_flag(obj,LV_OBJ_FLAG_SCROLLABLE)) return;
    lv_scrollbar_mode_t mode=lv_obj_get_scrollbar_mode(obj);
    if(mode==LV_SCROLLBAR_MODE_OFF || (mode==LV_SCROLLBAR_MODE_ACTIVE && !lv_obj_is_scrolling(obj))) return;
    lv_area_t a;lv_obj_get_coords(obj,&a);
    int height=lv_obj_get_height(obj),width=lv_obj_get_width(obj);
    float y=lv_obj_get_scroll_top(obj),x=lv_obj_get_scroll_left(obj);
    metrics_t vertical={height,height+y+lv_obj_get_scroll_bottom(obj),y};
    metrics_t horizontal={width,width+x+lv_obj_get_scroll_right(obj),x};
    lv_draw_ctx_t *ctx=lv_event_get_draw_ctx(e);
    if(vertical.content>vertical.viewport){lv_area_t r={a.x2-9,a.y1+2,a.x2-2,a.y2-2};paint(ctx,r,&vertical);}
    if(horizontal.content>horizontal.viewport){lv_area_t r={a.x1+2,a.y2-9,a.x2-2-(vertical.content>vertical.viewport?10:0),a.y2-2};paint(ctx,r,&horizontal);}
}

static void theme_apply(lv_theme_t *theme, lv_obj_t *obj)
{
    (void)theme;
    /* Theme runs BEFORE constructors assign SCROLLABLE. Select by class here;
     * the draw callback checks the final flags. Theme reapply is idempotent. */
    if(!lv_obj_check_type(obj,&lv_label_class) && !lv_obj_check_type(obj,&lv_img_class)) {
        while(lv_obj_remove_event_cb(obj,native_event)) {}
        if(!lv_obj_add_event_cb(obj,native_event,LV_EVENT_DRAW_POST_END,NULL))return;
        if(!lv_obj_add_event_cb(obj,native_event,LV_EVENT_DRAW_PART_BEGIN,NULL))
            lv_obj_remove_event_cb(obj,native_event); /* Keep native fallback on OOM. */
    }
}
void ui_scrollbar_init(lv_disp_t *display)
{
    if(!display || lv_disp_get_theme(display)==&scrollbar_theme) return;
    lv_theme_t *parent=lv_disp_get_theme(display);
    if(parent)scrollbar_theme=*parent;else memset(&scrollbar_theme,0,sizeof(scrollbar_theme));
    lv_theme_set_parent(&scrollbar_theme,parent);
    lv_theme_set_apply_cb(&scrollbar_theme,theme_apply);
    lv_disp_set_theme(display,&scrollbar_theme);
}
static void custom_event(lv_event_t *e)
{
    if(lv_event_get_target(e)!=lv_event_get_current_target(e))return;
    metrics_t *m=lv_event_get_user_data(e);
    if(lv_event_get_code(e)==LV_EVENT_DELETE){lv_mem_free(m);return;}
    if(lv_event_get_code(e)==LV_EVENT_DRAW_MAIN){lv_area_t a;lv_obj_get_coords(lv_event_get_target(e),&a);paint(lv_event_get_draw_ctx(e),a,m);}
}
lv_obj_t *ui_scrollbar_create(lv_obj_t *parent,int x,int y,int w,int h)
{
    metrics_t *m=lv_mem_alloc(sizeof(*m));if(!m)return NULL;memset(m,0,sizeof(*m));
    lv_obj_t *obj=lv_obj_create(parent);if(!obj){lv_mem_free(m);return NULL;}
    lv_obj_remove_style_all(obj);lv_obj_set_pos(obj,x,y);lv_obj_set_size(obj,w,h);
    lv_obj_clear_flag(obj,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(obj,LV_OBJ_FLAG_IGNORE_LAYOUT|LV_OBJ_FLAG_FLOATING);
    lv_obj_set_user_data(obj,m);
    if(!lv_obj_add_event_cb(obj,custom_event,LV_EVENT_ALL,m)){lv_obj_del(obj);lv_mem_free(m);return NULL;}
    return obj;
}
void ui_scrollbar_update(lv_obj_t *obj,float viewport,float content,float offset)
{
    if(!obj)return;
    metrics_t *m=lv_obj_get_user_data(obj);
    if(m){
        if(m->viewport==viewport&&m->content==content&&m->offset==offset)return;
        *m=(metrics_t){viewport,content,offset};lv_obj_invalidate(obj);return;
    }
}
