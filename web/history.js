import { t, translate, locale } from './i18n.js';
import {api} from './auth.js';
const $ = id => document.getElementById(id);
const rows = new Map(), cursors = [null];
let page = 0, state = { entries: [], total: 0, status: 'starting' }, busy = false, timer, disposed = false;
let viewerState={available:false,user:null,pending:null},viewerBusy=false,viewerTimer,voting=false;
function renderViewer(){
  $('viewer-status').textContent=viewerState.user?t('viewerSignedIn',{user:viewerState.user.login}):t(viewerState.available?'viewerRequired':'viewerUnavailable');
  $('viewer-login').hidden=!!viewerState.user||!!viewerState.pending;$('viewer-login').disabled=viewerBusy||!viewerState.available;
  $('viewer-logout').hidden=!viewerState.user&&!viewerState.pending;$('viewer-logout').disabled=viewerBusy;
  $('viewer-pending').hidden=!viewerState.pending;if(viewerState.pending){$('viewer-code').textContent=viewerState.pending.code;$('viewer-activate').href=viewerState.pending.url;}
  $('history-ratings-link').hidden=!state.canViewRatings;
}
async function viewerRequest(action){if(viewerBusy)return;viewerBusy=true;renderViewer();try{viewerState=await api('/api/public/twitch',action?{action}:undefined);$('viewer-error').textContent='';if(action==='logout')state.viewer=null;await load();}catch(error){$('viewer-error').textContent=error.message;if(action==='poll'){viewerState.pending=null;viewerState.user=null;}}finally{viewerBusy=false;render();}}
async function pollViewer(){if(disposed)return;await viewerRequest(viewerState.pending?'poll':undefined);if(!disposed)viewerTimer=setTimeout(pollViewer,5000);}
$('viewer-login').addEventListener('click',()=>viewerRequest('start'));$('viewer-logout').addEventListener('click',()=>viewerRequest('logout'));
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
    const rating=document.createElement('td');rating.className='history-rating';const stars=document.createElement('div');stars.className='rating-stars';stars.setAttribute('role','group');stars.setAttribute('aria-label',t('ratingYourVote'));
    for(let value=1;value<=5;value++){const button=document.createElement('button');button.type='button';button.textContent='★';button.dataset.stars=value;button.setAttribute('aria-label',t('ratingStars',{count:value}));button.addEventListener('click',async()=>{if(voting||!state.viewer)return;voting=true;render();try{const result=await api('/api/public/rating',{track:row.dataset.ratingId,stars:value});for(const item of state.entries)if(item.ratingId===row.dataset.ratingId)item.rating=result;$('rating-feedback').textContent=t('ratingThanks');render();}catch(error){$('rating-feedback').textContent=error.message;}finally{voting=false;await load();render();}});stars.append(button);}
    const average=document.createElement('span');average.className='rating-average';rating.append(stars,average);row.append(rating);
  }
  const field = (name, value) => { row.querySelector('[data-field="' + name + '"]').textContent = value; };
  const current = entry.isMaster === true && (state.status === 'connected' || state.status === 'demo');
  row.dataset.entryId = entry.entryId; row.dataset.current = String(current);
  field('number', entry.entryId);
  row.dataset.ratingId=entry.ratingId||'';
  row.querySelectorAll('[data-stars]').forEach(button=>{const active=Number(button.dataset.stars)<=entry.rating?.mine;button.dataset.active=active;button.setAttribute('aria-pressed',String(Number(button.dataset.stars)===entry.rating?.mine));button.setAttribute('aria-label',t('ratingStars',{count:button.dataset.stars}));button.disabled=voting||!state.viewer||!entry.ratingId;});
  row.querySelector('.rating-average').textContent=entry.rating?.count?t('ratingSummary',{average:entry.rating.average.toLocaleString(locale(),{maximumFractionDigits:1}),count:entry.rating.count}):t(entry.ratingId?'ratingNoVotes':'ratingUnavailable');
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
  renderViewer();
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
window.addEventListener('pagehide', () => { disposed = true; clearTimeout(timer);clearTimeout(viewerTimer); });
const ratingHeading=document.createElement('th');ratingHeading.scope='col';ratingHeading.dataset.i18n='ratingYourVote';document.querySelector('thead tr').append(ratingHeading);
translate(); load();pollViewer();
