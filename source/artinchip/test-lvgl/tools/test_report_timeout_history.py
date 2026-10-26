#!/usr/bin/env python3
"""Real report timeout -> next normal START -> A/B/history recovery and disk reread.

The first 0D has no END. Four ADD/tail scenarios exercise actual parsers,
report state, extracted command START preparation and history/storage. UI
side effects are the established fixture. No paused repeated-START testing.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
from test_main_view import function
from test_i18n_support import with_i18n
from test_controller_denom_live import start_preparation

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir',type=Path,required=True)
    parser.add_argument('--report-source',type=Path,help='Preserved baseline report source')
    args=parser.parse_args();out=args.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
    temp_root=out/'temp';temp_root.mkdir(exist_ok=True)
    runtime=(ROOT/'un260/app_service/app_counting_runtime.c').read_text()
    command_runtime=(ROOT/'un260/app_service/app_command_runtime.c').read_text()
    fixture=(ROOT/'tools/test_denom_history.c').read_text().split('int main(int argc, char **argv)')[0]
    send_begin=fixture.index('int protocol_send(')
    send_end=fixture.index('\n\ntypedef struct',send_begin)
    fixture=fixture[:send_begin]+'static unsigned sent[256];\nint protocol_send(uint8_t command,const uint8_t *payload,uint16_t length)\n{ assert(payload&&length);++sent[command];return 0; }'+fixture[send_end:]
    fixture=fixture.replace(function(fixture,'begin'), '''static bool g_add_result_cleared;
#define g_counting_session note_session
#define g_counting_detail_state note_detail
#define counting_data_mutable() (&note_data)
#include "start_prepare.inc"
static void begin(bool add)
{
 note_add=add;
 assert(counting_history_prepare_start(&note_session,&note_data,app_clock_uptime_ms()));
 uint8_t b[7]={0xFD,0xDF,7,0x0A,1,1,0};
 command_start_prepare(0x0A,b,sizeof(b));
 counting_control_reply_hooks_t hooks={.on_start_success=start_history};
 counting_control_reply_dispatch(0x0A,&note_session,b,sizeof(b),&hooks);
 assert(note_session.start_confirmed);
}
''')
    parts=['un260/storage/storage_worker.c','un260/counting/counting_history_service.c','un260/counting/counting_data_store.c',
      'un260/history/history_record_snapshot.c','un260/history/history_record_reader.c','un260/history/history_read_task.c',
      'un260/history/history_record_detail.c','un260/history/history_export_sn_parser.c','un260/history/history_query.c',
      'un260/counting/counting_serial_text.c','un260/counting/counting_info_reply.c','un260/protocol/protocol_frame.c',
      'un260/history/history_export_text.c','un260/lv_system/ui_report_i18n.c','un260/counting/counting_reject_reason.c',
      'un260/counting/counting_control_reply.c','un260/counting/counting_report_sync.c','un260/counting/counting_denom_reply.c',
      'un260/counting/counting_denom_query_service.c','un260/counting/counting_reject_sn_reply.c']
    sources=[args.report_source if args.report_source and rel.endswith('/counting_report_sync.c') else ROOT/rel for rel in parts]
    results=[]
    env=dict(os.environ,TMPDIR=str(temp_root),TEMP=str(temp_root),TMP=str(temp_root),PYTHONDONTWRITEBYTECODE='1')
    with tempfile.TemporaryDirectory(prefix='report-timeout-',dir=temp_root) as temporary:
        work=Path(temporary);store=work/'history';exe=work/'test'
        fixture+='\n#include "test_report_timeout_history.c"\n'
        (work/'test.c').write_text(fixture);(out/'fixture-under-test.c').write_text(fixture)
        (work/'history_runtime_under_test.h').write_text(function(runtime,'app_counting_runtime_reset_session'))
        names=['app_counting_runtime_try_history_commit','app_counting_runtime_handle_info','app_counting_runtime_handle_denom','app_counting_runtime_on_history_record']
        (work/'denom_history_runtime.inc').write_text('\n\n'.join(function(runtime,name) for name in names))
        (work/'start_prepare.inc').write_text(start_preparation(command_runtime))
        stub=work/'un260/lv_drivers/lv_drivers.h';stub.parent.mkdir(parents=True);stub.write_text('void uart_debug_printf(const char *, ...);\n')
        command=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Wno-misleading-indentation','-Wno-unused-variable','-Wno-sign-compare',
          '-pthread','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I'+str(work),'-I'+str(ROOT/'tools'),
          '-I'+str(ROOT/'tools/tests/history_export_stubs'),'-I'+str(ROOT),'-I'+str(ROOT/'un260/counting'),
          '-DUI_HISTORY_STORE_DIR="'+str(store)+'"',*map(str,with_i18n([work/'test.c',*sources],ROOT)),'-Wl,--wrap=malloc','-o',str(exe)]
        built=subprocess.run(command,capture_output=True,text=True,env=env);(out/'build.log').write_text(built.stdout+built.stderr)
        if built.returncode:print(built.stdout+built.stderr);raise SystemExit(built.returncode)
        for add in (0,1):
            for tail in (0,1):
                case=f'add{add}-tail{tail}';phases=[]
                for phase in ('write','read'):
                    run=subprocess.run([str(exe),phase,str(add),str(tail)],capture_output=True,text=True,env=env,timeout=30)
                    output=run.stdout+run.stderr;(out/f'{case}-{phase}.log').write_text(output);print(output,end='',flush=True)
                    phases.append(dict(phase=phase,returncode=run.returncode))
                    if run.returncode:break
                results.append(dict(case=case,passed=len(phases)==2 and all(x['returncode']==0 for x in phases),phases=phases))
                if store.exists():store.rename(out/(case+'-history'))
    summary=dict(passed=sum(x['passed'] for x in results),cases=len(results),results=results,
      source_hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [ROOT/'tools/test_denom_history.c',ROOT/'tools/test_report_timeout_history.c',ROOT/'un260/app_service/app_counting_runtime.c',ROOT/'un260/app_service/app_command_runtime.c',*sources]},
      scope='Actual report/control/info/0B/runtime/history/storage; extracted actual command START initialization; unrelated UI effects stubbed; fresh process disk reread; no hardware test.')
    (out/'results.json').write_text(json.dumps(summary,indent=2));print('SUMMARY',summary['passed'],'/',summary['cases'])
    raise SystemExit(any(not x['passed'] for x in results))

if __name__=='__main__':main()
