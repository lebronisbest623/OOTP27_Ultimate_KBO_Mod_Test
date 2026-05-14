const sharp = require('sharp');
const path = require('path');
const fs = require('fs');

const OUT = path.join(__dirname, '..', 'assets', 'icons');
fs.mkdirSync(OUT, { recursive: true });

// ─── A — Split Word ───────────────────────────────────────────────────────────
// No badge shape. Canvas cut in half: red left / navy right.
// "K" lives on red, "BO" lives on navy, gold seam between them.
function svgA() {
  return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 400 400">
  <rect x="0"   y="0" width="200" height="400" fill="#c9184a"/>
  <rect x="200" y="0" width="200" height="400" fill="#0d1a35"/>
  <line x1="200" y1="0" x2="200" y2="400" stroke="#d4a017" stroke-width="2.5" opacity="0.75"/>
  <text x="192" y="252" text-anchor="end"
    font-family="Arial Black, Impact, sans-serif" font-weight="900"
    font-size="148" fill="white">K</text>
  <text x="208" y="252" text-anchor="start"
    font-family="Arial Black, Impact, sans-serif" font-weight="900"
    font-size="148" fill="white" letter-spacing="-2">BO</text>
  <text x="200" y="42" text-anchor="middle"
    font-family="Arial, sans-serif" font-size="11"
    fill="rgba(255,255,255,0.48)" letter-spacing="9">OOTP 27</text>
  <text x="200" y="374" text-anchor="middle"
    font-family="Arial, sans-serif" font-size="11"
    fill="rgba(255,255,255,0.48)" letter-spacing="9">ULTIMATE MOD</text>
</svg>`;
}

// ─── B — Red Seal ─────────────────────────────────────────────────────────────
// Invert the palette: solid brand-red circle, KBO reversed in white.
function svgB() {
  return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 400 400">
  <rect width="400" height="400" fill="#060810"/>
  <circle cx="200" cy="200" r="178" fill="#c9184a"/>
  <circle cx="200" cy="200" r="178" fill="none" stroke="white" stroke-width="4"/>
  <circle cx="200" cy="200" r="170" fill="none" stroke="rgba(255,255,255,0.22)" stroke-width="1"/>
  <text x="200" y="120" text-anchor="middle"
    font-family="Arial, sans-serif" font-size="12"
    fill="rgba(255,255,255,0.58)" letter-spacing="7">OOTP 27</text>
  <line x1="132" y1="136" x2="268" y2="136"
    stroke="rgba(255,255,255,0.38)" stroke-width="1"/>
  <text x="200" y="252" text-anchor="middle"
    font-family="Arial Black, Impact, sans-serif" font-weight="900"
    font-size="122" fill="white" letter-spacing="-3">KBO</text>
  <line x1="132" y1="268" x2="268" y2="268"
    stroke="rgba(255,255,255,0.38)" stroke-width="1"/>
  <text x="200" y="302" text-anchor="middle"
    font-family="Arial, sans-serif" font-size="12"
    fill="rgba(255,255,255,0.75)" letter-spacing="8">ULTIMATE MOD</text>
</svg>`;
}

// ─── C — Crimson Wordmark ─────────────────────────────────────────────────────
// No container at all. KBO itself is the logo, in brand red.
// Full-width rules above and below. Text hierarchy does all the work.
function svgC() {
  return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 400 400">
  <rect width="400" height="400" fill="#060810"/>
  <text x="200" y="100" text-anchor="middle"
    font-family="Arial, sans-serif" font-size="12"
    fill="rgba(255,255,255,0.30)" letter-spacing="10">OOTP 27</text>
  <text x="200" y="122" text-anchor="middle"
    font-family="Arial, sans-serif" font-size="12"
    fill="rgba(255,255,255,0.30)" letter-spacing="8">ULTIMATE</text>
  <line x1="0" y1="145" x2="400" y2="145" stroke="rgba(201,24,74,0.45)" stroke-width="1"/>
  <text x="200" y="293" text-anchor="middle"
    font-family="Arial Black, Impact, sans-serif" font-weight="900"
    font-size="175" fill="#c9184a" letter-spacing="-5">KBO</text>
  <line x1="0" y1="315" x2="400" y2="315" stroke="rgba(201,24,74,0.45)" stroke-width="1"/>
  <text x="200" y="352" text-anchor="middle"
    font-family="Arial, sans-serif" font-size="12"
    fill="rgba(255,255,255,0.30)" letter-spacing="10">MOD</text>
</svg>`;
}

// ─── D — Diagonal ─────────────────────────────────────────────────────────────
// Navy square. Crimson diagonal band slashes across. KBO tilted inside it.
// Corner notations give a blueprint/technical feel.
function svgD() {
  return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 400 400">
  <rect width="400" height="400" fill="#0d1a35"/>
  <g transform="rotate(-18, 200, 200)">
    <rect x="-60" y="148" width="520" height="104" fill="#c9184a"/>
    <text x="200" y="230" text-anchor="middle"
      font-family="Arial Black, Impact, sans-serif" font-weight="900"
      font-size="86" fill="white" letter-spacing="-1">KBO</text>
  </g>
  <text x="36" y="62" text-anchor="start"
    font-family="Arial, sans-serif" font-size="11"
    fill="rgba(255,255,255,0.38)" letter-spacing="5">OOTP 27</text>
  <line x1="32" y1="70" x2="32" y2="84"  stroke="#d4a017" stroke-width="1" opacity="0.6"/>
  <line x1="32" y1="70" x2="92" y2="70"  stroke="#d4a017" stroke-width="1" opacity="0.6"/>
  <text x="364" y="352" text-anchor="end"
    font-family="Arial, sans-serif" font-size="11"
    fill="rgba(255,255,255,0.38)" letter-spacing="5">ULTIMATE MOD</text>
  <line x1="368" y1="356" x2="368" y2="342" stroke="#d4a017" stroke-width="1" opacity="0.6"/>
  <line x1="368" y1="356" x2="308" y2="356" stroke="#d4a017" stroke-width="1" opacity="0.6"/>
</svg>`;
}

const concepts = { A: svgA(), B: svgB(), C: svgC(), D: svgD() };

Promise.all(
  Object.entries(concepts).map(([name, svg]) => {
    fs.writeFileSync(path.join(OUT, `concept-${name}.svg`), svg);
    return sharp(Buffer.from(svg))
      .resize(512, 512)
      .png()
      .toFile(path.join(OUT, `concept-${name}.png`))
      .then(() => console.log(`✓ ${name}`));
  })
).catch(e => console.error(e));
