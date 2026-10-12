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
        exceptions = {'invariant_sources': [], 'same_spelling': {}}
        result = audit.audit(registry, catalogues, exceptions)
        self.assertEqual(len(result['locales']['ko']['findings']), 3)
        self.assertEqual(len(result['locales']['de']['findings']), 1)

    def test_partial_english_clause_is_rejected(self):
        registry = [{'tag': 'en'}, {'tag': 'tr'}]
        catalogues = {'en': {'guide': 'Clear request not sent. Retry.'},
                      'tr': {'guide': 'Clear request not sent. Yeniden deneyin.'}}
        result = audit.audit(registry, catalogues, {'invariant_sources': [], 'same_spelling': {}})
        self.assertEqual(result['locales']['tr']['findings'][0]['reason'], 'unchanged English clause')

    def test_technical_paths_and_same_spelling_words_survive(self):
        registry = [{'tag': 'en'}, {'tag': 'ko'}, {'tag': 'fr'}]
        catalogues = {'en': {'protocol': 'UV', 'path': 'Insert update/UN260_UPDATE.upk', 'word': 'Date'},
                      'ko': {'protocol': 'UV', 'path': 'update/UN260_UPDATE.upk를 넣으세요.', 'word': '날짜'},
                      'fr': {'protocol': 'UV', 'path': 'Insérer update/UN260_UPDATE.upk', 'word': 'Date'}}
        exceptions = {'invariant_sources': ['UV'], 'same_spelling': {'fr': ['Date']}}
        result = audit.audit(registry, catalogues, exceptions)
        self.assertFalse(result['locales']['ko']['findings'])
        self.assertFalse(result['locales']['fr']['findings'])

if __name__ == '__main__':
    unittest.main()
