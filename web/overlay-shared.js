import { t, locale, translate } from './i18n.js';
import {broadcastUrl} from './broadcast.js';
import {fonts, fieldNames, styledFields} from './master-options.js';

export function applyAppearance(options) {
  const rgb = options.background.slice(1).match(/../g).map(part => parseInt(part, 16));
  const variables = { width: options.width + 'px', font: fonts[options.font], 'font-size': options.fontSize + 'px',
    'text-color': options.textColor, 'muted-color': options.mutedColor, accent: options.accent,
    background: 'rgba(' + rgb.join(',') + ',' + options.opacity / 100 + ')', radius: options.radius + 'px',
    border: options.border + 'px', padding: options.padding + 'px', gap: options.gap + 'px',
    'cover-size': options.coverSize + 'px', 'element-gap': options.elementGap + 'px', 'content-align': options.contentAlign,
    shadow: options.shadow ? '0 3px 10px #0003' : 'none' };
  for (const [key, value] of Object.entries(variables)) document.documentElement.style.setProperty('--' + key, value);
  Object.assign(document.documentElement.dataset, {layout: options.coverPosition === 'top' ? 'stacked' : 'horizontal',
    coverPosition: options.coverPosition, coverShape: options.coverShape, coverSpin: String(options.coverSpin), overflow: options.overflow});
}

const pendingCards = new Set();
let measureFrame = 0;
function measure(card) {
  pendingCards.add(card);
  if (measureFrame) return;
  measureFrame = requestAnimationFrame(() => {
    measureFrame = 0;
    for (const row of pendingCards) {
      if (!row.isConnected) continue;
      if (row.options.coverFit && row.options.coverPosition !== 'top') {
        const size = Math.max(32, row.querySelector('.info').offsetHeight), art = row.querySelector('.art');
        if (art.style.width !== size + 'px') Object.assign(art.style, {width: size + 'px', height: size + 'px', flexBasis: size + 'px'});
      }
      if (row.options.overflow === 'slide') for (const clip of row.querySelectorAll('.text-clip')) {
        const inner = clip.firstElementChild, distance = Math.max(0, inner.scrollWidth - clip.clientWidth);
        clip.classList.toggle('scrolling', distance > 1);
        inner.style.setProperty('--scroll-distance', -distance + 'px');
        inner.style.setProperty('--scroll-time', Math.max(6, distance / 35 + 4) + 's');
      }
    }
    pendingCards.clear();
  });
}
const cardObserver = new ResizeObserver(entries => { for (const entry of entries) measure(entry.target.closest('.track')); });
export function disposeCard(card) { cardObserver.unobserve(card.querySelector('.info')); pendingCards.delete(card); }
document.addEventListener('visibilitychange', () => { document.documentElement.dataset.paused = String(document.hidden); });

export function makeCard(options) {
  const card = document.createElement('article');
  card.className = 'track';
  card.options = options;
  // Static application markup only; all track data is assigned via textContent.
  card.innerHTML = `<div class="card">
    <div class="art" data-field="cover"><span class="disc" aria-hidden="true"></span><img alt="" hidden decoding="async"></div>
    <div class="info"><div class="tag"><span data-role="label"></span><span class="demo" hidden data-i18n="demoBadge"></span></div>
      <h1 class="text-clip" data-field="title"><span></span></h1><p class="artist text-clip" data-field="artist"><span></span></p><p class="album text-clip" data-field="album"><span></span></p><p class="record-label text-clip" data-field="label"><span></span></p>
      <div class="metrics"><span data-field="key"><span data-i18n="key"></span> <strong></strong></span>
        <span class="tempos"><span data-field="currentBpm"><span data-i18n="currentBpm"></span> <strong data-value="bpm"></strong></span><span data-field="bpm"><span data-i18n="originalBpm"></span> <strong data-value="originalBpm"></strong></span></span></div>
      <div class="timeline" hidden><div class="time-labels"><span data-time="position">—</span><span data-time="duration">—</span></div>
        <div class="time-bar" role="progressbar" aria-valuemin="0" aria-valuemax="100" data-i18n-aria="timeline"><span class="time-fill"></span></div><p class="timeline-status" hidden data-i18n="timelineUnavailable"></p></div>
    </div></div>`;
  for (const name of fieldNames) card.querySelector('[data-field="' + name + '"]').hidden = !options.fields.includes(name);
  for (const name of styledFields) {
    const element = name === 'badges' ? card.querySelector('.tag') : name === 'timeline' ? card.querySelector('.timeline') : card.querySelector('[data-field="' + name + '"]');
    const style = options.fieldStyles[name] || {};
    if (style.color) { element.style.color = style.color; element.querySelectorAll('strong').forEach(node => node.style.color = 'inherit'); }
    if (style.background) element.style.backgroundColor = style.background;
    if (style.font) element.style.fontFamily = fonts[style.font];
    if (style.fontSize !== undefined) element.style.fontSize = style.fontSize + 'px';
    if (style.fontStyle) element.style.fontStyle = style.fontStyle;
    if (style.fontWeight) { element.style.fontWeight = style.fontWeight; element.querySelectorAll('strong').forEach(node => node.style.fontWeight = 'inherit'); }
    for (const key of ['marginTop', 'marginBottom']) if (style[key] !== undefined) element.style[key] = style[key] + 'px';
  }
  card.querySelector('.tag').hidden = !options.badges;
  const img = card.querySelector('img');
  img.addEventListener('load', () => { img.hidden = false; delete img.dataset.failed; if (img.dataset.selected === 'true') img.parentElement.hidden = false; });
  img.addEventListener('error', () => { img.hidden = true; img.dataset.failed = img.dataset.url; if (options.hideMissing) img.parentElement.hidden = true; });
  cardObserver.observe(card.querySelector('.info'));
  translate(card);
  return card;
}

