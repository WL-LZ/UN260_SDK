from pathlib import Path

root = Path(__file__).resolve().parents[1]
for directory in ('un260', 'tools'):
    for path in (root / directory).rglob('*'):
        if path.suffix not in ('.c', '.h'):
            continue
        source = path.read_text(errors='replace')
        for obsolete in ('icon_feedback_comp', 'page_01_main_icon_feedback', 'show_icon_feedback'):
            assert obsolete not in source, (path, obsolete)
events = (root / 'un260/lv_core/lv_page_event.c').read_text()
assert 'app_command_runtime_clear_counting_data("user clear")' in events
assert 'app_command_runtime_request_count_start()' in events
reply = (root / 'un260/app_service/app_setting_reply_basic.c').read_text()
assert 'setting_service_take_mode_result(&requested_mode)' in reply
assert 'machine_state_confirm_mode(requested_mode)' in reply
print('PASS obsolete icon zoom path and test stubs removed; command/ACK paths retained')
