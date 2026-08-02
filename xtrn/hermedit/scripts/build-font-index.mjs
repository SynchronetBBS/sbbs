// Build fonts/tdfont_index.json from Synchronet's fonts + the height map.
//
// The .tdf files themselves ship with Synchronet (ctrl/tdfonts/) — pass the
// font dir as argv[2] or set SBBS_CTRL; a local fonts/tdf/ dir is used when
// present (per-board overrides). The height map (tdfont_map.json,
// height -> [names]) is reused as-is for the size buckets; this adds each
// font's TYPE (Outline/Block/Color) by reading byte 41 of the .tdf header,
// so the picker can filter by size AND type without parsing 1071 fonts at
// runtime. Re-run when the font set changes:
//   node scripts/build-font-index.mjs [/path/to/tdfonts]
import { readFileSync, writeFileSync, existsSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const fontsDir = join(root, 'fonts');
const tdfDir = process.argv[2]
  || (process.env.SBBS_CTRL ? join(process.env.SBBS_CTRL, 'tdfonts') : '/sbbs/ctrl/tdfonts');
const map = JSON.parse(readFileSync(join(fontsDir, 'tdfont_map.json'), 'utf8'));

const MAGIC = [0x13, 0x54, 0x68, 0x65, 0x44, 0x72, 0x61, 0x77, 0x20, 0x46, 0x4f, 0x4e, 0x54, 0x53, 0x20, 0x66, 0x69, 0x6c, 0x65, 0x1a];
const TYPE = ['Outline', 'Block', 'Color'];

const out = [];
let missing = 0;
for (const height of Object.keys(map)) {
  for (const name of map[height]) {
    const path = join(tdfDir, name + '.tdf');
    if (!existsSync(path)) { missing++; continue; }
    const buf = readFileSync(path);
    let ok = buf.length >= 233;
    for (let i = 0; ok && i < MAGIC.length; i++) if (buf[i] !== MAGIC[i]) ok = false;
    if (!ok) { missing++; continue; }
    out.push({ name: name, height: Number(height), type: TYPE[buf[41]] || 'Unknown' });
  }
}
out.sort((a, b) => (a.name < b.name ? -1 : a.name > b.name ? 1 : 0));
writeFileSync(join(fontsDir, 'tdfont_index.json'), JSON.stringify(out));
console.log(`wrote tdfont_index.json: ${out.length} fonts (${missing} skipped)`);
const byType = {};
for (const f of out) byType[f.type] = (byType[f.type] || 0) + 1;
console.log('by type:', byType);
