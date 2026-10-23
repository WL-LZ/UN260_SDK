#!/usr/bin/env python3
"""Build-time Menu setting boundary audit; functional tests verify outcomes."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
files=[root/'un260/lv_core/page_03_menu.c',*sorted((root/'un260/lv_core').glob('menu_*.inc')),root/'un260/lv_components/lv_quick_controls.c']
text="\n".join(p.read_text() for p in files)
# Each mutating request/save API in Menu must have an explicitly reviewed owner.
# View setters, draft selection and navigation are not persistent commits.
reviewed={
 'app_standby_runtime_request':'navigation + blocker',
 'page_03_menu_show_batch_saved_tip':'legacy notification adapter',
 'print_config_request_field':'print_config result/timeout + app_setting_reply_detail',
 'record_request':'cashbook async owner',
 'app_add_runtime_request':'app_add_runtime: settings.add confirmation then count.add-reset outcome/retry',
 'setting_service_request_batch_switch':'settings.batch',
 'setting_service_request_beep':'settings.sound',
 'setting_service_request_fo_mode':'settings.sorting',
 'setting_service_request_mode':'settings.mode',
 'setting_service_request_reject_pocket_max':'settings.reject_pocket',
 'setting_service_request_speed':'settings.speed',
 'setting_service_request_work_mode':'settings.work_mode',
 'standby_store_save':'app_standby_runtime + standby page completion',
 'top_popup_save_failed':'disabled-notice failure dialog',
 'ui_export_data_request':'export owner',
 'user_cfg_qr_option_save':'QR selection: confirmed persisted value + failure dialog',
 'workspace_service_save_batches':'APP_UI_NOTICE_BATCH_SAVE',
 'workspace_store_save':'APP_UI_NOTICE_WORKSPACE_STORE'
}
mutations=set(re.findall(r'\b([a-zA-Z_]\w*(?:_request\w*|_save\w*|_set_enabled|_swap))\s*\(',text))
assert mutations==set(reviewed), 'Review setting feedback ownership: '+str(mutations^set(reviewed))
local=(root/'un260/app_service/app_local_setting.c').read_text()
header=(root/'un260/app_service/app_local_setting.h').read_text()
ids=set(re.findall(r'APP_LOCAL_[A-Z_]+',header)) - {'APP_LOCAL_SETTING_COUNT','APP_LOCAL_SETTING_H'}
entries=set(re.findall(r'\[(APP_LOCAL_[A-Z_]+)\]\s*=\{',local))
assert ids==entries, 'Every local setting needs execution and feedback metadata'
assert ids <= set(re.findall(r'APP_LOCAL_[A-Z_]+',text)), 'Unused local setting policy'
reply=(root/'un260/app_service/app_setting_reply_basic.c').read_text()+(root/'un260/app_service/app_setting_reply_detail.c').read_text()
runtime=(root/'un260/app_service/app_setting_runtime.c').read_text()
for key in reviewed.values():
 if key.startswith('settings.'):
  assert re.search(r'app_setting_notice_result\("'+re.escape(key)+r'"',reply), 'Missing completion: '+key
  assert key in runtime, 'Missing timeout/failure owner: '+key
assert 'app_add_runtime_setting_result' in reply, 'ADD must hand matched confirmation to its transaction owner'
assert 'app_add_runtime_setting_timeout' in runtime, 'ADD timeout must settle its transaction owner'
quick=(root/'un260/lv_components/lv_quick_controls.h').read_text().split('typedef enum {',1)[1].split('}',1)[0]
quick_ids=set(re.findall(r'QUICK_[A-Z_]+',quick))
assert quick_ids=={'QUICK_LAYOUT','QUICK_GESTURES','QUICK_PURE','QUICK_POPUP','QUICK_TOUCH','QUICK_EXPORT','QUICK_QR','QUICK_STANDBY','QUICK_TOP_POPUP','QUICK_CALCULATOR','QUICK_CASH_JOURNAL','QUICK_HISTORY','QUICK_OPERATORS'}, 'Classify new shortcut as setting or action'
print(f'PASS setting feedback contract: {len(reviewed)} Menu mutation boundaries, {len(ids)} local policies, {sum(key.startswith("settings.") for key in reviewed.values())} protocol settings, {len(quick_ids)} shortcut classifications (four navigation actions)')
