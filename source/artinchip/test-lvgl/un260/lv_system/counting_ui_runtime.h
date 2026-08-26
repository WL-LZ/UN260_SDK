#ifndef COUNTING_UI_RUNTIME_H
#define COUNTING_UI_RUNTIME_H

#include "un260/counting/counting_data_types.h"

void start_counting_sim(void);
void stop_counting_sim(void);
void pause_counting_sim(void);
void resume_counting_sim(void);
void sim_data_init(void);
void cleanup_counting_sim(void);

void sim_reset_counting_result(counting_sim_t *sim_data);
void sim_reset_for_currency(counting_sim_t *sim_data);

void ui_refresh_main_page(void);
void page_01_main_detail_refresh_rows_only(void);
void ui_count_end_anim_cancel(void);
void ui_count_end_anim_begin(const char *result_text);
void ui_count_end_anim_poll(void);

#endif
