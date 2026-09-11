#!/usr/bin/env node
'use strict';
// Optional maintenance script. Firmware make consumes the committed PNG assets.
// Pass a node_modules directory containing @phosphor-icons/react 2.1.10,
// react and react-dom; sharp is resolved through the usual NODE_PATH.
const fs = require('node:fs/promises');
const path = require('node:path');
const assert = require('node:assert/strict');
const sharp = require('sharp');
const modules = path.resolve(process.argv[2] || 'node_modules');
const React = require(path.join(modules, 'react'));
const {renderToStaticMarkup} = require(path.join(modules, 'react-dom/server'));
const {pathToFileURL} = require('node:url');
const out = path.resolve(__dirname, '../../../aic_ui/lvgl_data/main_icons');
const variants = [
  ['cube', 'Cube', 28], ['gear', 'GearSix', 28], ['list', 'ListBullets', 28],
  ['printer', 'Printer', 28], ['menu', 'SquaresFour', 28],
  ['play', 'Play', 28, '#26810A'], ['clear', 'ArrowCounterClockwise', 28],
  ['stack', 'Stack', 20], ['caret_down', 'CaretDown', 16],
  ['currencies', 'CurrencyCircleDollar', 32, '#0074F8'],
  ['currencies', 'CurrencyCircleDollar', 56, '#0074F8'],
];
(async () => {
  const icons = await import(pathToFileURL(path.join(modules, '@phosphor-icons/react/dist/index.es.js')).href);
  await fs.mkdir(out, {recursive:true});
  for (const [name, icon, size, color = '#657F90'] of variants) {
    assert.ok(icons[icon], icon);
    const svg = renderToStaticMarkup(React.createElement(icons[icon], {
      size:256, weight:'light', color,
    }));
    const png = await sharp(Buffer.from(svg)).resize(size, size).png().toBuffer();
    const metadata = await sharp(png).metadata();
    assert.equal(metadata.width, size);
    assert.equal(metadata.height, size);
    assert.equal(metadata.hasAlpha, true);
    await fs.writeFile(path.join(__dirname, `${name}_light.svg`), svg + '\n');
    await fs.writeFile(path.join(out, `${name}_${size}.png`), png);
  }
  const multiSvg = await fs.readFile(path.join(__dirname, 'currencies_light.svg'));
  await sharp(multiSvg).resize(72, 72).extend({
    top:15, bottom:16, left:55, right:55, background:{r:0,g:0,b:0,alpha:0},
  }).png().toFile(path.join(out, 'multi_card.png'));
  console.log(`Generated ${variants.length} Main icons from Phosphor 2.1.10.`);
})().catch(error => { console.error(error); process.exitCode = 1; });
