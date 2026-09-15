import { t, locale, translate } from './i18n.js';

export function applyAppearance(options) {
  const fonts = { system: '"Segoe UI", sans-serif', serif: 'Georgia, serif', mono: 'Consolas, monospace' };
  const rgb = options.background.slice(1).match(/../g).map(part => parseInt(part, 16));
  const variables = { width: options.width + 'px', font: fonts[options.font], 'font-size': options.fontSize + 'px',
    'text-color': options.textColor, 'muted-color': options.mutedColor, accent: options.accent,
    background: 'rgba(' + rgb.join(',') + ',' + options.opacity / 100 + ')', radius: options.radius + 'px',
    border: options.border + 'px', padding: options.padding + 'px', gap: options.gap + 'px',
    'cover-size': options.coverSize + 'px', shadow: options.shadow ? '0 3px 10px #0003' : 'none' };
  for (const [key, value] of Object.entries(variables)) document.documentElement.style.setProperty('--' + key, value);
  document.documentElement.dataset.layout = options.layout;
}

export function makeCard(options) {
  const card = document.createElement('article');
  card.className = 'track';
  // Static application markup only; all track data is assigned via textContent.
  card.innerHTML = `<div class="card">
    <div class="art" data-field="cover"><span class="disc" aria-hidden="true"></span><img alt="" hidden decoding="async"></div>
    <div class="info"><div class="tag"><span data-role="label"></span><span class="demo" hidden data-i18n="demoBadge"></span></div>
      <h1 data-field="title"></h1><p class="artist" data-field="artist"></p><p class="album" data-field="album"></p>
      <div class="metrics"><span data-field="key"><span data-i18n="key"></span> <strong></strong></span>
        <span class="tempos" data-field="bpm"><span><span data-i18n="currentBpm"></span> <strong data-value="bpm"></strong> BPM</span><span><span data-i18n="originalBpm"></span> <strong data-value="originalBpm"></strong> BPM</span></span></div>
      <div class="timeline" hidden><div class="time-labels"><span data-time="position">—</span><span data-time="duration">—</span></div>
        <div class="time-bar" role="progressbar" aria-valuemin="0" aria-valuemax="100" data-i18n-aria="timeline"><span class="time-fill"></span></div><p class="timeline-status" hidden data-i18n="timelineUnavailable"></p></div>
    </div></div>`;
  for (const name of ['title', 'artist', 'album', 'key', 'bpm', 'cover']) card.querySelector('[data-field="' + name + '"]').hidden = !options.fields.includes(name);
  card.querySelector('.metrics').hidden = !options.fields.includes('bpm') && !options.fields.includes('key');
  card.querySelector('.tag').hidden = !options.badges;
  const img = card.querySelector('img');
  img.addEventListener('load', () => { img.hidden = false; });
  img.addEventListener('error', () => { img.hidden = true; });
  translate(card);
  return card;
}

const text = value => typeof value === 'string' && value.trim() ? value : '—';
export function formatTime(milliseconds) {
  if (typeof milliseconds !== 'number' || !Number.isFinite(milliseconds)) return '—';
  const seconds = Math.floor(Math.abs(milliseconds) / 1000), hours = Math.floor(seconds / 3600);
  return (milliseconds < 0 ? '−' : '') + (hours ? hours + ':' + String(Math.floor(seconds / 60) % 60).padStart(2, '0') : Math.floor(seconds / 60)) + ':' + String(seconds % 60).padStart(2, '0');
}
export function updateCardContent(card, item, options, display) {
  card.querySelector('[data-role="label"]').textContent = display.label;
  card.querySelector('.demo').hidden = !display.demo;
  // Demo identification remains visible even when ordinary badges are disabled.
  card.querySelector('.tag').hidden = !options.badges && !display.demo;
  card.querySelector('[data-role="label"]').hidden = !options.badges;
  for (const name of ['title', 'artist', 'album']) card.querySelector('[data-field="' + name + '"]').textContent = name === 'title' && text(item[name]) === '—' ? t('metadataUnavailable') : text(item[name]);
  card.querySelector('[data-field="key"] strong').textContent = text(item.key);
  const formatter = new Intl.NumberFormat(locale(), { maximumFractionDigits: 2 });
  for (const name of ['bpm', 'originalBpm']) card.querySelector('[data-value="' + name + '"]').textContent = typeof item[name] === 'number' && Number.isFinite(item[name]) && item[name] > 0 ? formatter.format(item[name]) : '—';
  const timeline = card.querySelector('.timeline');
  timeline.hidden = !options.timeline || !display.timeline;
  if (!timeline.hidden) {
    const valid = typeof item.positionMs === 'number' && Number.isFinite(item.positionMs) && Math.abs(item.positionMs) <= 86400000 &&
      typeof item.durationMs === 'number' && Number.isFinite(item.durationMs) && item.durationMs > 0 && item.durationMs <= 86400000;
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
  if (!options.fields.includes('cover')) return;
  const img = card.querySelector('img'), url = item.coverUrl === display.coverUrl ? display.coverUrl : '';
  if (!url) { img.hidden = true; img.removeAttribute('src'); delete img.dataset.url; }
  else if (img.dataset.url !== url || (img.hidden && Date.now() - Number(img.dataset.attempt) > 5000)) {
    img.hidden = true; img.dataset.url = url; img.dataset.attempt = String(Date.now());
    img.alt = t('cover') + ': ' + text(item.title); img.src = url;
  }
}
