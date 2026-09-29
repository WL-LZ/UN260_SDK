/* Execute the actual emitted script against a minimal report DOM. */
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const html = fs.readFileSync(process.argv[2], 'utf8');
const scripts = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)];
assert.equal(scripts.length, 1, 'translation cannot create another script');
const node = (key, value) => ({textContent: value, getAttribute: () => key});
const title = node('Serial Number', 'Serial Number');
const amount = node('Amount', 'Amount');
const raw = node(null, 'Serial Number');
const batch = {textContent:'BAT:200',dataset:{i18nBatch:'200'}};
const off = {textContent:'OFF',dataset:{i18nBatch:'-1'}};
const status = {textContent:'3 matches',dataset:{count:'3'}};
const placeholder = {getAttribute:()=>'Serial Number'};
const context = {
    window: {},
    document: {
        documentElement: {},
        querySelectorAll: selector => ({
            '[data-i18n]':[title,amount],
            '[data-i18n-batch]':[batch,off],
            '[data-i18n-placeholder]':[placeholder]
        })[selector] || [],
        getElementById:id=>id==='searchStatus'?status:null
    }
};
vm.runInNewContext(scripts[0][1],context);
assert.equal(title.textContent,'冠字号');
assert.equal(raw.textContent,'Serial Number','unmarked user data stays unchanged');
assert.equal(placeholder.placeholder,'冠字号');
assert.equal(batch.textContent,'预置：200');
assert.equal(off.textContent,'OFF');
assert.equal(status.textContent,'匹配：3');
assert.equal(context.window.reportMatches(12),'匹配：12');
assert.equal(context.document.documentElement.lang,'zh-Hans');
assert.equal(amount.textContent,"</script><script>alert('translation')</script>");
console.log('report i18n JavaScript: PASS (frozen locale, positional count, marked text only, inert translation data)');
