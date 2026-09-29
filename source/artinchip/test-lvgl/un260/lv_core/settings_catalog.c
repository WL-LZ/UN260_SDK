#include "un260/lv_system/ui_i18n.h"
#include "settings_catalog.h"
#include "lv_page_manager.h"
#include <string.h>
static const settings_node_t nodes[]={
 {"device",NULL,UI_N_("Device"),UI_N_("Display, preferences and printed reports."),"Settings",SETTINGS_CATEGORY,0},
 {"counting",NULL,UI_N_("Counting"),UI_N_("Detection and handling preferences."),"Layers",SETTINGS_CATEGORY,0},
 {"maintenance",NULL,UI_N_("Maintenance"),UI_N_("Calibration, diagnostics and device tests."),"Wrench",SETTINGS_CATEGORY,0},
 {"data",NULL,UI_N_("Data"),UI_N_("Collection and software updates."),"Database",SETTINGS_CATEGORY,0},
 {"about",NULL,UI_N_("About & security"),UI_N_("Device information and access."),"ShieldCheck",SETTINGS_CATEGORY,0},
 {"language","device",UI_N_("Language"),UI_N_("Interface language"),"Languages",SETTINGS_DETAIL,UI_PAGE_LANGUAGE_SETTING,"preferences"},
 {"time","device",UI_N_("Date & time"),UI_N_("Device clock"),"Clock",SETTINGS_DETAIL,UI_PAGE_TIMESET,"preferences"},
 {"brightness","device",UI_N_("Brightness"),UI_N_("Screen backlight"),"Sun",SETTINGS_DETAIL,UI_PAGE_BRIGHTNESS_SETTING,"preferences"},
 {"print","device",UI_N_("Print settings"),UI_N_("Report content and output"),"EntryReceipt",SETTINGS_DETAIL,UI_PAGE_PRINT_SETTING,"output"},
 {"standby","device",UI_N_("Standby"),UI_N_("When the device is idle"),"EntryStandby",SETTINGS_DETAIL,UI_PAGE_STANDBY_SETTING,"preferences"},
 {"reject","counting",UI_N_("Reject pocket"),UI_N_("Maximum rejected notes"),"EntryReject",SETTINGS_DETAIL,UI_PAGE_REJECT_POCKET_SETTING,"handling"},
 {"cfd","counting",UI_N_("Counterfeit detection"),UI_N_("Currency detection profiles"),"ShieldCheck",SETTINGS_DETAIL,UI_PAGE_CFD_LEVEL_SETTING,"detection"},
 {"double","counting",UI_N_("Double-note detection"),UI_N_("Overlapping-note sensitivity"),"EntryDoubleNote",SETTINGS_DETAIL,UI_PAGE_DOUBLE_NOTE_SETTING,"detection"},
 {"serial","counting",UI_N_("Serial numbers"),UI_N_("Recognition comparison level"),"EntrySerial",SETTINGS_DETAIL,UI_PAGE_SERIAL_NUMBER_SETTING,"detection"},
 {"calibration","maintenance",UI_N_("Calibration"),UI_N_("CIS and white balance"),"EntryCalibration",SETTINGS_DIRECTORY,0,"calibration"},
 {"cis","calibration",UI_N_("CIS calibration"),UI_N_("Image sensor calibration"),"EntryCis",SETTINGS_DETAIL,UI_PAGE_CIS_CALIB,"calibration"},
 {"whiteBalance","calibration",UI_N_("White balance"),UI_N_("Color balance calibration"),"EntryBalance",SETTINGS_DETAIL,UI_PAGE_CIS_CALIB,"calibration"},
 {"motor","maintenance",UI_N_("Motor test"),UI_N_("Individual motor controls"),"EntryMotor",SETTINGS_DETAIL,UI_PAGE_MOTOR_TEST,"hardware"},
 {"sensors","maintenance",UI_N_("Sensors"),UI_N_("Live channel voltages"),"EntrySensor",SETTINGS_DETAIL,UI_PAGE_SENSOR,"hardware"},
 {"flap","maintenance",UI_N_("Flap position"),UI_N_("Upper or lower note path"),"EntryFlap",SETTINGS_DETAIL,UI_PAGE_FLAP_SETTING,"hardware"},
 {"image","maintenance",UI_N_("Image capture"),UI_N_("Inspect sensor images"),"EntryImage",SETTINGS_DETAIL,UI_PAGE_IMAGE_GET,"inspection"},
 {"wave","maintenance",UI_N_("Waveform"),UI_N_("Magnetic and UV signals"),"EntryWave",SETTINGS_DETAIL,UI_PAGE_WAVE_GET,"inspection"},
 {"aging","maintenance",UI_N_("Aging test"),UI_N_("Continuous hardware test"),"EntryAging",SETTINGS_DETAIL,UI_PAGE_AGING_SETTING,"tests"},
 {"display","maintenance",UI_N_("Color calibration"),UI_N_("Display reference and adjustment"),"EntryDisplay",SETTINGS_DETAIL,UI_PAGE_DISPLAY_TEST,"tests"},
 {"collection","data",UI_N_("Data collection"),UI_N_("All data or error reports"),"EntryCollect",SETTINGS_DETAIL,UI_PAGE_SETTING,"collection"},
 {"upgrade","data",UI_N_("Upgrade"),UI_N_("Controller, image board and UI"),"EntryUpgrade",SETTINGS_DIRECTORY,0,"updates"},
 {"mainUpdate","upgrade",UI_N_("Controller"),UI_N_("Main-board software"),"EntryBoard",SETTINGS_DETAIL,UI_PAGE_MAIN_UPGRADE,"updates"},
 {"imageUpdate","upgrade",UI_N_("Image board"),UI_N_("Image processing software"),"EntryImageBoard",SETTINGS_DETAIL,UI_PAGE_IMAGE_UPGRADE,"updates"},
 {"uiUpdate","upgrade",UI_N_("UI"),UI_N_("Display software package"),"EntryUiUpdate",SETTINGS_DETAIL,UI_PAGE_UI_UPGRADE,"updates"},
 {"versions","about",UI_N_("Versions"),UI_N_("Applications, bootloaders and FPGA"),"EntryVersion",SETTINGS_DETAIL,UI_PAGE_SETTING,"information"},
 {"password","about",UI_N_("Change password"),UI_N_("Settings access PIN"),"EntryPassword",SETTINGS_DETAIL,UI_PAGE_PASSWORD_CHANGE,"security"},
 {"debug","about",UI_N_("Debug"),NULL,"EntryDebug",SETTINGS_DETAIL,UI_PAGE_DEBUG,"service"},
 {"reset","about",UI_N_("Factory reset"),UI_N_("Restore device defaults"),"EntryReset",SETTINGS_DETAIL,UI_PAGE_FACTORY_SETTING,"security"},
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
