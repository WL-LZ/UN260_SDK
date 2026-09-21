#ifndef TEST_SETTINGS_TOOLBAR_H
#define TEST_SETTINGS_TOOLBAR_H
/* Every rendered standard-frame action stays inside its bar, before Back. */
static void assert_settings_toolbars(lv_obj_t *root)
{
    if(lv_obj_has_flag(root,LV_OBJ_FLAG_HIDDEN))return;
    if(lv_obj_has_flag(root,LV_OBJ_FLAG_USER_3)){
        lv_area_t bar;lv_obj_get_coords(root,&bar);int previous=bar.x1-1;
        assert(bar.x2<1147);
        for(unsigned i=0;i<lv_obj_get_child_cnt(root);i++){
            lv_obj_t *child=lv_obj_get_child(root,i);if(lv_obj_has_flag(child,LV_OBJ_FLAG_HIDDEN))continue;
            lv_area_t a;lv_obj_get_coords(child,&a);
            assert(a.x1>previous&&a.x1>=bar.x1&&a.x2<=bar.x2);
            assert(a.y1>=bar.y1&&a.y2<=bar.y2);previous=a.x2;
        }
    }
    for(unsigned i=0;i<lv_obj_get_child_cnt(root);i++)assert_settings_toolbars(lv_obj_get_child(root,i));
}
#endif
