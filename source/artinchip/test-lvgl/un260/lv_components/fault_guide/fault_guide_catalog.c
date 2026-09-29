#include "fault_guide_catalog.h"
#include <stdio.h>
#include <string.h>

#define T(en_, zh_) ((mf_text_t){en_, zh_})
enum { GUIDE_FOCUS, GUIDE_JAM, GUIDE_CLEAN, GUIDE_REMOVE, GUIDE_BOTH, GUIDE_COVERS, GUIDE_DUST, GUIDE_SERVICE };
typedef struct { mf_text_t title, location; mf_view_t view; mf_zone_t zone; uint8_t kind; } entry_t;
#define E(en,zh,loc,lzh,view,zone,kind) {{en,zh},{loc,lzh},view,zone,kind}
static const entry_t starts[] = {
    E("Unrecognized start response","未识别的启动响应","Exact location not reported","故障位置未细分",MF_FRONT,MF_MACHINE,GUIDE_SERVICE),
    E("Upper passage blocked","上通道传感器被遮挡","Upper passage · PS1 / PS2 / PS3 / PS4","上币道 · PS1 / PS2 / PS3 / PS4",MF_TOP,MF_PATH,GUIDE_CLEAN),
    E("Lower passage blocked","下通道传感器被遮挡","Rear lower passage · PS5","背面下币道 · PS5",MF_REAR,MF_PATH,GUIDE_CLEAN),
    E("Reject exit blocked","退钞出口传感器被遮挡","Upper front · Reject exit","正面上层 · 退钞出口",MF_FRONT,MF_REJECT,GUIDE_CLEAN),
    E("Reject pocket blocked","退钞口传感器被遮挡","Upper front · Reject pocket RJ","正面上层 · 退钞口 RJ",MF_FRONT,MF_REJECT,GUIDE_CLEAN),
    E("Reject pocket full","退钞口已满","Upper front · Reject pocket","正面上层 · 退钞口",MF_FRONT,MF_REJECT,GUIDE_REMOVE),
    E("Stacker pocket blocked","接钞口传感器被遮挡","Lower front · Stacker pocket ST","正面下层 · 接钞口 ST",MF_FRONT,MF_STACKER,GUIDE_CLEAN),
    E("Stacker pocket full","接钞口已满","Lower front · Stacker pocket","正面下层 · 接钞口",MF_FRONT,MF_STACKER,GUIDE_REMOVE),
    E("Both pockets full","接钞口和退钞口已满","Both front pockets · RJ / ST","正面上、下两层 · RJ / ST",MF_FRONT,MF_REJECT,GUIDE_BOTH),
    E("Passages not closed","上、下通道未闭合","Upper and lower passages · SAFE","上、下币道 · SAFE",MF_TOP,MF_PATH,GUIDE_COVERS),
    E("Stacker exit blocked","接钞出口传感器被遮挡","Lower front · Stacker exit PS6","正面下层 · 真钞出口 PS6",MF_FRONT,MF_STACKER,GUIDE_CLEAN),
    E("Dust cover or baffle open","防尘罩或挡板未闭合","Exact location not reported","故障位置未细分",MF_FRONT,MF_MACHINE,GUIDE_DUST),
    E("Diverter position fault","翻板位置异常","Exact location not reported","故障位置未细分",MF_SIDE,MF_MACHINE,GUIDE_SERVICE),
    E("Encoder fault","码盘检测异常","Internal side · Large / small encoder","侧面内部 · 大 / 小码盘",MF_SIDE,MF_ENCODERS,GUIDE_SERVICE)
};
static const entry_t runtime[] = {
    E("Unrecognized machine fault","未识别的机器异常","Exact location not reported","故障位置未细分",MF_FRONT,MF_MACHINE,GUIDE_SERVICE),
    E("Feeder jam","进钞口卡钞","Top feeder · QT","顶部进钞口 · QT",MF_FRONT,MF_HOPPER,GUIDE_JAM),
    E("Upper passage jam","上通道卡钞","Upper passage · PS1 / PS2 / PS3 / PS4","上币道 · PS1 / PS2 / PS3 / PS4",MF_TOP,MF_PATH,GUIDE_JAM),
    E("Lower passage jam","下通道卡钞","Rear lower passage · PS5","背面下币道 · PS5",MF_REAR,MF_PATH,GUIDE_JAM),
    E("Reject exit jam","退钞出口卡钞","Upper front · Reject exit","正面上层 · 退钞出口",MF_FRONT,MF_REJECT,GUIDE_JAM),
    E("Stacker exit jam","接钞出口卡钞","Lower front · Stacker exit PS6","正面下层 · 真钞出口 PS6",MF_FRONT,MF_STACKER,GUIDE_JAM),
    E("Diverter solenoid fault","拨叉电磁铁翻板异常","Exact location not reported","故障位置未细分",MF_SIDE,MF_MACHINE,GUIDE_SERVICE),
    E("Notes remain in stacker","接钞口有遗留钞票","Lower front · Stacker pocket ST","正面下层 · 接钞口 ST",MF_FRONT,MF_STACKER,GUIDE_REMOVE)
};
static const entry_t boot[] = {
    E("Self-test failed","自检失败","Exact location not reported","故障位置未细分",MF_FRONT,MF_MACHINE,GUIDE_SERVICE),
    E("Sensor self-test failed","传感器自检失败","Exact location not reported","故障位置未细分",MF_TOP,MF_MACHINE,GUIDE_FOCUS),
    E("Motor self-test failed","电机自检失败","Exact location not reported","故障位置未细分",MF_SIDE,MF_MACHINE,GUIDE_SERVICE),
    E("Solenoid self-test failed","电磁铁自检失败","Exact location not reported","故障位置未细分",MF_SIDE,MF_MACHINE,GUIDE_SERVICE),
    E("Configuration read failed","配置读取失败","Control system · No jam location","控制系统 · 非通道卡钞",MF_REAR,MF_MACHINE,GUIDE_SERVICE),
    E("Image board self-test failed","图像板自检失败","Internal side · Image board","侧面内部 · 图像板",MF_SIDE,MF_IMAGEBOARD,GUIDE_SERVICE)
};

