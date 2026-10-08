#!/usr/bin/env python3
"""Audit release catalogues and the separate four-language translation draft.

Reviewed draft rows are synchronized to owned catalogue entries. Counts are
translation coverage, not evidence of font, shaping or layout readiness.
"""
import argparse
import csv
import json
from pathlib import Path
import gen_i18n

LANGUAGES = ('zh-Hans', 'ko', 'ja', 'ar')
ARABIC_FORMS = {'zero', 'one', 'two', 'few', 'many', 'other'}


def read_draft(path):
    result = {tag: {} for tag in LANGUAGES}
    with path.open(encoding='utf-8', newline='') as source:
        rows = csv.DictReader(source, delimiter='\t')
        if rows.fieldnames != ['key', *LANGUAGES]:
            raise ValueError('Expected key and four locale columns')
        for number, row in enumerate(rows, 2):
            key = row['key']
            if not key or key in result['zh-Hans']:
                raise ValueError('Empty/duplicate draft key at line %d' % number)
            if None in row or any(row[tag] is None or not row[tag].strip() for tag in LANGUAGES):
                raise ValueError('Missing/extra draft column at line %d' % number)
            expected = gen_i18n.format_args(key)
            for tag in LANGUAGES:
                text = row[tag]
                if gen_i18n.format_args(text, strict=bool(expected)) != expected:
                    raise ValueError('%s: draft placeholder mismatch: %s' % (tag, key))
                if any(ord(char) < 32 and char != '\n' for char in text):
                    raise ValueError('%s: unexpected control in draft: %s' % (tag, key))
                result[tag][key] = text
    return result


def audit(registry, catalogues, draft):
    english = catalogues['en']
    result = {'source_keys': len(english), 'keyboard_policy': 'English', 'locales': {}}
    for tag in LANGUAGES:
        entries = catalogues.get(tag, {})
        missing = sorted(key for key in english if not entries.get(key))
        draft_only = sorted(set(draft[tag]) - set(english))
        conflicts = sorted(key for key, value in draft[tag].items()
                           if entries.get(key) and entries[key] != value)
        incomplete_plurals = sorted(key for key, value in entries.items()
                                    if tag == 'ar' and isinstance(value, dict)
                                    and (ARABIC_FORMS - {k for k, v in value.items() if v}))
        locale = next((item for item in registry if item['tag'] == tag), None)
        result['locales'][tag] = {
            'registered': locale is not None,
            'enabled': bool(locale and locale['enabled']),
            'translated_entries': len(entries),
            'missing_count': len(missing), 'missing_keys': missing,
            'draft_entries': len(draft[tag]),
            'draft_new_keys': draft_only,
            'draft_existing_differences': conflicts,
            'incomplete_plural_keys': incomplete_plurals,
            'font_ready': bool(locale and locale['font_ready']),
            'shaping_ready': bool(locale and locale['shaping_ready']),
            'rendered_layout_verified': False,
        }
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--require-resources', action='store_true',
                        help='Require complete released catalogues; does not validate rendered layouts')
    args = parser.parse_args()
    registry, catalogues = gen_i18n.catalogues()
    draft = read_draft(gen_i18n.I18N / 'four_language_terms.tsv')
    report = audit(registry, catalogues, draft)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes((json.dumps(report, ensure_ascii=False, indent=2) + '\n').encode('utf-8'))
    for tag, item in report['locales'].items():
        print('%s: translated=%d missing=%d draft=%d enabled=%s' %
              (tag, item['translated_entries'], item['missing_count'], item['draft_entries'], item['enabled']))
    if args.require_resources and any(not item['enabled'] or item['missing_count']
                            or item['incomplete_plural_keys'] or not item['font_ready']
                            or not item['shaping_ready'] for item in report['locales'].values()):
        raise SystemExit('Four-language release NOT ready; see audit report')


if __name__ == '__main__':
    main()
