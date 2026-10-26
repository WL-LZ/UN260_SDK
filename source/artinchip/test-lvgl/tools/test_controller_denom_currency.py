#!/usr/bin/env python3
"""AUTO detection/0B ordering and MULTI runtime routing; real models, UI probes.

Each case runs independently so cross-currency ownership failures do not hide
the first-detection, same-currency or MULTI results. Host-only, not hardware.
"""
from pathlib import Path
import argparse,json,os,subprocess,tempfile
from test_main_view import function

root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output-dir',type=Path,default=Path('/dev/shm/un260-controller-denom-live/currency'))
out=parser.parse_args().output_dir;out.mkdir(parents=True,exist_ok=True)
runtime=(root/'un260/app_service/app_counting_runtime.c').read_text()
currency=(root/'un260/app_service/app_currency_runtime.c').read_text()
cases=['auto-first-detection-before','auto-first-data-before',
       'auto-same-detection-before','auto-same-data-before','auto-same-add',
       'auto-cross-detection-before','auto-cross-data-before','auto-cross-add',
       'auto-hidden-same','auto-cross-idle',
       'multi-independent-table','multi-drain-after-switch']
with tempfile.TemporaryDirectory(prefix='currency-',dir=out) as temp:
    work=Path(temp);stub=work/'un260/lv_drivers/lv_drivers.h';stub.parent.mkdir(parents=True)
    stub.write_text('void uart_debug_printf(const char *, ...);\n')
    (work/'controller_denom_currency_runtime.inc').write_text(
        function(currency,'app_currency_runtime_handle_detected')+'\n'+
        function(runtime,'app_counting_runtime_handle_denom'))
    sources=['tools/test_controller_denom_currency.c','un260/currency/currency_state.c',
        'un260/currency/currency_reply.c','un260/counting/counting_denom_reply.c',
        'un260/counting/counting_denom_query_service.c','un260/counting/counting_report_sync.c',
        'un260/counting/counting_data_store.c','un260/counting/counting_multi.c']
    cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
         '-ffunction-sections','-fdata-sections','-Wl,--gc-sections',
         '-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie',
         '-I'+str(work),'-I'+str(root),*[str(root/s) for s in sources],'-o',str(work/'test')]
    built=subprocess.run(cmd,capture_output=True,text=True)
    (out/'build.log').write_text(built.stdout+built.stderr)
    if built.returncode:print(built.stdout+built.stderr);raise SystemExit(built.returncode)
    results=[]
    for case in cases:
        run=subprocess.run([str(work/'test'),case],capture_output=True,text=True)
        output=run.stdout+run.stderr
        print(output,end='' if output.endswith('\n') else '\n',flush=True)
        results.append({'case':case,'returncode':run.returncode,'output':output})
report={'passed':sum(r['returncode']==0 for r in results),'total':len(results),'results':results}
(out/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print('SUMMARY',report['passed'],'/',report['total'])
raise SystemExit(0 if report['passed']==report['total'] else 1)