static mf_step_t step(mf_text_t short_title, mf_text_t title, mf_text_t body,
                      mf_view_t view, mf_zone_t zone, mf_action_t action)
{
    return (mf_step_t){short_title, title, body, view, zone, action};
}

static mf_step_t remove_notes(mf_zone_t zone)
{
    bool reject = zone == MF_REJECT;
    return step(reject ? T("Reject pocket","退钞口") : T("Stacker pocket","接钞口"),
        reject ? T("Empty the reject pocket","取出退钞口钞票") : T("Empty the stacker pocket","取出接钞口钞票"),
        T("Remove the notes and check for scraps.","取出钞票，检查是否有纸张残留。"), MF_FRONT, zone, MF_REMOVE);
}

static mf_step_t close_passage(bool lower)
{
    return step(lower ? T("Push back","推回币道") : T("Close cover","合上盖板"),
        lower ? T("Push the passage fully back","将下币道推回到位") : T("Close the passage securely","将上币道盖合到位"),
        T("Check for remaining scraps before closing.","确认没有残留纸张，再恢复原位。"),
        lower ? MF_REAR : MF_TOP, MF_PATH, MF_CLOSE);
}

void fault_guide_lookup(machine_fault_key_t key, mf_guide_t *g)
{
    if (!g) return;
    entry_t e = runtime[0];
    if (key.source == MACHINE_FAULT_START) e = starts[key.type == 2 && key.code < sizeof(starts)/sizeof(starts[0]) ? key.code : 0];
    else if (key.source == MACHINE_FAULT_RUNTIME) e = runtime[key.code < sizeof(runtime)/sizeof(runtime[0]) ? key.code : 0];
    else if (key.source == MACHINE_FAULT_BOOT) e = boot[key.code < sizeof(boot)/sizeof(boot[0]) ? key.code : 0];
    else if (key.source == MACHINE_FAULT_SENSOR) {
        e = boot[1];
        switch (key.code) {
        case 1: e.title=T("PS1 sensor fault","位置1传感器异常"); e.location=T("Upper passage · PS1","上币道 · PS1"); e.zone=MF_PATH; break;
        case 2: e.title=T("PS2 sensor fault","位置2传感器异常"); e.location=T("Upper passage · PS2","上币道 · PS2"); e.zone=MF_PATH; break;
        case 3: e.title=T("PS5L sensor fault","位置5左传感器异常"); e.location=T("Rear lower passage · PS5L","背面下币道 · PS5L"); e.view=MF_REAR; e.zone=MF_PATH; break;
        case 4: e.title=T("PS5R sensor fault","位置5右传感器异常"); e.location=T("Rear lower passage · PS5R","背面下币道 · PS5R"); e.view=MF_REAR; e.zone=MF_PATH; break;
        case 23: e=starts[13]; e.title=T("Encoder sensor fault","码盘传感器异常"); break;
        case 31: e=boot[5]; e.title=T("Image board fault","图像板异常"); break;
        default:
            e=runtime[0];
            e.title=T("Unrecognized self-test status","未识别的自检状态");
            break;
        }
    }
    memset(g, 0, sizeof(*g));
    g->title=e.title; g->location=e.location; g->step_count=1;
    if (e.kind == GUIDE_BOTH) {
        g->step_count=2; g->steps[0]=remove_notes(MF_REJECT); g->steps[1]=remove_notes(MF_STACKER); return;
    }
    if (e.kind == GUIDE_REMOVE) { g->steps[0]=remove_notes(e.zone); return; }
    if (e.kind == GUIDE_COVERS) {
        g->step_count=2; g->steps[0]=close_passage(false); g->steps[1]=close_passage(true); return;
    }
    if (e.kind == GUIDE_DUST) {
        g->steps[0]=step(T("Check closure","检查闭合"),T("Check the cover and baffle","检查防尘罩与挡板"),
            T("Return them to their closed position. Do not force them.","恢复正常闭合位置，不要强行推动。"),MF_FRONT,MF_MACHINE,MF_FOCUS); return;
    }
    if ((e.kind == GUIDE_JAM || e.kind == GUIDE_CLEAN) && e.zone == MF_PATH) {
        bool lower=e.view==MF_REAR, clean=e.kind==GUIDE_CLEAN;
        g->step_count=3;
        g->steps[0]=step(lower?T("Pull out","拉出币道"):T("Open passage","打开币道"),
            lower?T("Pull out the lower passage","拉出背面下币道"):T("Lift the upper passage cover","抬起上币道盖板"),
            lower?T("Power off. Use the lower rear handle to pull straight out.","断电后，握住背面下方把手平直拉出。"):T("Power off, then lift the front edge toward the rear.","断电后，从前缘抬起盖板向后翻开。"), e.view,MF_PATH,MF_OPEN);
        g->steps[1]=step(clean?T("Clear debris","清理遮挡"):T("Remove notes","取出卡钞"),
            clean?T("Clear paper and dust","清理纸屑与浮尘"):T("Remove the trapped notes","取出卡住的钞票"),
            clean?T("Keep power off. Clear scraps, then gently use a soft brush.","保持断电，取出纸屑后用软刷轻轻清理。"):T("Keep power off. Gently remove notes and scraps.","保持断电，轻轻取出钞票与碎片。"),e.view,MF_PATH,clean?MF_CLEAN:MF_REMOVE);
        g->steps[2]=close_passage(lower); return;
    }
    if (e.kind == GUIDE_JAM || e.kind == GUIDE_CLEAN) {
        bool clean=e.kind==GUIDE_CLEAN;
        g->step_count=2;
        g->steps[0]=step(T("Remove notes","取出纸张"),T("Clear the opening","清除出口的纸张"),
            T("Remove loose notes. Do not force trapped paper.","轻轻取出松动纸张，夹紧时不要硬拉。"),e.view,e.zone,MF_REMOVE);
        g->steps[1]=step(T("Check area","检查通道"),clean?T("Clean accessible surfaces","清理可接触区域"):T("Check for remaining scraps","检查是否还有残留"),
            clean?T("Power off, then brush away scraps and dust.","断电后，用软刷清除纸屑与浮尘。"):T("Keep the opening clear and the baffle in place.","确认出口畅通，挡板已回到正常位置。"),e.view,e.zone,clean?MF_CLEAN:MF_FOCUS); return;
    }
    mf_text_t title=T("Contact service support","联系售后检查");
    mf_text_t body=T("Keep the error code. Do not remove internal parts.","记录错误码，不拆卸内部部件。");
    if (e.zone==MF_ENCODERS) body=T("Have the encoders and connections inspected.","由售后检查码盘与连接。");
    else if (e.zone==MF_IMAGEBOARD) body=T("Share the error code. Do not remove the side cover.","提供错误码，请勿自行拆卸侧盖。");
    else if (e.kind==GUIDE_FOCUS) {
        title=T("Inspect the sensor area","检查检测区域");
        body=T("Clear visible paper or dust. Contact support if it persists.","清除可见纸屑与遮挡；仍异常时联系售后。");
    }
    if (key.source==MACHINE_FAULT_RUNTIME && key.code==6) {
        title=T("Wait for the front door to stop","等待前门动作停止");
        body=T("Do not force the diverter. Contact support with the code.","不要推动内部翻板，记录错误码联系售后。");
    }
    g->steps[0]=step(T("Fault location","异常位置"),title,body,e.view,e.zone,MF_FOCUS);
}

void fault_guide_format_code(machine_fault_key_t key, char *buffer, size_t size)
{
    if (!buffer || !size) return;
    switch (key.source) {
    case MACHINE_FAULT_BOOT: snprintf(buffer,size,"0x37/0x%02X/0x%02X",key.code,key.type); break;
    case MACHINE_FAULT_START: snprintf(buffer,size,"0x0A/0x%02X/0x%02X",key.type,key.code); break;
    case MACHINE_FAULT_SENSOR: snprintf(buffer,size,"0x02/bit%u",key.code); break;
    default: snprintf(buffer,size,"0x0F/0x%02X",key.code); break;
    }
}
const char *fault_guide_text(mf_text_t text, bool chinese) { return chinese && text.zh ? text.zh : text.en; }
