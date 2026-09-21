/* First Menu concept SVG originals. PNGs are checked in for offline SDK builds.
 * Regenerate with Node + sharp (0.34.x); no browser/remote artwork required. */
const fs = require('node:fs');
const path = require('node:path');
const sharp = require('sharp');
const output = path.join(__dirname, '../aic_ui/lvgl_data/pin_icons');
fs.mkdirSync(output, {recursive:true});
Promise.all(['lock','close','erase','shield'].map(async name => {
  await sharp(path.join(__dirname, 'pin_icons', name+'.svg'))
    .png().toFile(path.join(output,name+'.png'));
})).catch(error => {console.error(error);process.exitCode=1;});
