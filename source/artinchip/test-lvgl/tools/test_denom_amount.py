#!/usr/bin/env python3
"""Check exact A-row amounts through actual UI expressions and report producers.

Optionally run the existing native LVGL Main detail harness with an injected
large-denomination display case, without changing that harness on disk.
"""
import argparse
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from test_i18n_support import with_i18n

ROOT=Path(__file__).resolve().parents[1]


def run(work, lvgl):
    work.mkdir(parents=True,exist_ok=True)
    main_source=(ROOT/'un260/lv_core/page_01_main_detail.c').read_text()
    list_source=(ROOT/'un260/lv_core/page_02_list.c').read_text()
    main_statement=next(line.strip() for line in main_source.splitlines()
                        if 'snprintf' in line and 'counting_data_denom_amount' in line)
    list_statement=next(line.strip() for line in list_source.splitlines()
                        if 'number_set(c,' in line and 'counting_data_denom_amount' in line)
    list_expression=list_statement.split('number_set(c,',1)[1].split(',&',1)[0]
    list_format=next(line.strip() for line in list_source.splitlines()
                     if 'snprintf(text, sizeof(text), "%.0f", value);' in line)
    fixture=work/'amount-test.c'
    fixture.write_text(r'''
#define main original_current_export_main
#include "tools/test_current_export.c"
#undef main
#include <inttypes.h>
#include <limits.h>
static void check_cells(denom_t denom, const char *expected_text)
{
    char text[48];
    MAIN_STATEMENT
    assert(!strcmp(text,expected_text));
    const denom_t *d=&denom;
    double value=LIST_EXPRESSION;
    LIST_FORMAT
    assert(!strcmp(text,expected_text));
}
static char *read_file(const char *path)
{
    FILE *f=fopen(path,"rb");assert(f);assert(!fseek(f,0,SEEK_END));
    long n=ftell(f);assert(n>=0);rewind(f);char *text=malloc((size_t)n+1);assert(text);
    assert(fread(text,1,(size_t)n,f)==(size_t)n);text[n]=0;fclose(f);return text;
}
int main(int argc,char **argv)
{
    assert(argc==2);
    const struct { int denomination; uint16_t pcs; uint64_t amount; } cases[]={
        {50000,9999,UINT64_C(499950000)}, {100000,9999,UINT64_C(999900000)},
        {10,0,0}, {10,83,830}, {INT_MAX,UINT16_MAX,UINT64_C(140735340806145)}
    };
    counting_sim_t *data=counting_data_mutable();
    char path[1024], exact[48], csv_row[128], html_row[256], bar_value[128];
    for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
        memset(data,0,sizeof(*data));
        data->denom_number=1;
        data->denom[0]=(denom_t){cases[i].denomination,cases[i].pcs,
                                (float)cases[i].denomination*cases[i].pcs};
        data->total_pcs=cases[i].pcs;data->total_amount=(double)cases[i].amount;
        assert(counting_data_denom_amount(&data->denom[0])==cases[i].amount);
        snprintf(exact,sizeof(exact),"%" PRIu64,cases[i].amount);
        check_cells(data->denom[0],exact);
        snprintf(path,sizeof(path),"%s/exact-%u.csv",argv[1],i);
        assert(ui_export_data_write_csv_file(path));
        char *text=read_file(path);
        snprintf(csv_row,sizeof(csv_row),"%d,%u,%s\n",cases[i].denomination,cases[i].pcs,exact);
        assert(strstr(text,csv_row));
        snprintf(csv_row,sizeof(csv_row),"Total Amount,%s\n",exact);
        assert(strstr(text,csv_row));free(text);
        snprintf(path,sizeof(path),"%s/exact-%u.html",argv[1],i);
        assert(ui_export_data_write_html_file(path));text=read_file(path);
        snprintf(html_row,sizeof(html_row),"<tr><td class=\"num-font\">%d</td><td class=\"text-right num-font\">%u</td><td class=\"text-right num-font\">%s</td></tr>",
                 cases[i].denomination,cases[i].pcs,exact);
        snprintf(bar_value,sizeof(bar_value),"<span class=\"amount-value\">%s</span>",exact);
        assert(strstr(text,html_row));assert(strstr(text,bar_value));
        snprintf(bar_value,sizeof(bar_value),"data-target=\"%s\"",exact);
        assert(strstr(text,bar_value));free(text);
        printf("PASS exact A amount: %d x %u = %s; Main/List expressions and real CSV/HTML row/chart\n",
               cases[i].denomination,cases[i].pcs,exact);
    }
    assert(counting_data_denom_amount(NULL)==0);
    denom_t invalid={-1,10,123};assert(counting_data_denom_amount(&invalid)==0);
    return 0;
}
'''.replace('MAIN_STATEMENT',main_statement).replace('LIST_EXPRESSION',list_expression).replace('LIST_FORMAT',list_format))
    sources=with_i18n([ROOT/'un260/lv_system/ui_report_i18n.c',
        ROOT/'un260/counting/counting_data_store.c',
        ROOT/'un260/counting/counting_reject_reason.c',
        ROOT/'un260/machine_state/machine_state.c'],ROOT)
    executable=work/'test-amount'
    subprocess.run([os.environ.get('CC') or shutil.which('cc') or 'gcc',
        '-std=c11','-D_POSIX_C_SOURCE=200809L','-O1','-g','-Wall','-Wextra','-Werror',
        '-fsanitize=undefined','-fno-sanitize-recover=all',
        '-ffunction-sections','-fdata-sections','-Wl,--gc-sections',
        '-I'+str(ROOT/'tools/tests/history_export_stubs'),'-I'+str(ROOT),
        str(fixture),*(str(p) for p in sources),'-o',str(executable)],check=True)
    subprocess.run([str(executable),str(work)],check=True)
    if lvgl:
        original=ROOT/'tools/test_main_detail_view.c'
        injected=r'''
    data->denom_number=1;
    data->denom[0]=(denom_t){50000,9999,(float)50000*9999};
    data->total_pcs=9999;
    page_01_main_detail_refresh(PAGE_01_DETAIL_SECTION_A);render();
    assert(!strcmp(cell(0,0),"50000"));
    assert(!strcmp(cell(0,1),"9999"));
    assert(!strcmp(cell(0,2),"499950000"));
    write_bmp("main-a-exact-amount");
    puts("PASS native LVGL Main A: 50000 x 9999 displays 499950000");
'''
        view_fixture=work/'main-amount-view.c'
        source=original.read_text()
        needle='    test_reject_live_projection();'
        assert source.count(needle)==1
        view_fixture.write_text(source.replace(needle,injected+'\n'+needle))
        script=ROOT/'tools/test_main_detail_view.py'
        program=script.read_text().replace('ROOT / "tools/test_main_detail_view.c"',
                                           'Path('+repr(str(view_fixture))+')')
        saved=sys.argv
        try:
            sys.argv=[str(script),'--lvgl-dir',str(lvgl),'--output-dir',str(work/'native-main')]
            exec(compile(program,str(script),'exec'),{'__name__':'__main__','__file__':str(script)})
        finally:
            sys.argv=saved
        run_list_amount(work,lvgl)
        run_main_total(work,lvgl)


