export const controls = [
  ['mode', 'select', ['line', 'fill', 'bars', 'mirror', 'radial', 'history']],
  ['width', 'number', 160, 2560, 10], ['height', 'number', 80, 1440, 10],
  ['color', 'color'], ['color2', 'color'], ['background', 'color'],
  ['opacity', 'range', 0, 100, 1], ['gradient', 'checkbox'],
  ['gain', 'range', 0.1, 10, 0.1], ['smoothing', 'range', 0, 95, 1],
  ['gate', 'range', 0, 0.1, 0.001], ['lineWidth', 'range', 1, 12, 0.5],
  ['bars', 'range', 8, 160, 1], ['gap', 'range', 0, 90, 1],
  ['rounding', 'range', 0, 24, 1], ['glow', 'range', 0, 40, 1],
  ['trails', 'range', 0, 90, 1], ['channel', 'select', ['mix', 'left', 'right']],
  ['minHz', 'number', 20, 10000, 10], ['maxHz', 'number', 100, 24000, 100],
  ['historySeconds', 'range', 2, 30, 1], ['fps', 'select', ['30', '60']],
  ['grid', 'checkbox'], ['centerLine', 'checkbox'], ['hideSilent', 'checkbox']
];
export const defaults = Object.freeze({mode:'line', width:960, height:240, color:'#a8eccf', color2:'#48bce8',
  background:'#0b1215', opacity:0, gradient:true, gain:1.5, smoothing:45, gate:0.002, lineWidth:3,
  bars:64, gap:35, rounding:4, glow:10, trails:0, channel:'mix', minHz:30, maxHz:16000,
  historySeconds:10, fps:'60', grid:false, centerLine:false, hideSilent:true});
export const presets = Object.freeze({
  mint:{...defaults},
  neon:{...defaults, mode:'bars', color:'#ae7cff', color2:'#42dbed', glow:18, smoothing:70},
  mirror:{...defaults, mode:'mirror', color:'#ff7b9c', color2:'#ffca80', bars:80, smoothing:65},
  minimal:{...defaults, color:'#ffffff', gradient:false, lineWidth:2, glow:0, smoothing:25},
  orbit:{...defaults, mode:'radial', width:480, height:480, color:'#72f5b4', color2:'#76a9ff', glow:16, smoothing:65},
  ribbon:{...defaults, mode:'history', color:'#77d5ff', color2:'#b9a0ff', height:180, glow:0, historySeconds:12}
});
export function normalize(input = {}) {
  if (!input || typeof input !== 'object') input = {};
  const result = {...defaults};
  for (const [key, type, a, b, step] of controls) {
    if (!Object.hasOwn(input, key)) continue;
    const value = input[key];
    if (type === 'checkbox') result[key] = value === true || value === '1' || value === 'true';
    else if (type === 'color') { if (typeof value === 'string' && /^#[0-9a-f]{6}$/i.test(value)) result[key] = value; }
    else if (type === 'select') { if (a.includes(String(value))) result[key] = String(value); }
    else if (value !== '' && value !== null && Number.isFinite(Number(value))) {
      result[key] = Number((Math.round(Math.min(b, Math.max(a, Number(value))) / step) * step).toFixed(3));
    }
  }
  result.maxHz = Math.max(result.minHz + 100, result.maxHz);
  return result;
}
export function overlayUrl(options, language = 'en') {
  const url = new URL('/waveform', location.origin);
  for (const [key, value] of Object.entries(normalize(options))) url.searchParams.set(key, typeof value === 'boolean' ? (value ? '1' : '0') : value);
  url.searchParams.set('lang', language === 'de' ? 'de' : 'en');
  return url.href;
}
