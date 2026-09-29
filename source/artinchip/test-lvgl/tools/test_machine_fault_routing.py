"""Exercise the actual runtime routing function with the parser and fault store."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'un260/app_service/app_protocol_runtime.c').read_text()
begin = source.index('static void handle_machine_state_reply(')
brace = source.index('{', begin)
end, depth = brace + 1, 1
while depth:
    if source[end] == '{': depth += 1
    elif source[end] == '}': depth -= 1
    end += 1
function = source[begin:end]
fixture = r'''
#include <assert.h>
#include <stdio.h>
#include "un260/protocol/machine_fault_reply.h"
#include "un260/machine_state/machine_fault.h"
static unsigned reports, malformed;
static void fault_popup_report_sensor_mask(uint32_t mask) { reports++;machine_fault_sensor_snapshot(mask); }
static void uart_debug_printf(const char *format, ...) {(void)format;malformed++;}
'''
main = r'''
int main(void) {
 uint8_t state[]={0xfd,0xdf,10,2,1,0x80,0x80,0,3,0};
 uint8_t activity[]={0xfd,0xdf,9,0x14,0,0,0,0,0};
 machine_fault_key_t boot={MACHINE_FAULT_BOOT,2,1};machine_fault_report(boot);
 handle_machine_state_reply(2,state,sizeof(state));
 assert(reports==1&&machine_fault_count()==5&&machine_fault_find(boot,NULL));
 handle_machine_state_reply(0x14,activity,sizeof(activity));
 assert(reports==1&&machine_fault_count()==5);
 activity[4]=activity[5]=activity[6]=activity[7]=255;
 handle_machine_state_reply(0x14,activity,sizeof(activity));
 assert(reports==1&&machine_fault_count()==5);
 handle_machine_state_reply(2,state,9);
 assert(malformed==1&&reports==1&&machine_fault_count()==5);
 state[5]=state[6]=state[7]=state[8]=0;
 handle_machine_state_reply(2,state,sizeof(state));
 assert(reports==2&&machine_fault_count()==1&&machine_fault_find(boot,NULL));
 puts("PASS actual machine-state routing: busy is not fault, malformed does not clear, snapshot recovery is source-scoped");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-fault-routing-') as tmp:
    code, exe = Path(tmp) / 'routing.c', Path(tmp) / 'routing'
    code.write_text(fixture + function + main)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                    '-no-pie', '-I'+str(root), str(code), str(root/'un260/protocol/machine_fault_reply.c'),
                    str(root/'un260/machine_state/machine_fault.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
