// Rasterize the approved Phosphor regular SVG sources once for LVGL.
// No new geometry is synthesized; LICENSE.phosphor accompanies these sources.
const fs=require('node:fs'),path=require('node:path');
const [modules,outputDirectory,sourceDirectory=__dirname]=process.argv.slice(2);
if(!modules || !outputDirectory)
 throw Error('Usage: node generate-icons.cjs <node_modules> <PNG output directory> [SVG source directory]');
const sharp=require(path.join(path.resolve(modules),'sharp'));
const out=path.resolve(outputDirectory),sources=path.resolve(sourceDirectory);
const variants=[['layout',28],['gesture',28],['popup',28],['touch',28],['pure',28],['export',28],['qr',28],['standby',28],['settings',22],['arrange',20],['up',16],['arrow',16],['info',14],['drag',24],['close',18]];
fs.mkdirSync(out,{recursive:true});
(async()=>{
 for(const [name,size] of variants){
  const svg=fs.readFileSync(path.join(sources,name+'.svg'));
  await sharp(svg,{density:384}).resize(size,size).png().toFile(path.join(out,name+'.png'));
 }
 console.log(`Exported ${variants.length} licensed SVG / PNG icons.`);
})();
