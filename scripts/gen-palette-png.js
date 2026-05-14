const sharp = require('sharp');
const path = require('path');
const fs = require('fs');

const palette = JSON.parse(fs.readFileSync(path.join(__dirname, '..', 'assets', 'palette.json'), 'utf8'));

const COL_W = 180;
const SWATCH_H = 56;
const LABEL_H = 22;
const PAD = 24;
const GAP = 10;

const groups = [
  { name: 'Surface',  tokens: palette.surface },
  { name: 'Brand',    tokens: palette.brand    },
  { name: 'Gold',     tokens: palette.gold     },
  { name: 'Text',     tokens: palette.text     },
  { name: 'Border',   tokens: palette.border   },
  { name: 'Semantic', tokens: palette.semantic  },
];

const maxCols = Math.max(...groups.map(g => Object.keys(g.tokens).length));
const W = PAD * 2 + groups.length * (COL_W + GAP) - GAP;
const ROW_H = LABEL_H + SWATCH_H;
const H = PAD * 2 + 28 + 14 + ROW_H * maxCols + GAP * (maxCols - 1);

function resolveDisplay(value) {
  // Just show the raw value, clipped
  return value.length > 22 ? value.slice(0, 21) + '…' : value;
}

function swatchFill(value) {
  return value; // SVG understands rgba() natively
}

let rects = '';
let x = PAD;

for (const group of groups) {
  const keys = Object.keys(group.tokens);
  // Group label
  rects += `<text x="${x + COL_W / 2}" y="${PAD + 20}" text-anchor="middle"
    font-family="Arial, sans-serif" font-size="11" font-weight="700"
    fill="rgba(255,255,255,0.55)" letter-spacing="2">${group.name.toUpperCase()}</text>\n`;

  let y = PAD + 28 + 10;
  for (const key of keys) {
    const { value } = group.tokens[key];
    const displayVal = resolveDisplay(value);

    // Swatch rect
    rects += `<rect x="${x}" y="${y}" width="${COL_W}" height="${SWATCH_H}" rx="6"
      fill="${swatchFill(value)}" stroke="rgba(255,255,255,0.08)" stroke-width="1"/>\n`;

    // Key name
    rects += `<text x="${x + 8}" y="${y + SWATCH_H + 14}" text-anchor="start"
      font-family="'Courier New', monospace" font-size="9" fill="rgba(255,255,255,0.45)">${key}</text>\n`;

    // Value
    rects += `<text x="${x + COL_W - 8}" y="${y + SWATCH_H + 14}" text-anchor="end"
      font-family="'Courier New', monospace" font-size="9" fill="rgba(255,255,255,0.30)">${displayVal}</text>\n`;

    y += SWATCH_H + LABEL_H + GAP;
  }

  x += COL_W + GAP;
}

const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="${W}" height="${H}">
  <rect width="${W}" height="${H}" fill="#0d1a35"/>
  <text x="${W / 2}" y="${PAD - 4}" text-anchor="middle"
    font-family="Arial, sans-serif" font-size="13" font-weight="700"
    fill="rgba(255,255,255,0.70)" letter-spacing="3">OOTP 27 ULTIMATE KBO MOD — COLOUR PALETTE</text>
  ${rects}
</svg>`;

const outPath = path.join(__dirname, '..', 'assets', 'palette.png');
sharp(Buffer.from(svg))
  .png()
  .toFile(outPath)
  .then(() => console.log(`✓ ${outPath}`))
  .catch(e => console.error(e));
