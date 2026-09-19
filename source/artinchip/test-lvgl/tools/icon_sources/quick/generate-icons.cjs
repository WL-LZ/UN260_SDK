// Established Lucide 1.8.0 geometry; no font glyphs or hand-drawn substitutes.
const fs=require('node:fs'),path=require('node:path');
const [modules,outputDirectory,sourceDirectory]=process.argv.slice(2);
if(!modules || !outputDirectory || !sourceDirectory)
 throw Error('Usage: node generate-icons.cjs <node_modules> <PNG output directory> <SVG source directory>');
const icons=require(path.join(modules,'lucide'));
const sharp=require(path.join(modules,'sharp'));
const out=path.resolve(outputDirectory);fs.mkdirSync(out,{recursive:true});
const sources=path.resolve(sourceDirectory);fs.mkdirSync(sources,{recursive:true});
const variants=[['layout','PanelsTopLeft',28],['gesture','Hand',28],['standby','Images',28],['up','ChevronUp',18],['settings','Settings',24]];
(async()=>{
 for(const [name,id,size] of variants){
  const node=icons[id];if(!Array.isArray(node))throw Error('Missing icon '+id);
  const body=node.map(([tag,attrs])=>`<${tag} ${Object.entries(attrs).map(([k,v])=>`${k}="${v}"`).join(' ')}/>`).join('');
  const svg=`<svg xmlns="http://www.w3.org/2000/svg" width="${size}" height="${size}" viewBox="0 0 24 24" fill="none" stroke="#496574" stroke-width="1.65" stroke-linecap="round" stroke-linejoin="round">${body}</svg>`;
  fs.writeFileSync(path.join(sources,name+'.svg'),svg+'\n');
  await sharp(Buffer.from(svg),{density:384}).resize(size,size).png().toFile(path.join(out,name+'.png'));
 }
 fs.copyFileSync(path.join(modules,'lucide/LICENSE'),path.join(sources,'LICENSE.lucide'));
 console.log(`Exported ${variants.length} licensed SVG / PNG icons.`);
})();
