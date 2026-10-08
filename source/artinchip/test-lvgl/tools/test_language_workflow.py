#!/usr/bin/env python3
"""Translation maintenance regression tests using isolated catalogs."""
import copy
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace as Args
import unittest
from unittest.mock import patch
import language_workflow as w


class Workflow(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.registry = [dict(id=0, tag='en'), dict(id=4, tag='ar', plural_forms=['one','other'])]
        self.catalogs = {'en': {'Amount %u': 'Amount %u', 'Notes': {'other': '%u notes'}},
                         'ar': {'Amount %u': 'قيمة %u', 'Notes': {'one':'%u ورقة', 'other':'%u أوراق'}}}
        (self.root / 'locales').mkdir(); (self.root / 'fragments').mkdir()
        self.write('locales.json', self.registry)
        self.write('locales/ar.json', {'ar': {}})
        self.write('fragments/test.json', {'ar': self.catalogs['ar'], 'en': self.catalogs['en']})
        self.write('font_profiles.json', {'latin': {'source':'test.ttf'}})
        self.patches = [patch.object(w.g, 'I18N', self.root),
                        patch.object(w.g, 'catalogues', side_effect=lambda: (copy.deepcopy(self.registry), copy.deepcopy(self.catalogs)))]
        for p in self.patches: p.start()
        self.args = Args(locale='ar', file=self.root / 'translation.json')
        w.export(self.args)

    def tearDown(self):
        for p in self.patches: p.stop()
        self.temp.cleanup()

    def write(self, path, value):
        (self.root / path).write_text(json.dumps(value, ensure_ascii=False), encoding='utf-8')

    def invalid(self, mutate):
        data = w.g.read_json(self.args.file); mutate(data)
        self.write('translation.json', data)
        before = (self.root / 'fragments/test.json').read_bytes()
        with self.assertRaises(ValueError): w.import_translation(self.args)
        self.assertEqual(before, (self.root / 'fragments/test.json').read_bytes())

    def test_roundtrip_preserves_owner_and_other_locales(self):
        w.import_translation(self.args)
        self.assertEqual(w.g.read_json(self.root / 'locales/ar.json'), {'ar':{}})
        self.assertEqual(w.g.read_json(self.root / 'fragments/test.json')['en'], self.catalogs['en'])
        self.assertFalse(w.g.read_json(self.root / 'locales.json')[1]['enabled'])

    def test_wrong_parameter(self):
        self.invalid(lambda d: d['entries']['Amount %u'].update(translation='قيمة %s'))

    def test_missing_plural(self):
        self.invalid(lambda d: d['entries']['Notes'].update(translation={'other':'%u أوراق'}))

    def test_stale_source(self):
        self.invalid(lambda d: d.update(source_sha256='stale'))

    def test_missing_key(self):
        self.invalid(lambda d: d['entries'].pop('Notes'))

    def test_add_keeps_stable_ids(self):
        w.add(Args(locale='fr', name='Français', code='FR', direction='ltr', font_profile='latin', plural_forms='one,other'))
        registry = w.g.read_json(self.root / 'locales.json')
        self.assertEqual([r['id'] for r in registry], [0,4,5])
        self.assertFalse(registry[-1]['enabled'])

    def test_reject_path_traversal(self):
        with self.assertRaises(ValueError):
            w.add(Args(locale='../bad', name='bad', code='BAD', direction='ltr', font_profile='latin', plural_forms='other'))


if __name__ == '__main__': unittest.main()
