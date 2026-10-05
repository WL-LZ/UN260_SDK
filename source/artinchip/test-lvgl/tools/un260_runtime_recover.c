#include "un260/app_service/runtime_guard.h"
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv)
{
    if(argc!=2 || strcmp(argv[1],"--ack-reconciled")) {
        fputs("After stopping the UI service, physically recount notes and reconcile controller state.\n"
              "Then use: un260_runtime_recover --ack-reconciled\n"
              "This retains fault evidence and does not restart the application.\n",stderr);
        return 1;
    }
    return runtime_guard_reconcile();
}
