"""Production scaled-raster geometry; verify it excludes invalidation padding."""
from pathlib import Path
import tempfile, subprocess
s=(Path(__file__).resolve().parents[1]/'lv_ge2d.c').read_text()
start=s.index('static struct mpp_frame *ge_prepare_scaled(',s.index('static struct mpp_frame *ge_scale_cache_get('))
end=s.index('static const uint8_t *ge_scaled_pixels',start)
stub=r'''
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#define LV_IMG_ZOOM_NONE 256
typedef struct {int x,y;}lv_point_t;
typedef struct {int x1,y1,x2,y2;}lv_area_t;
typedef struct {int angle,zoom;lv_point_t pivot;}lv_draw_img_dsc_t;
struct mpp_frame {struct {struct {int width,height;}size;}buf;};
static int width,height;
static struct mpp_frame*ge_scale_cache_get(struct mpp_frame*f,int w,int h){width=w;height=h;return f;}
static void lv_point_transform(lv_point_t*p,int a,int z,const lv_point_t*c){(void)a;p->x=((p->x-c->x)*z>>8)+c->x;p->y=((p->y-c->y)*z>>8)+c->y;}
'''
test=r'''
int main(void){struct mpp_frame f={.buf.size={1280,400}};lv_area_t a={30,76,1309,475},b;lv_draw_img_dsc_t d={.zoom=104};
assert(ge_prepare_scaled(&f,&d,&a,&b)==&f);assert(width==520&&height==162);assert(b.x1==30&&b.y1==76&&b.x2==549&&b.y2==237);
d.zoom=44;assert(ge_prepare_scaled(&f,&d,&a,&b));assert(width==220&&height==68);assert(b.x1==30&&b.y1==76);
d.pivot=(lv_point_t){640,200};assert(ge_prepare_scaled(&f,&d,&a,&b));assert(b.x1==560&&b.y1==241);
d.zoom=256;assert(!ge_prepare_scaled(&f,&d,&a,&b));d.zoom=31;assert(!ge_prepare_scaled(&f,&d,&a,&b));d.zoom=104;d.angle=900;assert(!ge_prepare_scaled(&f,&d,&a,&b));return 0;}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.c').write_text(stub+s[start:end]+test)
 subprocess.run(['gcc','-std=c11','-Wall','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS preview/thumbnail exact raster geometry, pivot and unsupported-scale fallback')
