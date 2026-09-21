#include "settings_catalog.h"
#include "lv_page_manager.h"
#include <string.h>
static const settings_node_t nodes[]={
 {"device",NULL,"Device","Display, preferences and printed reports.","Settings",SETTINGS_CATEGORY,0},
 {"counting",NULL,"Counting","Detection and handling preferences.","Layers",SETTINGS_CATEGORY,0},
 {"maintenance",NULL,"Maintenance","Calibration, diagnostics and device tests.","Wrench",SETTINGS_CATEGORY,0},
 {"data",NULL,"Data","Collection and software updates.","Database",SETTINGS_CATEGORY,0},
 {"about",NULL,"About & security","Device information and access.","ShieldCheck",SETTINGS_CATEGORY,0},
 {"language","device","Language","Interface language","Languages",SETTINGS_DETAIL,UI_PAGE_LANGUAGE_SETTING,"preferences"},
 {"time","device","Date & time","Device clock","Clock",SETTINGS_DETAIL,UI_PAGE_TIMESET,"preferences"},
 {"brightness","device","Brightness","Screen backlight","Sun",SETTINGS_DETAIL,UI_PAGE_BRIGHTNESS_SETTING,"preferences"},
 {"print","device","Print settings","Report content and output","EntryReceipt",SETTINGS_DETAIL,UI_PAGE_PRINT_SETTING,"output"},
 {"standby","device","Standby","When the device is idle","EntryStandby",SETTINGS_DETAIL,UI_PAGE_STANDBY_SETTING,"preferences"},
 {"reject","counting","Reject pocket","Maximum rejected notes","EntryReject",SETTINGS_DETAIL,UI_PAGE_REJECT_POCKET_SETTING,"handling"},
 {"cfd","counting","Counterfeit detection","Currency detection profiles","ShieldCheck",SETTINGS_DETAIL,UI_PAGE_CFD_LEVEL_SETTING,"detection"},
 {"double","counting","Double-note detection","Overlapping-note sensitivity","EntryDoubleNote",SETTINGS_DETAIL,UI_PAGE_DOUBLE_NOTE_SETTING,"detection"},
 {"serial","counting","Serial numbers","Recognition comparison level","EntrySerial",SETTINGS_DETAIL,UI_PAGE_SERIAL_NUMBER_SETTING,"detection"},
 {"calibration","maintenance","Calibration","CIS and white balance","EntryCalibration",SETTINGS_DIRECTORY,0,"calibration"},
 {"cis","calibration","CIS calibration","Image sensor calibration","EntryCis",SETTINGS_DETAIL,UI_PAGE_CIS_CALIB,"calibration"},
 {"whiteBalance","calibration","White balance","Color balance calibration","EntryBalance",SETTINGS_DETAIL,UI_PAGE_CIS_CALIB,"calibration"},
 {"motor","maintenance","Motor test","Individual motor controls","EntryMotor",SETTINGS_DETAIL,UI_PAGE_MOTOR_TEST,"hardware"},
 {"sensors","maintenance","Sensors","Live channel voltages","EntrySensor",SETTINGS_DETAIL,UI_PAGE_SENSOR,"hardware"},
 {"flap","maintenance","Flap position","Upper or lower note path","EntryFlap",SETTINGS_DETAIL,UI_PAGE_FLAP_SETTING,"hardware"},
 {"image","maintenance","Image capture","Inspect sensor images","EntryImage",SETTINGS_DETAIL,UI_PAGE_IMAGE_GET,"inspection"},
 {"wave","maintenance","Waveform","Magnetic and UV signals","EntryWave",SETTINGS_DETAIL,UI_PAGE_WAVE_GET,"inspection"},
 {"aging","maintenance","Aging test","Continuous hardware test","EntryAging",SETTINGS_DETAIL,UI_PAGE_AGING_SETTING,"tests"},
 {"display","maintenance","Color calibration","Display reference and adjustment","EntryDisplay",SETTINGS_DETAIL,UI_PAGE_DISPLAY_TEST,"tests"},
 {"collection","data","Data collection","All data or error reports","EntryCollect",SETTINGS_DETAIL,UI_PAGE_SETTING,"collection"},
 {"upgrade","data","Upgrade","Controller, image board and UI","EntryUpgrade",SETTINGS_DIRECTORY,0,"updates"},
 {"mainUpdate","upgrade","Controller","Main-board software","EntryBoard",SETTINGS_DETAIL,UI_PAGE_MAIN_UPGRADE,"updates"},
 {"imageUpdate","upgrade","Image board","Image processing software","EntryImageBoard",SETTINGS_DETAIL,UI_PAGE_IMAGE_UPGRADE,"updates"},
 {"uiUpdate","upgrade","UI","Display software package","EntryUiUpdate",SETTINGS_DETAIL,UI_PAGE_UI_UPGRADE,"updates"},
 {"versions","about","Versions","Applications, bootloaders and FPGA","EntryVersion",SETTINGS_DETAIL,UI_PAGE_SETTING,"information"},
 {"password","about","Change password","Settings access PIN","EntryPassword",SETTINGS_DETAIL,UI_PAGE_PASSWORD_CHANGE,"security"},
 {"debug","about","Debug",NULL,"EntryDebug",SETTINGS_DETAIL,UI_PAGE_DEBUG,"service"},
 {"reset","about","Factory reset","Restore device defaults","EntryReset",SETTINGS_DETAIL,UI_PAGE_FACTORY_SETTING,"security"},
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
