#!/usr/bin/env python3
"""Replay captured UART denomination streams through production parser."""
from pathlib import Path
import subprocess
import sys
import tempfile

root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='un260-denom-replay-') as temp:
    work=Path(temp)
    stub=work/'un260/lv_drivers/lv_drivers.h'
    stub.parent.mkdir(parents=True)
    stub.write_text('void uart_debug_printf(const char *fmt, ...);\n')
    harness=work/'replay.c'
    harness.write_text(r'''
#define main existing_stream_tests
#include "tools/test_denom_stream.c"
#undef main
int main(int argc,char **argv) {
    assert(argc==2); FILE *f=fopen(argv[1],"r"); assert(f);
    char line[1024];unsigned streams=0,frames=0;
    const int expected[]={500,200,100,50,20,10,5};
    reset();
    while(fgets(line,sizeof(line),f)) {
        char *p=strstr(line,"RX[16]: FD DF 10 0B");if(!p)continue;
        p=strchr(p,':')+1;uint8_t b[16];
        for(int i=0;i<16;i++){unsigned v;int n;assert(sscanf(p," %x%n",&v,&n)==1);b[i]=v;p+=n;}
        if(b[4]==0)counting_denom_query_expect_push(&d,now,true);
        counting_denom_reply_handle(&d,&s,&sim,b,sizeof(b),NULL);frames++;
        if(b[4]==255){
            assert(d.query_complete && sim.denom_number==7);
            for(int i=0;i<7;i++){assert(sim.denom[i].value==expected[i]);assert(sim.denom[i].pcs==0);}
            streams++;
        }
    }
    fclose(f);assert(streams==5 && frames==45 && sends==0 && rejects==0);
    counting_denom_query_expect_push(&d,now,true);marker(0);row(1000);row(25);marker(255);
    assert(sim.denom_number==2 && sim.denom[0].value==1000 && sim.denom[1].value==25);
    puts("PASS captured 5 streams/45 frames: 500 200 100 50 20 10 5, zero pcs; new list fully replaces old list");
    existing_stream_tests(1,argv);
    return 0;
}
''')
    binary=work/'replay'
    subprocess.run(['cc','-std=c11','-O2','-fsanitize=undefined','-fno-sanitize-recover=all',
        '-I'+str(work),'-I'+str(root),str(harness),
        str(root/'un260/counting/counting_denom_reply.c'),
        str(root/'un260/counting/counting_denom_query_service.c'),'-o',str(binary)],check=True)
    subprocess.run([str(binary),sys.argv[1]],check=True)
