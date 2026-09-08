#!/usr/bin/env python3
"""Create strict file-level deltas from two validated full UN260 packages.
No binary patching; deletions explicit. The pinned baseline never advances here.
"""
import argparse, hashlib, io, json, os, pathlib, re, tarfile

def sha(data): return hashlib.sha256(data).hexdigest()
def load(path):
    with tarfile.open(path, 'r:gz') as t:
        names=set(); files={}
        for m in t.getmembers():
            n=m.name.rstrip('/')
            if n in names or '..' in n or n.startswith('/') or not re.fullmatch(r'[A-Za-z0-9_./-]+',n):
                raise ValueError('Unsafe/duplicate archive member: '+n)
            names.add(n)
            if not (m.isfile() or m.isdir()): raise ValueError('Unsupported member type')
            if m.isfile():
                if m.size>24*1024*1024: raise ValueError('Oversized member')
                files[n]=(t.extractfile(m).read(),m.mode & 0o777)
        manifest=dict(x.split('=',1) for x in files['manifest.ini'][0].decode().splitlines())
        if manifest.get('schema')!='1' or manifest.get('product')!='UN260': raise ValueError('Baseline/target must be full UN260 packages')
        checks=files['checksums.sha256'][0]
        if sha(checks)!=manifest['package_id']: raise ValueError('Bad package ID')
        expected={}
        for line in checks.decode().splitlines():
            h,n=line.split()
            if n in expected or n not in files or sha(files[n][0])!=h: raise ValueError('Bad payload checksum')
            expected[n]=h
        payload={n[8:]:v for n,v in files.items() if n.startswith('payload/')}
        if set(expected)!={'payload/'+n for n in payload}: raise ValueError('Payload/checksum mismatch')
        for n,(data,mode) in payload.items():
            if mode not in (0o644,0o755): raise ValueError('Unsupported mode')
        if sum(len(v[0]) for v in payload.values())>24*1024*1024: raise ValueError('Payload budget exceeded')
        return manifest,payload

def table(files):
    return ''.join(f'{sha(d)}|{m:04o}|{len(d)}|{p}\n' for p,(d,m) in sorted(files.items())).encode()

def build(base,full,out):
    bm,b=load(base); fm,f=load(full)
    changed={p:v for p,v in f.items() if b.get(p)!=v}
    removed=sorted(set(b)-set(f))
    bt,ft=table(b),table(f)
    checks=''.join(f'{sha(d)}  payload/{p}\n' for p,(d,m) in sorted(changed.items())).encode()
    install=''.join(f'file|{m:04o}|{p}\n' for p,(d,m) in sorted(changed.items()))
    install+=''.join(f'delete|{b[p][1]:04o}|{p}\n' for p in removed)
    meta=dict(format='UN260_UPGRADE',schema='2',product='UN260',package_type='ui-delta',
              version=fm['version'],package_id=sha(checks),baseline_id=sha(bt),target_id=sha(ft),requires_reboot='1')
    entries={'manifest.ini':('\n'.join(f'{k}={v}' for k,v in meta.items())+'\n').encode(),
             'checksums.sha256':checks,'install.tsv':install.encode(),'baseline.tsv':bt,'target.tsv':ft}
    out=pathlib.Path(out);out.parent.mkdir(parents=True,exist_ok=True)
    tmp=out.with_suffix(out.suffix+'.tmp')
    with tarfile.open(tmp,'w:gz',format=tarfile.USTAR_FORMAT) as t:
        for n,data in entries.items():
            m=tarfile.TarInfo(n);m.size=len(data);m.mode=0o644;t.addfile(m,io.BytesIO(data))
        m=tarfile.TarInfo('payload');m.type=tarfile.DIRTYPE;m.mode=0o755;t.addfile(m)
        for p,(data,mode) in sorted(changed.items()):
            m=tarfile.TarInfo('payload/'+p);m.size=len(data);m.mode=mode;t.addfile(m,io.BytesIO(data))
    os.replace(tmp,out)
    out.with_suffix(out.suffix+'.sha256').write_text(f'{sha(out.read_bytes())}  {out.name}\n')
    full_report=out.parent/'UN260_RELEASE.json'
    full_report.write_text(json.dumps(dict(version=fm['version'],baseline=str(pathlib.Path(base).resolve()),
        baseline_id=sha(bt),target_id=sha(ft),changed=list(changed),deleted=removed,
        unchanged=len(f)-len(changed),payload_bytes=sum(len(v[0]) for v in changed.values()),
        files=[dict(path=p,sha256=sha(d),mode=f'{m:04o}',size=len(d)) for p,(d,m) in sorted(f.items())]),indent=2)+'\n')
    print(f'DELTA: changed={len(changed)} deleted={len(removed)} unchanged={len(f)-len(changed)} output={out}')

if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--base',required=True);ap.add_argument('--full',required=True);ap.add_argument('--output',required=True)
    a=ap.parse_args();build(a.base,a.full,a.output)
