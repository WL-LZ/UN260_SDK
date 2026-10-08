#!/usr/bin/env python3
"""Add reviewed term rows to the owned fragment, preserving other catalogues."""
import argparse
import json
import gen_i18n
from audit_four_languages import LANGUAGES, read_draft

# Reviewed neutral Arabic count labels; their wording does not inflect the noun
# according to the count. Do not extend this set without reviewing the copy.
ARABIC_NEUTRAL_COUNTS = {
    '%u notes', ' · %u runs', '%d notes · denomination summary',
    '%s  %s / %u notes', '%s / %u notes', '%u currencies', '%u minutes',
    '%u notes to check', '%u notes%s',
    '%u undated groups: set the business date.',
    'Review %u pending counts', 'Selected: %u minutes',
}



def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    path = gen_i18n.I18N / 'fragments/four_language_common.json'
    previous = gen_i18n.read_json(path) if path.exists() else {}
    _, catalog = gen_i18n.catalogues()
    terms = read_draft(gen_i18n.I18N / 'four_language_terms.tsv')
    aliases = gen_i18n.read_json(gen_i18n.I18N / 'four_language_aliases.json')
    # Some historical Korean entries are English placeholders, not translations.
    # Replace only exact English copies with explicitly reviewed draft wording,
    # in their existing owner, rather than creating conflicting fragment entries.
    corrections = {}
    for tag in ('zh-Hans', 'ko'):
        reviewed = dict(terms[tag])
        for key, source in aliases.items():
            if catalog['en'].get(key) != source:
                raise ValueError('Reviewed alias changed; review its meaning: ' + key)
            if source in reviewed:
                reviewed[key] = reviewed[source]
        corrections[tag] = {
            key: text for key, text in reviewed.items()
            if isinstance(catalog['en'].get(key), str)
            and catalog[tag].get(key) == catalog['en'][key]
            and text != catalog['en'][key]
        }
    corrected = 0
    for owner in [*(gen_i18n.I18N / 'locales').glob('*.json'),
                  *sorted((gen_i18n.I18N / 'fragments').glob('*.json'))]:
        content = gen_i18n.read_json(owner)
        changed = False
        for tag, entries in content.items():
            for key, text in corrections.get(tag, {}).items():
                if key in entries:
                    entries[key] = text
                    catalog[tag][key] = text
                    if owner == path:
                        previous[tag][key] = text
                    changed = True
                    corrected += 1
        if changed:
            if args.check:
                raise SystemExit('Reviewed English placeholders remain in ' + str(owner))
            gen_i18n.write(owner, gen_i18n.json_text(content))
    output = {}
    for tag in LANGUAGES:
        owned = dict(previous.get(tag, {}))
        for key, text in terms[tag].items():
            if key not in catalog['en']:
                continue
            if key in catalog.get(tag, {}) and key not in owned:
                continue
            if isinstance(catalog['en'][key], dict):
                # This neutral count-label phrasing is identical for every
                # category. Other Arabic plural copy needs explicit review.
                if tag == 'ar' and key not in ARABIC_NEUTRAL_COUNTS:
                    existing = owned.get(key)
                    required = {'zero','one','two','few','many','other'}
                    if not isinstance(existing, dict) or not required.issubset(existing):
                        raise ValueError('Arabic plural needs all six reviewed forms: ' + key)
                    # A one-cell term sheet cannot replace six reviewed forms.
                    text = dict(existing)
                else:
                    forms = ('zero','one','two','few','many','other') if tag == 'ar' else ('other',)
                    text = {form: text for form in forms}
            owned[key] = text
        for key, source in aliases.items():
            if catalog['en'].get(key) != source:
                raise ValueError('Reviewed alias changed; review its meaning: ' + key)
            text = owned.get(source, catalog.get(tag, {}).get(source))
            if text and (key not in catalog.get(tag, {}) or key in owned):
                owned[key] = text.get('other') if isinstance(text, dict) else text
        output[tag] = dict(sorted(owned.items()))
    result = gen_i18n.json_text(output)
    if args.check:
        if not path.exists() or path.read_text(encoding='utf-8') != result:
            raise SystemExit('Four-language fragment is stale; run sync_four_language_terms.py')
    else:
        gen_i18n.write(path, result)
    print('Four-language owned fragment: ' + ', '.join('%s=%d' % (tag,len(entries)) for tag,entries in output.items()))
    print('Reviewed English placeholders replaced: %d' % corrected)


if __name__ == '__main__':
    main()
