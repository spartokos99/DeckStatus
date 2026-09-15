export const fieldNames = ['title', 'artist', 'album', 'key', 'bpm', 'cover'];
export const presets = {
  midnight: { background: '#112122', textColor: '#f5faf8', mutedColor: '#b1c5bc', accent: '#a8eccf', opacity: 95, radius: 12, border: 3, shadow: true },
  light: { background: '#f3f7f8', textColor: '#172a32', mutedColor: '#465d68', accent: '#007c91', opacity: 96, radius: 16, border: 3, shadow: true },
  minimal: { background: '#101820', textColor: '#ffffff', mutedColor: '#d3e2ea', accent: '#80c9ff', opacity: 0, radius: 0, border: 0, shadow: false }
};
export const defaults = { history: 5, fields: [...fieldNames], duration: 650, width: 720, historyScale: 1, align: 'left',
  lang: 'en', deck: 1, timeline: false, font: 'system', fontSize: 23, coverSize: 84, padding: 18, gap: 10,
  layout: 'horizontal', badges: true, ...presets.midnight };

export function normalize(input = {}) {
  const integer = (value, fallback, min, max) => {
    if (!/^\d+$/.test(String(value))) return fallback;
    return Math.min(max, Math.max(min, Number(value)));
  };
  const fields = Array.isArray(input.fields) ? fieldNames.filter(name => input.fields.includes(name)) : defaults.fields;
  const scale = /^-?(?:\d+\.?\d*|\.\d+)$/.test(String(input.historyScale)) ? Number(input.historyScale) : NaN;
  const color = name => /^#[0-9a-f]{6}$/i.test(input[name]) ? input[name].toLowerCase() : defaults[name];
  const flag = name => input[name] === undefined ? defaults[name] : [true, 'true', '1', 1].includes(input[name]);
  return {
    history: integer(input.history, defaults.history, 0, 50),
    fields: fields.length ? [...fields] : ['title'],
    duration: integer(input.duration, defaults.duration, 0, 2000),
    width: integer(input.width, defaults.width, 320, 1600),
    historyScale: Number.isFinite(scale) ? Math.round(Math.min(1, Math.max(.2, scale)) * 100) / 100 : defaults.historyScale,
    align: ['left', 'center', 'right'].includes(input.align) ? input.align : defaults.align,
    lang: input.lang === 'de' ? 'de' : 'en', deck: integer(input.deck, 1, 1, 4), timeline: flag('timeline'),
    background: color('background'), textColor: color('textColor'), mutedColor: color('mutedColor'), accent: color('accent'),
    opacity: integer(input.opacity, defaults.opacity, 0, 100), radius: integer(input.radius, defaults.radius, 0, 40),
    border: integer(input.border, defaults.border, 0, 10), shadow: flag('shadow'), badges: flag('badges'),
    font: ['system', 'serif', 'mono'].includes(input.font) ? input.font : defaults.font,
    fontSize: integer(input.fontSize, defaults.fontSize, 14, 40), coverSize: integer(input.coverSize, defaults.coverSize, 32, 180),
    padding: integer(input.padding, defaults.padding, 4, 48), gap: integer(input.gap, defaults.gap, 0, 40),
    layout: input.layout === 'stacked' ? 'stacked' : 'horizontal'
  };
}

export function parseOptions(search) {
  const params = new URLSearchParams(search);
  const value = Object.fromEntries(params);
  if (params.has('fields')) value.fields = params.get('fields').split(',');
  return normalize(value);
}

export function overlayPath(options) {
  const value = normalize(options);
  return '/master-overlay?' + optionQuery(value);
}

export function optionQuery(options) {
  const value = normalize(options);
  const params = new URLSearchParams();
  for (const [key, setting] of Object.entries(value)) {
    if (key === 'deck') continue;
    params.set(key, key === 'fields' ? setting.join(',') : key === 'historyScale' ? setting.toFixed(2) : typeof setting === 'boolean' ? (setting ? '1' : '0') : setting);
  }
  return params;
}

export function deckOverlayPath(options) {
  const value = normalize(options), params = optionQuery(value);
  params.delete('history'); params.delete('historyScale'); params.set('deck', value.deck);
  return '/overlay?' + params;
}
