#include "fault_guide_catalog.h"
#include "un260/lv_system/ui_i18n.h"
#include <stdio.h>
#include <string.h>

#define T(key_) ((mf_text_t){UI_N_(key_)})
enum { GUIDE_FOCUS, GUIDE_JAM, GUIDE_CLEAN, GUIDE_REMOVE, GUIDE_BOTH, GUIDE_COVERS, GUIDE_DUST, GUIDE_SERVICE };
typedef struct { mf_text_t title, location; mf_view_t view; mf_zone_t zone; uint8_t kind; } entry_t;
#define E(key,loc,view,zone,kind) {{UI_N_(key)},{UI_N_(loc)},view,zone,kind}
static const entry_t starts[] = {
    E("Unrecognized start response","Exact location not reported",MF_FRONT,MF_MACHINE,GUIDE_SERVICE),
    E("Upper passage blocked","Upper passage · PS1 / PS2 / PS3 / PS4",MF_TOP,MF_PATH,GUIDE_CLEAN),
    E("Lower passage blocked","Rear lower passage · PS5",MF_REAR,MF_PATH,GUIDE_CLEAN),
    E("Reject exit blocked","Upper front · Reject exit",MF_FRONT,MF_REJECT,GUIDE_CLEAN),
    E("Reject pocket blocked","Upper front · Reject pocket RJ",MF_FRONT,MF_REJECT,GUIDE_CLEAN),
    E("Reject pocket full","Upper front · Reject pocket",MF_FRONT,MF_REJECT,GUIDE_REMOVE),
    E("Stacker pocket blocked","Lower front · Stacker pocket ST",MF_FRONT,MF_STACKER,GUIDE_CLEAN),
    E("Stacker pocket full","Lower front · Stacker pocket",MF_FRONT,MF_STACKER,GUIDE_REMOVE),
    E("Both pockets full","Both front pockets · RJ / ST",MF_FRONT,MF_REJECT,GUIDE_BOTH),
    E("Passages not closed","Upper and lower passages · SAFE",MF_TOP,MF_PATH,GUIDE_COVERS),
    E("Stacker exit blocked","Lower front · Stacker exit PS6",MF_FRONT,MF_STACKER,GUIDE_CLEAN),
    E("Dust cover or baffle open","Exact location not reported",MF_FRONT,MF_MACHINE,GUIDE_DUST),
    E("Diverter position fault","Exact location not reported",MF_SIDE,MF_MACHINE,GUIDE_SERVICE),
    E("Encoder fault","Internal side · Large / small encoder",MF_SIDE,MF_ENCODERS,GUIDE_SERVICE)
};
static const entry_t runtime[] = {
    E("Unrecognized machine fault","Exact location not reported",MF_FRONT,MF_MACHINE,GUIDE_SERVICE),
    E("Feeder jam","Top feeder · QT",MF_FRONT,MF_HOPPER,GUIDE_JAM),
    E("Upper passage jam","Upper passage · PS1 / PS2 / PS3 / PS4",MF_TOP,MF_PATH,GUIDE_JAM),
    E("Lower passage jam","Rear lower passage · PS5",MF_REAR,MF_PATH,GUIDE_JAM),
    E("Reject exit jam","Upper front · Reject exit",MF_FRONT,MF_REJECT,GUIDE_JAM),
    E("Stacker exit jam","Lower front · Stacker exit PS6",MF_FRONT,MF_STACKER,GUIDE_JAM),
    E("Diverter solenoid fault","Exact location not reported",MF_SIDE,MF_MACHINE,GUIDE_SERVICE),
    E("Notes remain in stacker","Lower front · Stacker pocket ST",MF_FRONT,MF_STACKER,GUIDE_REMOVE)
};
static const entry_t boot[] = {
    E("Self-test failed","Exact location not reported",MF_FRONT,MF_MACHINE,GUIDE_SERVICE),
    E("Sensor self-test failed","Exact location not reported",MF_FRONT,MF_MACHINE,GUIDE_FOCUS),
    E("Motor self-test failed","Exact location not reported",MF_SIDE,MF_MACHINE,GUIDE_SERVICE),
    E("Solenoid self-test failed","Exact location not reported",MF_SIDE,MF_MACHINE,GUIDE_SERVICE),
    E("Configuration read failed","Control system · No jam location",MF_FRONT,MF_MACHINE,GUIDE_SERVICE),
    E("Image board self-test failed","Internal side · Image board",MF_SIDE,MF_IMAGEBOARD,GUIDE_SERVICE)
};

