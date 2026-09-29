from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
lvgl=root.parents[1]/'third-party/lvgl-8.3.2'
with tempfile.TemporaryDirectory(prefix='un260-fault-recovery-') as d:
 work=Path(d);(work/'lvgl').mkdir();(work/'lvgl/lvgl.h').write_text('#include "'+str(lvgl/'lvgl.h')+'"\n')
 (work/'lv_conf.h').write_text('#define LV_COLOR_DEPTH 32\n#define LV_USE_GPU_AIC 0\n#define LV_USE_GPU_AIC_GE 0\n')
 exe=work/'test'
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-no-pie','-I'+str(work),'-I'+str(root),'-DLV_CONF_PATH='+str(work/'lv_conf.h'),str(root/'tools/test_fault_recovery.c'),str(root/'un260/app_service/app_fault_recovery.c'),str(root/'un260/machine_state/machine_fault.c'),str(root/'un260/protocol/protocol_request.c'),str(root/'un260/protocol/protocol_frame.c'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
