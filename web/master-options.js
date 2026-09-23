export const fieldNames = ['title', 'artist', 'album', 'label', 'key', 'bpm', 'currentBpm', 'cover'];
export const styledFields = [...fieldNames.filter(name => name !== 'cover'), 'badges', 'timeline'];
export const fonts = { system: '"Segoe UI", sans-serif', serif: 'Georgia, serif', mono: 'Consolas, monospace',
  arial: 'Arial, sans-serif', calibri: 'Calibri, sans-serif', tahoma: 'Tahoma, sans-serif', verdana: 'Verdana, sans-serif',
  trebuchet: '"Trebuchet MS", sans-serif', impact: 'Impact, sans-serif', palatino: '"Palatino Linotype", serif' };
export const presets = {
  midnight: { background: '#112122', textColor: '#f5faf8', mutedColor: '#b1c5bc', accent: '#a8eccf', opacity: 95, radius: 12, border: 3, shadow: true },
  light: { background: '#f3f7f8', textColor: '#172a32', mutedColor: '#465d68', accent: '#007c91', opacity: 96, radius: 16, border: 3, shadow: true },
  minimal: { background: '#101820', textColor: '#ffffff', mutedColor: '#d3e2ea', accent: '#80c9ff', opacity: 0, radius: 0, border: 0, shadow: false }
};
export const defaults = { infoVersion: 2, history: 5, fields: fieldNames.filter(name => name !== 'label'), historyFields: null,
  bpmInteger: false, hideMissing: false, contentAlign: 'left', overflow: 'ellipsis', elementGap: 0, fieldStyles: {},
  coverPosition: 'left', coverShape: 'square', coverSpin: false, coverFit: false,
  duration: 650, width: 720, historyScale: 1, align: 'left',
  lang: 'en', deck: 1, timeline: false, font: 'system', fontSize: 23, coverSize: 84, padding: 18, gap: 10,
  layout: 'horizontal', badges: true, ...presets.midnight };

export function normalize(input = {}) {
  const integer = (value, fallback, min, max) => {
    if (!/^\d+$/.test(String(value))) return fallback;
    return Math.min(max, Math.max(min, Number(value)));
  };
  const selectedFields = value => {
    const selected = fieldNames.filter(name => value.includes(name));
    // Old URLs/presets used one BPM switch for both values.
    if (Number(input.infoVersion) !== 2 && selected.includes('bpm') && !selected.includes('currentBpm')) selected.push('currentBpm');
    return selected;
  };
  const fields = Array.isArray(input.fields) ? selectedFields(input.fields) : defaults.fields;
  const scale = /^-?(?:\d+\.?\d*|\.\d+)$/.test(String(input.historyScale)) ? Number(input.historyScale) : NaN;
  const color = name => /^#[0-9a-f]{6}$/i.test(input[name]) ? input[name].toLowerCase() : defaults[name];
  const flag = name => input[name] === undefined ? defaults[name] : [true, 'true', '1', 1].includes(input[name]);
  const fieldStyles = {};
  for (const name of styledFields) {
    const style = input.fieldStyles?.[name];
    if (!style || typeof style !== 'object' || Array.isArray(style)) continue;
    const value = {};
    for (const key of ['color', 'background']) if (/^#[0-9a-f]{6}$/i.test(style[key])) value[key] = style[key].toLowerCase();
    if (Object.hasOwn(fonts, style.font)) value.font = style.font;
    if (/^\d+$/.test(String(style.fontSize))) value.fontSize = integer(style.fontSize, 23, 8, 200);
    if (['normal', 'italic', 'oblique'].includes(style.fontStyle)) value.fontStyle = style.fontStyle;
    if (/^[1-9]00$/.test(String(style.fontWeight))) value.fontWeight = Number(style.fontWeight);
    for (const key of ['marginTop', 'marginBottom']) if (style[key] !== undefined) value[key] = integer(style[key], 0, 0, 64);
    if (Object.keys(value).length) fieldStyles[name] = value;
  }
  return {
    infoVersion: 2, historyFields: Array.isArray(input.historyFields) ? selectedFields(input.historyFields) : null,
    bpmInteger: flag('bpmInteger'), hideMissing: flag('hideMissing'), fieldStyles,
    contentAlign: ['left', 'center', 'right'].includes(input.contentAlign) ? input.contentAlign : defaults.contentAlign,
    overflow: ['ellipsis', 'slide', 'expand'].includes(input.overflow) ? input.overflow : defaults.overflow,
    elementGap: integer(input.elementGap, 0, 0, 40),
    coverPosition: ['left', 'top', 'right'].includes(input.coverPosition) ? input.coverPosition : input.layout === 'stacked' ? 'top' : 'left',
    coverShape: input.coverShape === 'round' ? 'round' : 'square', coverSpin: flag('coverSpin'), coverFit: flag('coverFit'),
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
    font: Object.hasOwn(fonts, input.font) ? input.font : defaults.font,
    fontSize: integer(input.fontSize, defaults.fontSize, 14, 40), coverSize: integer(input.coverSize, defaults.coverSize, 32, 180),
    padding: integer(input.padding, defaults.padding, 4, 48), gap: integer(input.gap, defaults.gap, 0, 40),
    layout: input.layout === 'stacked' ? 'stacked' : 'horizontal'
  };
}

export function parseOptions(search) {
  const params = new URLSearchParams(search);
  const value = Object.fromEntries(params);
  if (params.has('fields')) value.fields = params.get('fields').split(',');
  if (params.has('historyFields')) value.historyFields = params.get('historyFields') === 'same' ? null : params.get('historyFields').split(',');
  if (params.has('fieldStyles')) { try { value.fieldStyles = JSON.parse(params.get('fieldStyles')); } catch { value.fieldStyles = {}; } }
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
    params.set(key, key === 'fieldStyles' ? JSON.stringify(setting) : key === 'historyFields' ? setting === null ? 'same' : setting.join(',') : key === 'fields' ? setting.join(',') : key === 'historyScale' ? setting.toFixed(2) : typeof setting === 'boolean' ? (setting ? '1' : '0') : setting);
  }
  return params;
}

export function deckOverlayPath(options) {
  const value = normalize(options), params = optionQuery(value);
  params.delete('history'); params.delete('historyScale'); params.set('deck', value.deck);
  return '/overlay?' + params;
}
