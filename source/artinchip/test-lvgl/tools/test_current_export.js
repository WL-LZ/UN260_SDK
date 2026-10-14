/* Run both actual emitted scripts: renderer first, then captured-locale lookup. */
const assert=require('node:assert/strict');
const fs=require('node:fs');
const vm=require('node:vm');
const manifest=JSON.parse(fs.readFileSync(process.argv[2],'utf8'));
assert.equal(manifest.length,40);
const historyManifest=JSON.parse(fs.readFileSync(process.argv[3],'utf8'));
assert.equal(historyManifest.length,10);
function attributes(text) {
    const result={};
    for(const m of text.matchAll(/([a-zA-Z][a-zA-Z0-9-]*)(?:="([^"]*)")?/g))result[m[1]]=m[2]??'';
    return result;
}
function element(attrs={},text='') {
    return {textContent:text,dataset:Object.fromEntries(Object.entries(attrs).filter(([k])=>k.startsWith('data-')).map(([k,v])=>[k.slice(5).replace(/-([a-z])/g,(_,c)=>c.toUpperCase()),v])),
        getAttribute:name=>Object.hasOwn(attrs,name)?attrs[name]:null,
        style:{},classList:{toggle(){}},addEventListener(){}};
}
function marked(text) {
    const result=[];
    for(const m of text.matchAll(/<([a-z][a-z0-9]*)\b([^>]*\bdata-i18n(?:="[^"]*")?)(?:\s[^>]*)?>([^<]*)</g)) {
        const attrs=attributes(m[2]);
        if(Object.hasOwn(attrs,'data-i18n'))result.push(element(attrs,m[3]));
    }
    return result;
}
for(const item of [...manifest,...historyManifest]) {
    const html=fs.readFileSync(item.path,'utf8');
    const scripts=[...html.matchAll(/<script>([\s\S]*?)<\/script>/g)];
    assert.equal(scripts.length,2,'producer and locale scripts');
    const body=html.replace(/<script>[\s\S]*?<\/script>/g,'').replace(/<style>[\s\S]*?<\/style>/g,'');
    const initial=marked(body),reject={innerHTML:''};
    let renderedNodes=[];
    const status=element({'data-count':'0'});
    const rows=[...html.matchAll(/<tr data-sn="([^"]*)">/g)].map(m=>element({'data-sn':m[1]}));
    const input={value:'',addEventListener(){}};
    const placeholders=[...body.matchAll(/<input\b([^>]*data-i18n-placeholder="[^"]*"[^>]*)>/g)].map(m=>element(attributes(m[1])));
    const batches=[...body.matchAll(/<span\b([^>]*data-i18n-batch="[^"]*"[^>]*)>([^<]*)<\/span>/g)].map(m=>element(attributes(m[1]),m[2]));
    const callbacks=[];
    const context={window:{addEventListener(){}},performance:{now:()=>0},
        requestAnimationFrame:callback=>callbacks.push(callback),
        document:{documentElement:{},
            querySelectorAll:selector=>selector==='.counter'?[]:(selector==='#snTableBody tr'||selector==='#snTableBody tr[data-sn]')?rows:
                selector==='[data-i18n]'?[...initial,...renderedNodes]:
                selector==='[data-i18n-batch]'?batches:selector==='[data-i18n-placeholder]'?placeholders:[],
            getElementById:id=>({rejectContent:reject,snSearch:input,searchStatus:status,noMatch:element()})[id]||null}};
    vm.runInNewContext(scripts[0][1],context);
    const rendered=reject.innerHTML;
    renderedNodes=marked(rendered);
    const history=item.kind==='history';
    assert.match(rendered,history||item.scenario===1||item.scenario===2?/data-i18n="No\."/:/No rejected notes/);
    assert(!/data-i18n="No">No<\/th>/.test(rendered),'ordinal header must not use negation No');
    assert(!/suspectNotes|damagedNotes|Excellent|All notes passed validation successfully/.test(scripts[0][1]),
        'no inferred counterfeit/damage classification or authenticity verdict');
    assert(!/Suspect Notes|Damaged Notes|Excellent|All notes passed validation/.test(rendered));
    if(history)assert.match(scripts[0][1],/savedRejectPcs:6/);
    else assert.match(scripts[0][1],new RegExp('totalAmount:425,totalNotes:85,rejectCount:'+item.rejectCount));
    if(!history)assert.match(rendered,/data-i18n="Rejected"/);
    assert(rendered.includes('>'+item.rejectCount+'</'), 'actual total stays visible, even if detail is incomplete');
    if(!history&&(item.scenario===0||item.scenario===3)) {
        assert.match(rendered,/No rejected notes/);
        assert.match(rendered,/No reject handling is needed. This is not an authenticity verdict./);
        assert(!rendered.includes('<table>'));
    } else if(item.scenario===1) {
        const data=[...scripts[0][1].matchAll(/\{no:(\d+),pcs:(\d+),reason:"([^"]*)"\}/g)].map(m=>[+m[1],+m[2],m[3]]);
        assert.deepEqual(data,history?[[4,2,'Double 1'],[9,1,'Version Unknow'],[20,3,'OCR1 Err']]:[[1,2,'Double 1'],[2,1,'Version Unknown'],[3,3,'OCR1 Err']]);
        assert(rendered.indexOf('Double 1')<rendered.indexOf(history?'Version Unknow':'Version Unknown'));
        assert(rendered.indexOf(history?'Version Unknow':'Version Unknown')<rendered.indexOf('OCR1 Err'));
    } else {
        assert(!rendered.includes('No rejected notes'),'missing detail must not imply zero rejects');
        assert.equal((rendered.match(/<tr>/g)||[]).length,1,'only header, no fabricated detail rows');
    }
    vm.runInNewContext(scripts[1][1],context);
    assert.equal(context.document.documentElement.lang,item.tag);
    assert.equal(context.document.documentElement.dir,item.tag==='ar'?'rtl':'ltr');
    for(const [key,value] of Object.entries(item.expected)) {
        assert.equal(context.window.reportTr(key),value,'actual captured locale: '+key);
    }
    assert(initial.some(n=>n.textContent===item.expected['No.']), 'static serial ordinal header uses native No.');
    if(history||item.scenario===1||item.scenario===2)
        assert(renderedNodes.some(n=>n.textContent===item.expected['No.']), 'dynamic reject ordinal header uses native No.');
    if(!history) {
        const mode=['Mixed denominations','Single denomination','Piece counting','None'][item.scenario];
        assert(initial.some(n=>n.textContent===item.expected[mode]),'mode badge is translated at its visible boundary');
        assert(initial.some(n=>n.textContent===item.expected.Rejected),'Rejected label is translated');
        if(item.tag!=='en')assert.notEqual(item.expected[mode],mode,'mode label requires native explanation');
        assert.deepEqual(rows.map(r=>r.dataset.sn),['Z90','AB2','JK3'],'serial user data/order remains intact');
    }
}
const csv=fs.readFileSync(require('node:path').join(require('node:path').dirname(process.argv[2]),'stable.csv'),'utf8');
assert.match(csv,/DENOM,PCS,AMOUNT\n5,85,425/);
assert.match(csv,/NO,SN,DENOM\n1,Z90,5\n2,AB2,5\n3,JK3,5/);
assert.match(csv,/NO,PCS,REASON\n1,2,Double 1\n2,1,Version Unknown\n3,3,OCR1 Err/);
console.log('current report emitted JavaScript: PASS (50 current/history reports; native ordinal headers/mode badges, frozen locale/RTL, real rejects, serial/order/schema invariants)');
