#include "settings_catalog.h"
#include "lv_page_manager.h"
#include <string.h>
static const settings_node_t nodes[]={
 {"device",NULL,"Device","Display, preferences and printed reports.","Settings",SETTINGS_CATEGORY,0},
 {"counting",NULL,"Counting","Detection and handling preferences.","Layers",SETTINGS_CATEGORY,0},
 {"maintenance",NULL,"Maintenance","Calibration, diagnostics and device tests.","Wrench",SETTINGS_CATEGORY,0},
 {"data",NULL,"Data","Collection and software updates.","Database",SETTINGS_CATEGORY,0},
 {"about",NULL,"About & security","Device information and access.","ShieldCheck",SETTINGS_CATEGORY,0},
 {"language","device","Language","Interface language",NULL,SETTINGS_DETAIL,UI_PAGE_LANGUAGE_SETTING},
 {"time","device","Date & time","Device clock",NULL,SETTINGS_DETAIL,UI_PAGE_TIMESET},
 {"brightness","device","Brightness","Screen backlight",NULL,SETTINGS_DETAIL,UI_PAGE_BRIGHTNESS_SETTING},
 {"print","device","Print settings","Report content and output",NULL,SETTINGS_DETAIL,UI_PAGE_PRINT_SETTING},
 {"standby","device","Standby","When the device is idle",NULL,SETTINGS_DETAIL,UI_PAGE_STANDBY_SETTING},
 {"reject","counting","Reject pocket","Maximum rejected notes",NULL,SETTINGS_DETAIL,UI_PAGE_REJECT_POCKET_SETTING},
 {"cfd","counting","Counterfeit detection","Currency detection profiles",NULL,SETTINGS_DETAIL,UI_PAGE_CFD_LEVEL_SETTING},
 {"double","counting","Double-note detection","Overlapping-note sensitivity",NULL,SETTINGS_DETAIL,UI_PAGE_DOUBLE_NOTE_SETTING},
 {"serial","counting","Serial numbers","Recognition comparison level",NULL,SETTINGS_DETAIL,UI_PAGE_SERIAL_NUMBER_SETTING},
 {"calibration","maintenance","Calibration","CIS and white balance",NULL,SETTINGS_DIRECTORY,0},
 {"cis","calibration","CIS calibration","Image sensor calibration","Layers",SETTINGS_DETAIL,UI_PAGE_CIS_CALIB},
 {"whiteBalance","calibration","White balance","Color balance calibration","Sun",SETTINGS_DETAIL,UI_PAGE_CIS_CALIB},
 {"motor","maintenance","Motor test","Individual motor controls",NULL,SETTINGS_DETAIL,UI_PAGE_MOTOR_TEST},
 {"sensors","maintenance","Sensors","Live channel voltages",NULL,SETTINGS_DETAIL,UI_PAGE_SENSOR},
 {"flap","maintenance","Flap position","Upper or lower note path",NULL,SETTINGS_DETAIL,UI_PAGE_FLAP_SETTING},
 {"image","maintenance","Image capture","Inspect sensor images",NULL,SETTINGS_DETAIL,UI_PAGE_IMAGE_GET},
 {"wave","maintenance","Waveform","Magnetic and UV signals",NULL,SETTINGS_DETAIL,UI_PAGE_WAVE_GET},
 {"aging","maintenance","Aging test","Continuous hardware test",NULL,SETTINGS_DETAIL,UI_PAGE_AGING_SETTING},
 {"display","maintenance","Display test","Color and grayscale checks",NULL,SETTINGS_DETAIL,UI_PAGE_DISPLAY_TEST},
 {"collection","data","Data collection","All data or error reports",NULL,SETTINGS_DETAIL,UI_PAGE_SETTING},
 {"upgrade","data","Upgrade","Controller, image board and UI",NULL,SETTINGS_DIRECTORY,0},
 {"mainUpdate","upgrade","Controller","Main-board software",NULL,SETTINGS_DETAIL,UI_PAGE_MAIN_UPGRADE},
 {"imageUpdate","upgrade","Image board","Image processing software",NULL,SETTINGS_DETAIL,UI_PAGE_IMAGE_UPGRADE},
 {"uiUpdate","upgrade","UI","Display software package",NULL,SETTINGS_DETAIL,UI_PAGE_UI_UPGRADE},
 {"versions","about","Versions","Applications, bootloaders and FPGA",NULL,SETTINGS_DETAIL,UI_PAGE_SETTING},
 {"password","about","Change password","Settings access PIN",NULL,SETTINGS_DETAIL,UI_PAGE_PASSWORD_CHANGE},
 {"debug","about","Debug",NULL,NULL,SETTINGS_DETAIL,UI_PAGE_DEBUG},
 {"reset","about","Factory reset","Restore device defaults",NULL,SETTINGS_DETAIL,UI_PAGE_FACTORY_SETTING},
};
const settings_node_t *settings_catalog(size_t *count){if(count)*count=sizeof(nodes)/sizeof(nodes[0]);return nodes;}
const settings_node_t *settings_catalog_find(const settings_node_t *items,size_t count,const char *id)
{if(!items||!id)return NULL;for(size_t i=0;i<count;i++)if(items[i].id&&!strcmp(items[i].id,id))return items+i;return NULL;}
bool settings_catalog_validate(const settings_node_t *items,size_t count)
{
 if(!items||!count)return false;
 for(size_t i=0;i<count;i++){
  const settings_node_t *n=items+i,*p=n;unsigned depth=n->kind==SETTINGS_DETAIL?0:1;
  if(!n->id||!n->id[0]||!n->title||!n->title[0]||(unsigned)n->kind>SETTINGS_DETAIL)return false;
  for(size_t j=0;j<i;j++)if(!strcmp(n->id,items[j].id))return false;
  if(n->kind==SETTINGS_CATEGORY){if(n->parent)return false;continue;}
  for(size_t hops=0;;hops++){
   if(hops>=count||!p->parent)return false;
   p=settings_catalog_find(items,count,p->parent);
   if(!p||p->kind==SETTINGS_DETAIL||++depth>3)return false;
   if(p->kind==SETTINGS_CATEGORY)break;
  }
 }
 return true;
}
