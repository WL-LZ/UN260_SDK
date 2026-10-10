#!/usr/bin/env python3
"""Actual setting owners and negative ACK branches; UI rendering tested separately."""
from pathlib import Path
import os, subprocess, tempfile
from test_currency_modes import function, block, without_includes
from test_i18n_support import with_i18n
ROOT=Path(__file__).resolve().parents[1]
def main():
    recovery=(ROOT/'un260/app_service/app_fault_recovery.c').read_text()
    parts=['static struct { uint8_t command; uint64_t received_ms; } setting_rejection; static uint8_t deferred_setting_rejection;']
    parts += [function(recovery,n) for n in ['app_fault_recovery_cancel_setting_detail', 'setting_command', 'app_fault_recovery_cancel_setting_presentation', 'app_fault_recovery_resolve_setting_rejection', 'setting_request_started', 'app_fault_recovery_report_setting_rejection', 'app_fault_recovery_observe_setting_detail', 'present_deferred_setting_rejection']]
    parts += [without_includes((ROOT/'un260/app_service/app_setting_notice.c').read_text())]
    source=(ROOT/'un260/app_service/app_setting_reply_basic.c').read_text()
    cases=[]
    for command in ['0x04','0x06','0x3A']:
        start=source.index('case '+command+':')
        cases.append('case '+command+':'+block(source,source.index('{',start))+' break;')
    parts.append('app_setting_reply_action_t app_setting_reply_handle_basic(uint8_t cmd,const uint8_t *buf,uint8_t len) { app_setting_reply_action_t actions=APP_SETTING_REPLY_ACTION_NONE; switch(cmd) {'+''.join(cases)+' default: break;}return actions;}')
    sources=['tools/test_setting_rejection_guide.c','un260/app_service/setting_service.c','un260/protocol/protocol_request.c','un260/protocol/mode_codec.c','un260/machine_state/machine_state.c','un260/machine_state/machine_fault.c','un260/currency/currency_service.c','un260/currency/currency_state.c']
    with tempfile.TemporaryDirectory(prefix='un260-setting-refusal-') as temp:
        work=Path(temp);(work/'setting_rejection_under_test.inc').write_text('\n\n'.join(parts))
        driver=work/'un260/lv_drivers/lv_drivers.h';driver.parent.mkdir(parents=True);driver.write_text('void uart_debug_printf(const char *format,...);\n')
        for opt in ['-O0','-O2']:
            exe=work/'test';subprocess.run([os.environ.get('CC','cc'),'-std=c11',opt,'-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I'+str(work),'-I'+str(ROOT),*map(str,with_i18n([ROOT/p for p in sources],ROOT)),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True,timeout=30)
if __name__=='__main__':main()
