#include "un260/machine_state/machine_fault.h"
#include "un260/lv_components/fault_guide/fault_guide_catalog.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static machine_fault_key_t key(machine_fault_source_t s,uint8_t type,uint8_t code)
{return (machine_fault_key_t){s,type,code};}
int main(void)
{
    machine_fault_record_t r;
    machine_fault_key_t upper=key(MACHINE_FAULT_RUNTIME,0,2),lower=key(MACHINE_FAULT_RUNTIME,0,3);
    machine_fault_clear();
    assert(machine_fault_report(upper));assert(!machine_fault_report(upper));
    machine_fault_acknowledge(upper);
    assert(machine_fault_find(upper,&r) && r.acknowledged);
    assert(!machine_fault_first_unread(NULL));assert(!machine_fault_report(upper));
    assert(machine_fault_count()==1); /* Acknowledgement is not recovery. */
    assert(machine_fault_report(lower));assert(!machine_fault_find(upper,NULL));
    assert(machine_fault_count()==1 && machine_fault_first_unread(&r));
    assert(machine_fault_key_equal(r.key,lower)); /* 0x0F is a state, not a log. */

    machine_fault_report(key(MACHINE_FAULT_BOOT,2,5));
    uint32_t sensors=(UINT32_C(1)<<1)|(UINT32_C(1)<<31);
    assert(machine_fault_sensor_snapshot(sensors));assert(machine_fault_count()==4);
    machine_fault_acknowledge(key(MACHINE_FAULT_SENSOR,0,1));
    assert(!machine_fault_sensor_snapshot(sensors));
    assert(machine_fault_find(key(MACHINE_FAULT_SENSOR,0,1),&r)&&r.acknowledged);
    machine_fault_clear_source(MACHINE_FAULT_RUNTIME);
    machine_fault_clear_source(MACHINE_FAULT_START);
    assert(machine_fault_count()==3); /* Runtime normal cannot erase self-test. */
    assert(machine_fault_sensor_snapshot(UINT32_C(1)<<31));
    assert(machine_fault_count()==2 && !machine_fault_find(key(MACHINE_FAULT_SENSOR,0,1),NULL));
    assert(machine_fault_sensor_snapshot(sensors));
    assert(machine_fault_find(key(MACHINE_FAULT_SENSOR,0,1),&r)&&!r.acknowledged);
    machine_fault_clear_code(MACHINE_FAULT_BOOT,5);assert(machine_fault_count()==2);
    machine_fault_sensor_snapshot(0);assert(machine_fault_count()==0);
    machine_fault_sensor_snapshot(UINT32_MAX);assert(machine_fault_count()==32);
    for(uint8_t i=1;i<=5;++i)assert(machine_fault_report(key(MACHINE_FAULT_BOOT,2,i)));
    assert(machine_fault_report(upper));assert(machine_fault_report(key(MACHINE_FAULT_START,2,1)));
    assert(machine_fault_count()==39); /* Full snapshot fits without allocating. */

    mf_guide_t g;char code[48];
    fault_guide_lookup(key(MACHINE_FAULT_START,1,2),&g);
    assert(g.steps[0].zone==MF_MACHINE); /* No-notes never maps to lower path. */
    fault_guide_lookup(key(MACHINE_FAULT_START,2,2),&g);
    assert(g.step_count==3 && g.steps[0].view==MF_REAR && g.steps[0].action==MF_OPEN);
    assert(g.steps[1].action==MF_CLEAN && g.steps[2].action==MF_CLOSE);
    fault_guide_lookup(upper,&g);
    assert(g.steps[0].view==MF_TOP && g.steps[1].action==MF_REMOVE);
    fault_guide_lookup(key(MACHINE_FAULT_START,2,13),&g);
    assert(g.step_count==1 && g.steps[0].zone==MF_ENCODERS && g.steps[0].action==MF_FOCUS);
    fault_guide_lookup(key(MACHINE_FAULT_BOOT,2,5),&g);
    assert(g.steps[0].view==MF_SIDE && g.steps[0].zone==MF_IMAGEBOARD);
    fault_guide_lookup(key(MACHINE_FAULT_SENSOR,0,31),&g);assert(g.steps[0].zone==MF_IMAGEBOARD);
    fault_guide_lookup(key(MACHINE_FAULT_RUNTIME,0,225),&g);
    assert(g.step_count==1 && g.steps[0].zone==MF_MACHINE && g.steps[0].action==MF_FOCUS);
    fault_guide_format_code(key(MACHINE_FAULT_BOOT,9,5),code,sizeof(code));assert(!strcmp(code,"0x37/0x05/0x09"));
    fault_guide_format_code(key(MACHINE_FAULT_START,1,2),code,sizeof(code));assert(!strcmp(code,"0x0A/0x01/0x02"));
    fault_guide_format_code(key(MACHINE_FAULT_SENSOR,0,31),code,sizeof(code));assert(!strcmp(code,"0x02/bit31"));
    for(uint8_t i=1;i<=13;++i) {
        fault_guide_lookup(key(MACHINE_FAULT_START,2,i),&g);assert(g.step_count>=1&&g.step_count<=3);
        for(uint8_t j=0;j<g.step_count;++j)assert(g.steps[j].title.key&&g.steps[j].body.key&&g.steps[j].short_title.key);
    }
    puts("machine fault state/catalogue tests passed");return 0;
}