const text = value => typeof value === 'string' && value.trim() ? value : '—';
const numberFormats = new Map();
const setText = (node,value) => { if(node.textContent!==value)node.textContent=value; };
export function formatTime(milliseconds) {
  if (typeof milliseconds !== 'number' || !Number.isFinite(milliseconds)) return '—';
  const seconds = Math.floor(Math.abs(milliseconds) / 1000), hours = Math.floor(seconds / 3600);
  return (milliseconds < 0 ? '−' : '') + (hours ? hours + ':' + String(Math.floor(seconds / 60) % 60).padStart(2, '0') : Math.floor(seconds / 60)) + ':' + String(seconds % 60).padStart(2, '0');
}
export function updateCardContent(card, item, options, display) {
  setText(card.querySelector('[data-role="label"]'),display.label);
  card.querySelector('.demo').hidden = !display.demo;
  // Demo identification remains visible even when ordinary badges are disabled.
  card.querySelector('.tag').hidden = !options.badges && !display.demo;
  card.querySelector('[data-role="label"]').hidden = !options.badges;
  const fields = !display.timeline && options.historyFields !== null ? options.historyFields : options.fields;
  const known = name => ['bpm', 'currentBpm'].includes(name) ? typeof item[name === 'bpm' ? 'originalBpm' : 'bpm'] === 'number' && Number.isFinite(item[name === 'bpm' ? 'originalBpm' : 'bpm']) && item[name === 'bpm' ? 'originalBpm' : 'bpm'] > 0 : text(item[name === 'cover' ? 'coverUrl' : name]) !== '—';
  for (const name of fieldNames) card.querySelector('[data-field="' + name + '"]').hidden = !fields.includes(name) || (name !== 'title' && options.hideMissing && !known(name));
  for (const name of ['title', 'artist', 'album', 'label']) {
    const node = card.querySelector('[data-field="' + name + '"]'), value = name === 'title' && text(item[name]) === '—' ? t('metadataUnavailable') : text(item[name]);
    if (node.firstElementChild.textContent !== value) { node.firstElementChild.textContent = value; node.title = value; measure(card); }
  }
  card.querySelector('.metrics').hidden = ['key', 'bpm', 'currentBpm'].every(name => card.querySelector('[data-field="' + name + '"]').hidden);
  card.querySelector('.tempos').hidden = ['bpm', 'currentBpm'].every(name => card.querySelector('[data-field="' + name + '"]').hidden);
  setText(card.querySelector('[data-field="key"] strong'),text(item.key));
  const formatKey=locale()+':'+options.bpmInteger;
  if(!numberFormats.has(formatKey))numberFormats.set(formatKey,new Intl.NumberFormat(locale(), { maximumFractionDigits: options.bpmInteger ? 0 : 2 }));
  const formatter=numberFormats.get(formatKey);
  for (const name of ['bpm', 'originalBpm']) setText(card.querySelector('[data-value="' + name + '"]'),typeof item[name] === 'number' && Number.isFinite(item[name]) && item[name] > 0 ? formatter.format(item[name]) : '—');
  const timeline = card.querySelector('.timeline');
  timeline.hidden = !options.timeline || !display.timeline;
  if (!timeline.hidden) {
    const valid = typeof item.positionMs === 'number' && Number.isFinite(item.positionMs) && Math.abs(item.positionMs) <= 86400000 &&
      typeof item.durationMs === 'number' && Number.isFinite(item.durationMs) && item.durationMs > 0 && item.durationMs <= 86400000;
    if (options.hideMissing && !valid) timeline.hidden = true;
    timeline.dataset.available = String(valid);
    timeline.querySelector('[data-time="position"]').textContent = valid ? formatTime(item.positionMs) : '—';
    timeline.querySelector('[data-time="duration"]').textContent = valid ? formatTime(item.durationMs) : '—';
    timeline.querySelector('.timeline-status').hidden = valid;
    const progress = valid ? Math.max(0, Math.min(100, item.positionMs / item.durationMs * 100)) : 0;
    const bar = timeline.querySelector('.time-bar'), fill = timeline.querySelector('.time-fill');
    if (valid) { bar.setAttribute('aria-valuenow', progress.toFixed(2)); bar.setAttribute('aria-valuetext', formatTime(item.positionMs) + ' / ' + formatTime(item.durationMs)); }
    else { bar.removeAttribute('aria-valuenow'); bar.removeAttribute('aria-valuetext'); }
    const jump = !valid || timeline.dataset.track !== String(item.trackId) || Math.abs(item.positionMs - Number(timeline.dataset.position)) > 2000;
    fill.style.transitionDuration = jump ? '0ms' : '';
    fill.style.width = progress + '%';
    timeline.dataset.track = String(item.trackId); timeline.dataset.position = String(item.positionMs);
  }
  if (!fields.includes('cover')) { card.querySelector('img').dataset.selected = 'false'; return; }
  const img = card.querySelector('img'), url = item.coverUrl === display.coverUrl ? display.coverUrl : '';
  img.dataset.selected = 'true';
  if (options.hideMissing && (!url || img.dataset.failed === url)) img.parentElement.hidden = true;
  if (!url) { img.hidden = true; img.removeAttribute('src'); delete img.dataset.url; }
  else if (img.dataset.url !== url || (img.hidden && Date.now() - Number(img.dataset.attempt) > 5000)) {
    img.hidden = true; img.dataset.url = url; img.dataset.attempt = String(Date.now());
    img.alt = t('cover') + ': ' + text(item.title); img.src = broadcastUrl(url);
  }
}