def run_list_amount(work,lvgl):
    original=ROOT/'tools/test_list_view.c'
    source=original.read_text().replace('int main(void)','int original_list_suite_main(void)')
    source+=r'''
int main(void)
{
    lv_init();register_host_assets();
    static lv_color_t pixels[1280*40];static lv_disp_draw_buf_t buf;
    lv_disp_draw_buf_init(&buf,pixels,NULL,1280*40);
    lv_disp_drv_t driver;lv_disp_drv_init(&driver);driver.hor_res=1280;driver.ver_res=400;
    driver.draw_buf=&buf;driver.flush_cb=flush;lv_disp_drv_register(&driver);
    counting_sim_t *data=counting_data_mutable();
    data->denom_number=1;data->denom[0]=(denom_t){50000,9999,(float)50000*9999};
    data->total_pcs=9999;data->total_amount=(double)50000*9999;
    ui_page_02_list_create(lv_scr_act());tick(100);assert(view);
    page_02_list_report_reset();tick(100);render_page();
    lv_obj_t *viewport=lv_recycled_list_object(view->section[0].list);
    lv_obj_t *row=lv_obj_get_child(viewport,0);
    assert(!strcmp(lv_label_get_text(lv_obj_get_child(row,0)),"50000"));
    assert(!strcmp(lv_label_get_text(lv_obj_get_child(row,1)),"9999"));
    assert(!strcmp(lv_label_get_text(lv_obj_get_child(row,2)),"499950000"));
    assert(!strcmp(lv_label_get_text(view->amount),"499950000"));
    label_fits(lv_obj_get_child(row,2));
    write_bmp("list-a-exact-amount");
    ui_page_02_list_destroy();lv_deinit();
    puts("PASS native LVGL List A: 50000 x 9999 row and TOTAL display 499950000 and fit the existing cell");
    return 0;
}
'''
    fixture=work/'list-amount-view.c';fixture.write_text(source)
    script=ROOT/'tools/test_list_view.py'
    program=script.read_text().replace('ROOT / "tools/test_list_view.c"','Path('+repr(str(fixture))+')')
    program=program.replace('f"-I{work}", f"-I{ROOT}", f"-I{lvgl}"',
                            'f"-I{work}", f"-I{ROOT}", f"-I{ROOT}/tools", f"-I{lvgl}"')
    saved=sys.argv
    try:
        sys.argv=[str(script),'--lvgl-dir',str(lvgl),'--output-dir',str(work/'native-list')]
        exec(compile(program,str(script),'exec'),{'__name__':'__main__','__file__':str(script)})
    finally:
        sys.argv=saved


