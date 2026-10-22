#ifndef PANEL_VIEW_CHECKS_H
#define PANEL_VIEW_CHECKS_H

/* Accepted coordinates are the baseline, independent of component internals. */
static void panel_test_layout(lv_obj_t *root,bool entry)
{
 lv_obj_update_layout(root);
 assert(lv_obj_get_width(root)==1280&&lv_obj_get_height(root)==400);
 lv_obj_t *content=lv_obj_get_child(root,2);
 assert(lv_obj_get_x(content)==244&&lv_obj_get_y(content)==0&&lv_obj_get_width(content)==792&&lv_obj_get_height(content)==400);
 for(unsigned side=0;side<2;side++){
  lv_obj_t *wing=lv_obj_get_child(root,side);unsigned index=0,count=entry?7:side?5:6;
  assert(lv_obj_get_x(wing)==(side?1036:0)&&lv_obj_get_y(wing)==0&&lv_obj_get_width(wing)==244&&lv_obj_get_height(wing)==400);
  for(unsigned i=0;i<lv_obj_get_child_cnt(wing);i++){
   lv_obj_t *key=lv_obj_get_child(wing,i);if(!lv_obj_check_type(key,&lv_btn_class)||lv_obj_has_flag(key,LV_OBJ_FLAG_HIDDEN))continue;
   bool wide=index+1==count&&(side||entry);int height=side?(entry?60:84):(entry?81:112);
   int x=wide?20:20+(index%2)*108,y=(side?104:20)+(index/2)*(height+12);
   assert(lv_obj_get_x(key)==x&&lv_obj_get_y(key)==y&&lv_obj_get_width(key)==(wide?203:96)&&lv_obj_get_height(key)==height);
   index++;
  }
  assert(index==count);
 }
}
static lv_obj_t *panel_test_status_slot(lv_obj_t *root)
{
 lv_obj_t *wing=lv_obj_get_child(root,1);
 for(unsigned i=0;i<lv_obj_get_child_cnt(wing);i++){
  lv_obj_t *slot=lv_obj_get_child(wing,i);
  if(lv_obj_get_x(slot)==20&&lv_obj_get_y(slot)==20&&lv_obj_get_width(slot)==203&&lv_obj_get_height(slot)==72)return slot;
 }
 assert(!"Missing fixed 203 x 72 status slot");return NULL;
}
static unsigned panel_test_status_labels(lv_obj_t *object)
{
 assert(!lv_obj_check_type(object,&lv_img_class));
 unsigned count=0;
 if(lv_obj_check_type(object,&lv_label_class)){
  assert(!lv_obj_has_flag(object,LV_OBJ_FLAG_HIDDEN)&&*lv_label_get_text(object));count=1;
 }
 for(unsigned i=0;i<lv_obj_get_child_cnt(object);i++)count+=panel_test_status_labels(lv_obj_get_child(object,i));
 return count;
}
static void panel_test_no_corner_graphics(lv_obj_t *root)
{assert(panel_test_status_labels(panel_test_status_slot(root))==2);}

#endif
