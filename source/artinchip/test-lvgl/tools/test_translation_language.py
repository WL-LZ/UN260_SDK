#!/usr/bin/env python3
import unittest
from pathlib import Path
import audit_translation_language as audit
import gen_i18n as g

class TranslationLanguageAudit(unittest.TestCase):
    def test_current_catalogues_have_no_unreviewed_english(self):
        registry, catalogues = g.catalogues()
        exceptions = g.read_json(g.I18N / 'review/translation_language_exceptions.json')
        result = audit.audit(registry, catalogues, exceptions)
        self.assertFalse({tag: item['findings'] for tag, item in result['locales'].items() if item['findings']})

    def test_visible_actions_do_not_force_english(self):
        # Latin character keys and data stay unchanged; visible operations use
        # the active locale, including callers configuring shared keyboards.
        forced = []
        for path in (g.ROOT / 'un260').rglob('*'):
            if path.suffix in ('.c', '.h', '.inc'):
                for number, line in enumerate(path.read_text().splitlines(), 1):
                    if 'ui_tr_for(LANGUAGE_EN' in line:
                        forced.append(f'{path.relative_to(g.ROOT)}:{number}')
        self.assertFalse(forced)

    def test_nonempty_english_and_case_changes_are_not_translations(self):
        registry = [{'tag': 'en'}, {'tag': 'ko'}, {'tag': 'de'}]
        english = {'Title': 'Change password', 'Preset': 'Profile 1', 'Guide': 'Tap Save now.'}
        catalogues = {'en': english, 'ko': {'Title': 'Change Password', 'Preset': 'CUSTOM1', 'Guide': 'Save를 누르세요.'},
                      'de': {'Title': 'CHANGE PASSWORD', 'Preset': 'Profil 1', 'Guide': 'Jetzt speichern.'}}
        exceptions = {'invariant_keys': {}, 'same_spelling': {}}
        result = audit.audit(registry, catalogues, exceptions)
        self.assertEqual(len(result['locales']['ko']['findings']), 3)
        self.assertEqual(len(result['locales']['de']['findings']), 1)

    def test_partial_english_clause_is_rejected(self):
        registry = [{'tag': 'en'}, {'tag': 'tr'}]
        catalogues = {'en': {'guide': 'Clear request not sent. Retry.'},
                      'tr': {'guide': 'Clear request not sent. Yeniden deneyin.'}}
        result = audit.audit(registry, catalogues, {'invariant_keys': {}, 'same_spelling': {}})
        self.assertEqual(result['locales']['tr']['findings'][0]['reason'], 'unchanged English clause')

    def test_french_gesture_word_is_not_hidden_inside_a_native_sentence(self):
        exceptions = g.read_json(g.I18N / 'review/translation_language_exceptions.json')
        source = {'  /  Swipe for more': '  /  Swipe for more', '  /  Swipe to return': '  /  Swipe to return'}
        translated = {'  /  Swipe for more': '  / Swipe pour en savoir plus', '  /  Swipe to return': '  / SWIPE pour revenir'}
        registry = [{'tag': 'en'}, {'tag': 'fr'}]
        result = audit.audit(registry, {'en': source, 'fr': translated}, exceptions)
        self.assertEqual({item['key'] for item in result['locales']['fr']['findings']}, set(source))
        self.assertEqual({item['reason'] for item in result['locales']['fr']['findings']}, {'untranslated operation word'})
        translated = {'  /  Swipe for more': '  / Glissez pour voir plus', '  /  Swipe to return': '  / Glissez pour revenir'}
        result = audit.audit(registry, {'en': source, 'fr': translated}, exceptions)
        self.assertFalse(result['locales']['fr']['findings'])

    def test_native_calendar_short_form_does_not_waive_an_english_verb(self):
        exceptions = g.read_json(g.I18N / 'review/translation_language_exceptions.json')
        source = {'Mar': 'Mar', 'Damage note': 'Mar this note'}
        translated = {'Mar': 'mar', 'Damage note': 'Mar this note'}
        result = audit.audit([{'tag': 'en'}, {'tag': 'pl'}], {'en': source, 'pl': translated}, exceptions)
        self.assertEqual([item['key'] for item in result['locales']['pl']['findings']], ['Damage note'])
        self.assertEqual([item['key'] for item in result['locales']['pl']['preserved']], ['Mar'])

    def test_technical_paths_and_same_spelling_words_survive(self):
        registry = [{'tag': 'en'}, {'tag': 'ko'}, {'tag': 'fr'}]
        catalogues = {'en': {'UI_TEXT_SETTINGS_CFD_LEVEL_UV': 'UV', 'path': 'Insert update/UN260_UPDATE.upk', 'word': 'Date'},
                      'ko': {'UI_TEXT_SETTINGS_CFD_LEVEL_UV': 'UV', 'path': 'update/UN260_UPDATE.upk를 넣으세요.', 'word': '날짜'},
                      'fr': {'UI_TEXT_SETTINGS_CFD_LEVEL_UV': 'UV', 'path': 'Insérer update/UN260_UPDATE.upk', 'word': 'Date'}}
        exceptions = {'invariant_keys': {'UI_TEXT_SETTINGS_CFD_LEVEL_UV': 'Reviewed engineering channel with native description'}, 'same_spelling': {'fr': ['Date']}}
        result = audit.audit(registry, catalogues, exceptions)
        self.assertFalse(result['locales']['ko']['findings'])
        self.assertFalse(result['locales']['fr']['findings'])

    def test_business_abbreviations_and_detection_levels_are_not_exempt(self):
        exceptions = g.read_json(g.I18N / 'review/translation_language_exceptions.json')
        registry = [{'tag': 'en'}, {'tag': 'zh-Hans'}, {'tag': 'ko'}]
        keys = {
            'UI_TEXT_PAGE01_BOTTOM_MODE_MDC': 'MDC',
            'UI_TEXT_PAGE01_BOTTOM_MODE_SDC': 'SDC',
            'UI_TEXT_PAGE01_BOTTOM_MODE_CNT': 'CNT',
            'UI_TEXT_PAGE01_BOTTOM_ADD_ON': 'ADD:ON',
            'UI_TEXT_PAGE01_BOTTOM_CFD_FMT': 'CFD:%s',
            'Detection level\004Low': 'Low',
            'Detection level\004Medium': 'Medium',
            'Detection level\004High': 'High',
        }
        untranslated = {key: source for key, source in keys.items()}
        result = audit.audit(registry, {'en': keys, 'zh-Hans': untranslated, 'ko': untranslated}, exceptions)
        for tag in ('zh-Hans', 'ko'):
            self.assertEqual({item['key'] for item in result['locales'][tag]['findings']}, set(keys))
            self.assertFalse(result['locales'][tag]['preserved'])

    def test_engineering_exemption_cannot_cover_reject_reason(self):
        exceptions = g.read_json(g.I18N / 'review/translation_language_exceptions.json')
        source = {'UI_TEXT_SETTINGS_CFD_LEVEL_UV': 'UV', 'UV': 'Ultraviolet', 'UV guide': 'Ultraviolet guide'}
        korean = {'UI_TEXT_SETTINGS_CFD_LEVEL_UV': 'UV', 'UV': 'UV', 'UV guide': 'UV 안내'}
        result = audit.audit([{'tag': 'en'}, {'tag': 'ko'}], {'en': source, 'ko': korean}, exceptions)
        self.assertEqual({item['key'] for item in result['locales']['ko']['findings']}, {'UV', 'UV guide'})
        self.assertEqual([item['key'] for item in result['locales']['ko']['preserved']], ['UI_TEXT_SETTINGS_CFD_LEVEL_UV'])

    def test_number_abbreviation_does_not_waive_negative_no(self):
        exceptions = g.read_json(g.I18N / 'review/translation_language_exceptions.json')
        source = {'No.': 'No.', 'No': 'No'}
        result = audit.audit([{'tag': 'en'}, {'tag': 'tr'}],
                             {'en': source, 'tr': {'No.': 'No.', 'No': 'No'}}, exceptions)
        self.assertEqual([item['key'] for item in result['locales']['tr']['findings']], ['No'])

    def test_numbered_channel_exemption_is_key_scoped(self):
        exceptions = g.read_json(g.I18N / 'review/translation_language_exceptions.json')
        source = {'MG12 waveform': 'MG12 waveform', 'business': 'MG12 result'}
        korean = {'MG12 waveform': 'MG12 파형', 'business': 'MG12 결과'}
        result = audit.audit([{'tag': 'en'}, {'tag': 'ko'}], {'en': source, 'ko': korean}, exceptions)
        self.assertEqual([item['key'] for item in result['locales']['ko']['findings']], ['business'])

if __name__ == '__main__':
    unittest.main()
