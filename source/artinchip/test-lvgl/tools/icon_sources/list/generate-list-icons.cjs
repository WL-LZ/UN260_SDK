#!/usr/bin/env node
'use strict';

// Optional source-asset maintenance; not part of the firmware make dependency tree.
const fs = require('node:fs/promises');
const path = require('node:path');
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const sharp = require('sharp');

const outputDir = path.resolve(__dirname, '../../../aic_ui/lvgl_data/list_icons');
const variants = [
  { name: 'receipt', size: 24, color: '#8394A0' },
  { name: 'barcode', size: 24, color: '#8394A0' },
  { name: 'warning_circle', size: 24, color: '#8394A0' },
  { name: 'barcode', size: 36, color: '#879BA8' },
  { name: 'warning_circle', size: 36, color: '#879BA8' },
];

async function validate(png, size) {
  const metadata = await sharp(png).metadata();
  assert.equal(metadata.format, 'png');
  assert.equal(metadata.width, size);
  assert.equal(metadata.height, size);
  assert.equal(metadata.channels, 4);
  assert.equal(metadata.hasAlpha, true);
  const pixels = await sharp(png).raw().toBuffer();
  let transparent = 0;
  let nonempty = 0;
  let antialiased = 0;
  let maxAlpha = 0;
  let minX = size, minY = size, maxX = -1, maxY = -1;
  for (let y = 0; y < size; ++y) {
    for (let x = 0; x < size; ++x) {
      const alpha = pixels[(y * size + x) * 4 + 3];
      maxAlpha = Math.max(maxAlpha, alpha);
      if (alpha === 0) ++transparent;
      else {
        ++nonempty;
        if (alpha < 255) ++antialiased;
        minX = Math.min(minX, x); minY = Math.min(minY, y);
        maxX = Math.max(maxX, x); maxY = Math.max(maxY, y);
      }
    }
  }
  assert.ok(nonempty > 0, 'Icon must not be empty');
  assert.ok(transparent > size, 'Canvas must retain transparent background');
  assert.ok(antialiased > 0, 'Edges must retain antialiased alpha');
  assert.ok(maxAlpha > 128, 'Icon must remain visible at its target size');
  assert.ok(minX > 0 && minY > 0 && maxX < size - 1 && maxY < size - 1,
    'Icon must not be clipped against canvas edges');
  return { width: size, height: size, channels: 4, transparent, nonempty,
    antialiased, maxAlpha, bounds: [minX, minY, maxX, maxY],
    sha256: crypto.createHash('sha256').update(png).digest('hex') };
}

async function main() {
  await fs.mkdir(outputDir, { recursive: true });
  const results = [];
  for (const { name, size, color } of variants) {
    const source = await fs.readFile(path.join(__dirname, `${name}_light.svg`), 'utf8');
    assert.ok(source.includes('viewBox="0 0 256 256"'));
    assert.ok(source.includes('fill="currentColor"'));
    const svg = source.replace('width="256"', `width="${size}"`)
      .replace('height="256"', `height="${size}"`)
      .replace('fill="currentColor"', `fill="${color}"`);
    // Rasterize the original vector at the final dimensions, with no bitmap resize.
    const png = await sharp(Buffer.from(svg), { density: 72 })
      .toColourspace('srgb').ensureAlpha()
      .png({ compressionLevel: 9, adaptiveFiltering: false, palette: false })
      .toBuffer();
    const validation = await validate(png, size);
    const file = `${name}_${size}.png`;
    await fs.writeFile(path.join(outputDir, file), png);
    results.push({ file, color, ...validation });
  }
  console.log(JSON.stringify({ sharp: sharp.versions, outputDir, results }, null, 2));
}

main().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
