#!/usr/bin/env python3
import unittest
from audit_visible_text import analyze

class VisibleTextAudit(unittest.TestCase):
    def test_ignores_comments_and_nested_translation(self):
        raw, marked = analyze('/* lv_label_set_text(x,"Wrong"); */\n'
            'lv_label_set_text(x, ok ? ui_tr("Ready") : ui_tr("Waiting"));', 'x.c')
        self.assertEqual(raw, [])
        self.assertEqual([i['text'] for i in marked], ['Ready', 'Waiting'])

    def test_source_mark_alone_is_not_a_display_translation(self):
        raw, _ = analyze('lv_label_set_text(x, UI_N_("Ready"));', 'x.c')
        self.assertEqual(raw[0]['text'], 'Ready')

    def test_format_and_data_are_separate(self):
        raw, _ = analyze('lv_label_set_text_fmt(x,"Count: %u", n);'
                         'lv_label_set_text_fmt(x,"%02u / %02u", a,b);'
                         'lv_label_set_text(x,"USD");'
                         'lv_label_set_text(x,"→");', 'x.c')
        self.assertEqual([i['text'] for i in raw], ['Count: %u'])

    def test_notice_key_is_not_display_copy(self):
        raw, _ = analyze('ui_notice_post(UI_NOTICE_ERROR,"task.key",ui_tr("Failed"),"Retry later");', 'x.c')
        self.assertEqual([i['text'] for i in raw], ['Retry later'])

    def test_deferred_key_sinks_require_markers(self):
        raw, _ = analyze('ui_notice_post_text(UI_NOTICE_ERROR,"task.key",UI_N_("Failed"),UI_N_("Retry"));'
                         'settings_detail_action_block(button,UI_N_("Waiting"));'
                         'settings_detail_action_block(button,"Unmarked");', 'x.c')
        self.assertEqual([i['text'] for i in raw], ['Unmarked'])

if __name__ == '__main__':
    unittest.main()
