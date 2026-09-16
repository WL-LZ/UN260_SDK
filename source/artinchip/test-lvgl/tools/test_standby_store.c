#include "un260/storage/standby_store.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <png.h>
static bool usb;
bool usb_storage_prepare(void){return usb;}
static void wait_job(void){char msg[160];for(int i=0;i<10000;i++){if(standby_store_poll(msg,sizeof(msg))){puts(msg);return;}usleep(1000);}assert(!"worker timeout");}
int main(int argc,char**argv){(void)argv;if(argc>1){const standby_config_t*p=standby_config();assert(p->version==2&&p->minutes==37&&p->mode==1);assert(p->layout[1][0].color==0x345678&&p->layout[1][0].text_color==0xFFFFFF&&p->layout[1][0].auto_text==1);assert(p->layout[1][0].x==44&&p->layout[1][0].date_y==223);assert(p->layout[1][1].x==840&&p->layout[1][1].scale_percent==75);assert(standby_config_valid(p));assert(standby_store_save(p));wait_job();puts("PASS v1 migration preserves colours, positions, scale, preset selection and writes valid v2");return 0;}standby_config_t c;standby_defaults(&c);assert(standby_config_valid(&c));assert(c.layout[0][0].scale_percent==100);c.layout[0][0].scale_percent=0;assert(standby_config_valid(&c));assert(standby_layout_scale(&c.layout[0][0])==100);c.layout[0][0].scale_percent=74;assert(!standby_config_valid(&c));c.layout[0][0].scale_percent=141;assert(!standby_config_valid(&c));c.layout[0][0].scale_percent=140;standby_layout_clamp(&c.layout[0][0]);assert(standby_config_valid(&c));c.minutes=1441;assert(!standby_config_valid(&c));c.minutes=37;c.mode=1;c.layout[1][0].color=0x123456;assert(standby_store_save(&c));assert(!standby_store_save(&c));wait_job();assert(standby_config()->minutes==37);assert(standby_config()->layout[1][0].color==0x123456);assert(standby_config()->layout[0][0].scale_percent==140);
 assert(standby_store_import());wait_job();assert(!standby_photo_exists(0));usb=true;
 png_image im={0};im.version=PNG_IMAGE_VERSION;im.width=1280;im.height=400;im.format=PNG_FORMAT_RGBA;unsigned char *pixels=malloc(1280*400*4);memset(pixels,255,1280*400*4);assert(png_image_write_to_file(&im,STANDBY_USB_DIRECTORY "/un260_delay_01.png",0,pixels,0,NULL));free(pixels);
 assert(standby_store_import());wait_job();assert(standby_photo_exists(0));assert(!standby_photo_exists(1));assert(standby_store_import());wait_job();assert(!standby_photo_exists(1));
 c=*standby_config();c.layout[0][0].photo=3;c.layout[0][0].scheduled=0;assert(standby_store_save(&c));wait_job();assert(standby_store_delete(3));wait_job();assert(!standby_photo_exists(0));assert(standby_config()->layout[0][0].scheduled==1);
 FILE*f=fopen(STANDBY_USB_DIRECTORY "/un260_delay_02.png","wb");assert(f);fputs("not a PNG",f);fclose(f);assert(standby_store_import());wait_job();assert(standby_photo_exists(0));assert(!standby_photo_exists(1));
 puts("PASS config bounds, custom save, busy exclusion, missing USB, PNG import, duplicate skip, corrupt PNG, delete fallback");return 0;
}