def run_main_total(work,lvgl):
    original=ROOT/'tools/test_main_view.c'
    source=original.read_text()
    setup=source[source.index('    setvbuf(stdout,NULL,_IONBF,0);',source.index('int main(void)')):]
    setup=setup[:setup.index('    assert(page_01_main_layout_is_enabled()')]
    harness=work/'main-total-amount.c'
    renamed=source.replace('int main(void)','int original_main_suite_main(void)')
    renamed=re.sub(r'#include "([^"/]+)"',lambda m: '#include "'+str(ROOT/'tools'/m[1])+'"' if (ROOT/'tools'/m[1]).is_file() else m[0],renamed)
    harness.write_text(renamed+'\nint main(void)\n{\n'+setup+r'''
    fixture(true);assert(currency_state_confirm_active_code("USD"));
    ui_main_create(lv_scr_act());page_01_main_resume();tick(300);
    counting_sim_t *data=counting_data_mutable();
    data->denom_number=1;data->denom[0]=(denom_t){50000,9999,(float)50000*9999};
    data->total_pcs=9999;data->total_amount=(double)50000*9999;
    page_01_detail_section_set(PAGE_01_DETAIL_SECTION_A,true);
    ui_refresh_main_page();tick(100);render();
    assert(!strcmp(lv_label_get_text(s_total_amount_label),"499,950,000"));
    assert(s_main_counting_snapshot.total_amount==499950000.0);
    lv_obj_t *viewport=lv_recycled_list_object(detail_view->section[0].list);
    lv_obj_t *row=lv_obj_get_child(viewport,0);
    assert(!strcmp(lv_label_get_text(lv_obj_get_child(row,2)),"499950000"));
    write_bmp("main-total-exact-amount");
    smart_island_notify_count_start();smart_island_update_counting(9999,4000000000.0);
    assert(g_si_ctx.counting.amount==4000000000.0);
    ui_main_destroy();counting_data_clear_serials(data);counting_data_clear_errors(data);
    lv_indev_delete(indev);lv_img_decoder_delete(decoder);lv_deinit();host_external_assets_release();
    puts("PASS native LVGL Main total/row/snapshot: 499,950,000 / 499950000; island amount 4000000000 without int overflow");
    return 0;
}
''')
    subprocess.run([sys.executable,str(ROOT/'tools/test_main_view.py'),'--lvgl-dir',str(lvgl),
                    '--harness',str(harness),'--output-dir',str(work/'native-main-total')],check=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path)
    parser.add_argument('--lvgl-dir',type=Path)
    args=parser.parse_args()
    if args.output:
        run(args.output.resolve(),args.lvgl_dir)
    else:
        with tempfile.TemporaryDirectory(prefix='un260-denom-amount-') as directory:
            run(Path(directory),args.lvgl_dir)


if __name__=='__main__':main()
