#!/usr/bin/env python3
"""Review English leftovers in all registered release dictionaries.

Checks unchanged English and Latin words in CJK/Arabic translations; explicitly
reviewed same-spelling words, protocol labels, paths and filenames remain data.
This is a regression audit, not a semantic translation or on-device layout proof.
"""
import argparse
import json
from pathlib import Path
import re
import gen_i18n as g

SCRIPT = {'zh-Hans': r'[\u3400-\u9fff]', 'ko': r'[\uac00-\ud7af]',
          'ja': r'[\u3040-\u30ff\u3400-\u9fff]', 'ar': r'[\u0600-\u06ff]'}
TECHNICAL_WORDS = {
    'UN260', 'USB', 'SD', 'UART', 'LVGL', 'CPU', 'RAM', 'FPS', 'FPGA', 'PNG', 'CSV',
    'MDC', 'SDC', 'CNT', 'MUL', 'AUT', 'RJ', 'ST', 'CIS', 'UV', 'IR', 'MG', 'MT',
    'UI', 'SN', 'PCS', 'RGB', 'CMY', 'TX', 'RX', 'MB', 'KB', 'ms', 'px', 'Linux', 'LINUX', 'inode',
    'ADD', 'BATCH', 'SORT', 'MULTI', 'PIN', 'QR', 'ID', 'DDR', 'NAND', 'RISC-V',
    'OCR', 'IMG', 'QT', 'SAFE', 'FO', 'SOT', 'OVD', 'UPK', 'UVUP', 'UVDOWN',
    'CNY', 'USD', 'EUR', 'GBP', 'KRW', 'EGP', 'ISK', 'PHP', 'SOS', 'TRY',
    'AED', 'SAR', 'OMR', 'QAR', 'MAD', 'DZD', 'INR', 'PKR', 'IQD', 'FD', 'DF',
    'Aa', 'OFF', 'HEX', 'YYYY', 'YYYYMM', 'YYYYMMDD', 'HH', 'HHMM', 'HHMMSS',
}

def normal(text):
    return text.casefold().strip()

def technical(source, exceptions):
    if source in exceptions['invariant_sources']:
        return True
    if re.fullmatch(r'IMG F(?:[1-9]|1[0-5])|\d+ FPS', source):
        return True
    return bool(re.fullmatch(r'(CPU|FPS|LOOP|LVGL|MAIN|MEM|RX|TX):[^\n]*', source))

def words(text):
    text = g.FORMAT.sub('', text)
    text = re.sub(r'#[0-9a-fA-F]{6}\s|(?<![A-Za-z0-9])[0-9A-F]{6}(?![A-Za-z0-9])', '', text)
    text = re.sub(r'\b[\w./-]+\.(?:png|upk|txt|csv|bin|cfg)\b', '', text)
    text = re.sub(r'(?:/[\w./-]+|\b[\w.-]+/[\w./-]+)', '', text)
    return re.findall(r'[A-Za-z][A-Za-z0-9_-]+', text)

def untranslated_clauses(source, translated, tag, exceptions):
    source = g.FORMAT.sub('', source)
    source = re.sub(r'#[0-9a-fA-F]{6}\s', '', source)
    source = re.sub(r'\b[\w./-]+\.(?:png|upk|txt|csv|bin|cfg)\b', '', source)
    allowed = {normal(x) for x in exceptions.get('same_clauses', {}).get(tag, [])}
    findings = []
    for clause in re.split(r'[.!?;\n]| / | [|] ', source):
        clause = clause.strip()
        parts = re.findall(r'[A-Za-z]{2,}', clause)
        if len(parts) < 2 or normal(clause) in allowed or set(parts) <= TECHNICAL_WORDS:
            continue
        pattern = r'(?<![A-Za-z])' + re.escape(clause) + r'(?![A-Za-z])'
        if re.search(pattern, translated, re.I):
            findings.append(clause)
    return findings

def audit(registry, catalogues, exceptions):
    report = {'source_keys': len(catalogues['en']), 'locales': {}}
    for locale in registry:
        tag = locale['tag']
        if tag == 'en':
            continue
        accepted = {normal(x) for x in exceptions['same_spelling'].get(tag, [])}
        findings, preserved = [], []
        for key, source_value in catalogues['en'].items():
            source = source_value['other'] if isinstance(source_value, dict) else source_value
            translated_value = catalogues[tag].get(key)
            variants = translated_value.items() if isinstance(translated_value, dict) else [('singular', translated_value)]
            for form, text in variants:
                if not text:
                    findings.append({'key': key, 'form': form, 'source': source, 'translation': text, 'reason': 'missing'})
                    continue
                if technical(source, exceptions):
                    preserved.append({'key': key, 'source': source, 'reason': 'protocol, diagnostic format or keyboard identity'})
                    continue
                if normal(source) in accepted and normal(text) == normal(source):
                    preserved.append({'key': key, 'source': source, 'reason': 'reviewed same-spelling word in this language'})
                    continue
                latin = words(text)
                if not latin:
                    continue
                if normal(text) == normal(source):
                    findings.append({'key': key, 'form': form, 'source': source, 'translation': text, 'reason': 'unchanged English'})
                elif tag not in SCRIPT and untranslated_clauses(source, text, tag, exceptions):
                    findings.append({'key': key, 'form': form, 'source': source, 'translation': text, 'reason': 'unchanged English clause', 'clauses': untranslated_clauses(source, text, tag, exceptions)})
                elif tag in SCRIPT:
                    unknown = sorted(w for w in set(latin) - TECHNICAL_WORDS if not re.fullmatch(r'(PS|F|MG|OCR)\d+[LR]?', w))
                    if unknown:
                        findings.append({'key': key, 'form': form, 'source': source, 'translation': text, 'reason': 'Latin words in translation', 'words': unknown})
        report['locales'][tag] = {'checked': len(catalogues['en']), 'findings': findings, 'preserved': preserved}
    return report

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    registry, catalogues = g.catalogues()
    exceptions = g.read_json(g.I18N / 'review/translation_language_exceptions.json')
    report = audit(registry, catalogues, exceptions)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(g.json_text(report), encoding='utf-8')
    total = 0
    for tag, result in report['locales'].items():
        print('%s: %d keys, %d English leftovers, %d reviewed preserved rows' %
              (tag, result['checked'], len(result['findings']), len(result['preserved'])))
        total += len(result['findings'])
        for finding in result['findings'][:25]:
            print(' ', json.dumps(finding, ensure_ascii=False))
    if args.check and total:
        raise SystemExit(1)

if __name__ == '__main__':
    main()