static mf_step_t step(mf_text_t short_title, mf_text_t title, mf_text_t body,
                      mf_view_t view, mf_zone_t zone, mf_action_t action)
{
    return (mf_step_t){short_title, title, body, view, zone, action};
}

static mf_step_t remove_notes(mf_zone_t zone)
{
    bool reject = zone == MF_REJECT;
    return step(reject ? T("Reject pocket") : T("Stacker pocket"),
        reject ? T("Empty the reject pocket") : T("Empty the stacker pocket"),
        T("Remove the notes and check for scraps."), MF_FRONT, zone, MF_REMOVE);
}

static mf_step_t close_passage(bool lower)
{
    return step(lower ? T("Push back") : T("Close cover"),
        lower ? T("Push the passage fully back") : T("Close the passage securely"),
        T("Check for remaining scraps before closing."),
        lower ? MF_REAR : MF_TOP, MF_PATH, MF_CLOSE);
}

void fault_guide_lookup(machine_fault_key_t key, mf_guide_t *g)
{
    if (!g) return;
    entry_t e = runtime[0];
    if (key.source == MACHINE_FAULT_PRESET) {
        e = (entry_t)E("Preset count reached","Feeder · Preset count full",MF_FRONT,MF_PRESET,GUIDE_FOCUS);
    }
    else if (key.source == MACHINE_FAULT_START && key.type == 1 && key.code == 2) {
        e = (entry_t)E("No banknotes detected","Feeder · No jam location",MF_FRONT,MF_HOPPER,GUIDE_FOCUS);
    }
    else if (key.source == MACHINE_FAULT_START) e = starts[key.type == 2 && key.code < sizeof(starts)/sizeof(starts[0]) ? key.code : 0];
    else if (key.source == MACHINE_FAULT_RUNTIME) e = runtime[key.code < sizeof(runtime)/sizeof(runtime[0]) ? key.code : 0];
    else if (key.source == MACHINE_FAULT_BOOT) e = boot[key.code < sizeof(boot)/sizeof(boot[0]) ? key.code : 0];
    else if (key.source == MACHINE_FAULT_SENSOR) {
        e = boot[1];
        switch (key.code) {
        case 1: e.title=T("PS1 sensor fault"); e.location=T("Upper passage · PS1"); e.zone=MF_PATH; break;
        case 2: e.title=T("PS2 sensor fault"); e.location=T("Upper passage · PS2"); e.zone=MF_PATH; break;
        case 3: e.title=T("PS5L sensor fault"); e.location=T("Rear lower passage · PS5L"); e.zone=MF_PATH; break;
        case 4: e.title=T("PS5R sensor fault"); e.location=T("Rear lower passage · PS5R"); e.zone=MF_PATH; break;
        case 23: e=starts[13]; e.title=T("Encoder sensor fault"); break;
        case 31: e=boot[5]; e.title=T("Image board fault"); break;
        default:
            e=runtime[0];
            e.title=T("Unrecognized self-test status");
            break;
        }
        e.view=MF_FRONT;
    }
    memset(g, 0, sizeof(*g));
    g->title=e.title; g->location=e.location; g->step_count=1;
    if (key.source == MACHINE_FAULT_PRESET) {
        g->steps[0]=step(T("Remove notes"),T("Remove banknotes from the feeder"),
            T("Take the banknotes out of the feeder, then confirm to clear the preset stop."),
            MF_FRONT,MF_PRESET,MF_FOCUS); return;
    }
    if (key.source == MACHINE_FAULT_START && key.type == 1 && key.code == 2) {
        g->steps[0]=step(T("Check feeder"),T("Place notes in the feeder"),
            T("Check that banknotes are positioned in the feeder."),
            MF_FRONT,MF_HOPPER,MF_FOCUS); return;
    }
    if (e.kind == GUIDE_BOTH) {
        g->step_count=2; g->steps[0]=remove_notes(MF_REJECT); g->steps[1]=remove_notes(MF_STACKER); return;
    }
    if (e.kind == GUIDE_REMOVE) { g->steps[0]=remove_notes(e.zone); return; }
    if (e.kind == GUIDE_COVERS) {
        g->step_count=2; g->steps[0]=close_passage(false); g->steps[1]=close_passage(true); return;
    }
    if (e.kind == GUIDE_DUST) {
        g->steps[0]=step(T("Check closure"),T("Check the cover and baffle"),
            T("Return them to their closed position. Do not force them."),MF_FRONT,MF_MACHINE,MF_FOCUS); return;
    }
    if ((e.kind == GUIDE_JAM || e.kind == GUIDE_CLEAN) && e.zone == MF_PATH) {
        bool lower=e.view==MF_REAR, clean=e.kind==GUIDE_CLEAN;
        g->step_count=3;
        g->steps[0]=step(lower?T("Pull out"):T("Open passage"),
            lower?T("Pull out the lower passage"):T("Lift the upper passage cover"),
            lower?T("Power off. Use the lower rear handle to pull straight out."):T("Power off, then lift the front edge toward the rear."), e.view,MF_PATH,MF_OPEN);
        g->steps[1]=step(clean?T("Clear debris"):T("Remove notes"),
            clean?T("Clear paper and dust"):T("Remove the trapped notes"),
            clean?T("Keep power off. Clear scraps, then gently use a soft brush."):T("Keep power off. Gently remove notes and scraps."),e.view,MF_PATH,clean?MF_CLEAN:MF_REMOVE);
        g->steps[2]=close_passage(lower); return;
    }
    if (e.kind == GUIDE_JAM || e.kind == GUIDE_CLEAN) {
        bool clean=e.kind==GUIDE_CLEAN;
        g->step_count=2;
        g->steps[0]=step(T("Remove notes"),T("Clear the opening"),
            T("Remove loose notes. Do not force trapped paper."),e.view,e.zone,MF_REMOVE);
        g->steps[1]=step(T("Check area"),clean?T("Clean accessible surfaces"):T("Check for remaining scraps"),
            clean?T("Power off, then brush away scraps and dust."):T("Keep the opening clear and the baffle in place."),e.view,e.zone,clean?MF_CLEAN:MF_FOCUS); return;
    }
    mf_text_t title=T("Contact service support");
    mf_text_t body=T("Keep the error code. Do not remove internal parts.");
    if (e.zone==MF_ENCODERS) body=T("Have the encoders and connections inspected.");
    else if (e.zone==MF_IMAGEBOARD) body=T("Share the error code. Do not remove the side cover.");
    else if (e.kind==GUIDE_FOCUS) {
        title=T("Inspect the sensor area");
        body=T("Clear visible paper or dust. Contact support if it persists.");
    }
    if (key.source==MACHINE_FAULT_RUNTIME && key.code==6) {
        title=T("Wait for the front door to stop");
        body=T("Do not force the diverter. Contact support with the code.");
    }
    g->steps[0]=step(T("Fault location"),title,body,e.view,e.zone,MF_FOCUS);
}

void fault_guide_format_code(machine_fault_key_t key, char *buffer, size_t size)
{
    if (!buffer || !size) return;
    switch (key.source) {
    case MACHINE_FAULT_BOOT: snprintf(buffer,size,"0x37/0x%02X/0x%02X",key.code,key.type); break;
    case MACHINE_FAULT_START: snprintf(buffer,size,"0x0A/0x%02X/0x%02X",key.type,key.code); break;
    case MACHINE_FAULT_SENSOR: snprintf(buffer,size,"0x02/bit%u",key.code); break;
    case MACHINE_FAULT_PRESET: snprintf(buffer,size,"0x06/0x04"); break;
    default: snprintf(buffer,size,"0x0F/0x%02X",key.code); break;
    }
}
const char *fault_guide_text(mf_text_t text) { return ui_tr(text.key); }
