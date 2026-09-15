import { t, translate, locale } from './i18n.js';
const $ = id => document.getElementById(id);
const rows = new Map(), cursors = [null];
let page = 0, state = { entries: [], total: 0, status: 'starting' }, busy = false, timer, disposed = false;
const text = value => typeof value === 'string' && value.trim() ? value : '—';
const tempo = value => Number.isFinite(value) && value > 0 ? value.toLocaleString(locale(), { maximumFractionDigits: 2 }) : '—';
function rowFor(entry) {
  let row = rows.get(entry.entryId);
  if (!row) {
    row = document.createElement('tr');
    row.innerHTML = '<td data-field="number"></td><td class="history-time"><span data-field="time"></span><span class="history-date" data-field="date"></span></td><td><div class="history-track"><div class="history-art"><span aria-hidden="true">♪</span><img hidden decoding="async" alt=""></div><div><div class="history-title" data-field="title"></div><div class="history-artist" data-field="artist"></div></div></div></td><td class="history-album" data-field="album"></td><td data-field="deck"></td><td data-field="key"></td><td class="history-tempo"><div data-field="bpm"></div><div class="history-original" data-field="original"></div></td><td><span class="history-badge" data-field="state"></span></td>';
    const image = row.querySelector('img');
    image.addEventListener('load', () => { image.hidden = false; });
    image.addEventListener('error', () => { image.hidden = true; });
    rows.set(entry.entryId, row);
  }
  const field = (name, value) => { row.querySelector('[data-field="' + name + '"]').textContent = value; };
  const current = entry.isMaster === true && (state.status === 'connected' || state.status === 'demo');
  row.dataset.entryId = entry.entryId; row.dataset.current = String(current);
  field('number', entry.entryId);
  const date = Number.isFinite(entry.startedAt) ? new Date(entry.startedAt) : null;
  const validDate = date && Number.isFinite(date.getTime());
  field('time', validDate ? date.toLocaleTimeString(locale(), { hour: '2-digit', minute: '2-digit', second: '2-digit' }) : '—');
  field('date', validDate ? date.toLocaleDateString(locale(), { day: '2-digit', month: 'short' }) : '');
  field('title', text(entry.title) === '—' ? t('metadataUnavailable') : entry.title);
  for (const key of ['artist', 'album', 'key']) field(key, text(entry[key]));
  field('deck', Number.isInteger(entry.id) ? String(entry.id) : '—');
  field('bpm', t(current ? 'currentBpm' : 'historyCapturedBpm') + ': ' + tempo(entry.bpm));
  field('original', t('originalBpm') + ': ' + tempo(entry.originalBpm));
  field('state', t(current ? 'historyCurrent' : entry.endedAt == null ? 'historyLastObserved' : 'historyPrevious'));
  const image = row.querySelector('img');
  const expected = '/api/history/covers/' + entry.trackId;
  const url = Number.isSafeInteger(entry.trackId) && entry.trackId > 0 && entry.coverUrl === expected ? expected : '';
  if (!url) { image.hidden = true; image.removeAttribute('src'); delete image.dataset.url; }
  else if (image.dataset.url !== url || (image.hidden && Date.now() - Number(image.dataset.attempt) > 5000)) {
    image.hidden = true; image.dataset.url = url; image.dataset.attempt = Date.now(); image.src = url;
  }
  return row;
}
function render() {
  document.title = t('fullHistory') + ' · DeckStatus';
  $('count').textContent = t('historyCount', { count: state.total.toLocaleString(locale()) });
  const live = state.status === 'connected' || state.status === 'demo';
  const statusKey = state.status === 'disconnected' ? 'unreachable' : ['starting', 'unsupported', 'error', 'stale'].includes(state.status) ? state.status : 'unknown';
  $('status').textContent = t(live ? page === 0 ? 'historyLive' : 'historyOlderPage' : statusKey);
  $('demo-notice').hidden = !state.demo;
  const fragment = document.createDocumentFragment(), ids = new Set();
  for (const entry of state.entries) { ids.add(entry.entryId); fragment.append(rowFor(entry)); }
  $('history-rows').replaceChildren(fragment);
  for (const id of rows.keys()) if (!ids.has(id)) rows.delete(id);
  $('empty').hidden = state.entries.length > 0;
  $('newer').disabled = busy || page === 0;
  $('older').disabled = busy || !state.nextBefore;
  $('refresh').disabled = busy;
  $('page').textContent = t('historyPage', { page: page + 1 });
}
async function load() {
  if (busy || disposed) return;
  clearTimeout(timer); busy = true; render();
  try {
    const query = new URLSearchParams({ limit: '100' });
    if (cursors[page]) query.set('before', cursors[page]);
    const response = await fetch('/api/history?' + query, { cache: 'no-store', signal: AbortSignal.timeout(2500) });
    if (!response.ok) throw Error('History unavailable');
    const result = await response.json();
    if (!Array.isArray(result.entries) || !Number.isSafeInteger(result.total) || result.total < 0) throw Error('Invalid history');
    state = result;
  } catch { state = { ...state, status: 'disconnected' }; }
  finally { busy = false; render(); if (!disposed && page === 0) timer = setTimeout(load, 1000); }
}
$('older').addEventListener('click', () => { if (busy || !state.nextBefore) return; cursors[++page] = state.nextBefore; load(); });
$('newer').addEventListener('click', () => { if (busy || !page) return; --page; load(); });
$('refresh').addEventListener('click', () => { if (busy) return; page = 0; cursors.splice(1); load(); });
window.addEventListener('languagechange', render);
window.addEventListener('pagehide', () => { disposed = true; clearTimeout(timer); });
translate(); load();
