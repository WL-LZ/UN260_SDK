/* Rasterize the approved Lucide vector definitions, not hand-drawn replacements. */
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const sharp=require('sharp');
const ctx={window:{}};vm.runInNewContext(fs.readFileSync(path.join(__dirname,'icons.js'),'utf8'),ctx);
const out=path.resolve(__dirname,'../../../aic_ui/lvgl_data/settings_icons');fs.mkdirSync(out,{recursive:true});
(async()=>{for(const name of ['Settings','Layers','Wrench','Database','ShieldCheck','ChevronRight','Languages','Clock','Sun','Check']){
 const shapes=ctx.window.SETTINGS_ICONS[name].map(([tag,attrs])=>`<${tag} ${Object.entries(attrs).map(([k,v])=>`${k}="${v}"`).join(' ')}/>`).join('');
 for(const active of [false]){const color='#536b79';
  const svg=`<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="${color}" color="${color}" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">${shapes}</svg>`;
  await sharp(Buffer.from(svg),{density:288}).resize(24,24).png().toFile(path.join(out,name+(active?'-active':'')+'.png'));
 }
}console.log('10 approved vector-derived PNG icons; active color is applied by LVGL');})();
