import { readSetting, writeSetting } from './storage.js';
import { defaults, presets, normalize, parseOptions, overlayPath, deckOverlayPath } from './master-options.js';
import { t, getLanguage, locale } from './i18n.js';
import {broadcastUrl,loadBroadcastKeys,obsUrl} from './broadcast.js';
import {appReady} from './navigation.js';
import {setupPresets} from './component-presets.js';
import { poll as startPolling } from './poll.js';
import {trackExtras,fontOptions} from './track-controls.js';
const broadcastKeys=await loadBroadcastKeys();
const app=await appReady;
const mode = location.pathname.startsWith('/master-overlay') ? 'master' : 'deck';
const byId = id => document.getElementById(id);
const query = new URLSearchParams(location.search);
const fields = [...document.querySelectorAll('[name="field"]')];
const controls = [...document.querySelectorAll('[data-option]')];
const read = key => { try { return JSON.parse(readSetting(key)); } catch (_) { return null; } };
const save = (key, value) => { try { writeSetting(key, JSON.stringify(value)); } catch (_) {} };
let deck = normalize({ deck: query.get('deck') || read('deckstatus.deck.selected') || 1 }).deck;
const key = (id = deck) => mode === 'master' ? 'deckstatus.master.options' : 'deckstatus.deck.options.' + id;
const stored = id => normalize({ ...(read(key(id)) || defaults), deck: id, lang: getLanguage() });
let options = stored(deck);
if ([...query.keys()].some(name => name in defaults && !['lang', 'deck'].includes(name))) options = normalize({ ...parseOptions(location.search), deck, lang: getLanguage() });
let pending, state = null;
let previewObserver;
byId('preview').addEventListener('load',()=>{
  previewObserver?.disconnect();
  if(options.overflow!=='expand')return;
  const tracks=byId('preview').contentDocument?.getElementById('tracks');if(!tracks)return;
  previewObserver=new ResizeObserver(()=>{
    const width=Math.ceil(Math.max(options.width,tracks.offsetWidth)+16);
    byId('preview').style.width=width+'px';
    byId('dimensions').textContent=t('sourceSize',{width,height:parseInt(byId('preview').style.height,10)});
  });
  previewObserver.observe(tracks);
});
fontOptions(byId('font'));
const extras=document.createElement('div');byId('reset').before(extras);
const refreshExtras=trackExtras(extras,{read:()=>options,write:value=>{options=normalize(value);save(key(),options);clearTimeout(pending);pending=setTimeout(apply,180);},master:mode==='master'});
document.querySelectorAll('main [data-mode]').forEach(node => { node.hidden = node.dataset.mode !== mode; });
byId('deck').value = deck;

function labels() {
  byId('heading').textContent = t(mode + 'Title');
  byId('intro').textContent = t(mode + 'Intro');
  document.title = t(mode === 'master' ? 'masterSettings' : 'deckSettings') + ' · ' + t('appName');
  byId('duration-value').textContent = options.duration + ' ms';
  byId('scale-value').textContent = new Intl.NumberFormat(locale(), { minimumFractionDigits: 2, maximumFractionDigits: 2 }).format(options.historyScale) + '×';
  byId('opacity-value').textContent = options.opacity + '%';
  const available = state && ['connected', 'demo'].includes(state.status);
  const current = mode === 'master' ? state?.current : state?.decks?.find(item => item.id === deck && item.loaded);
  byId('status').textContent = !state ? t('starting') : available ? state.demo ? t('demo') : current ? t(mode === 'master' ? 'masterDeck' : 'deck', { id: current.id }) : t(mode === 'master' ? 'waitMaster' : 'waitDeck') : t(state.status in { stale:1, disconnected:1, unsupported:1, error:1, starting:1 } ? state.status : 'unknown');
}
function syncControls() {
  controls.forEach(input => { if (input.type === 'checkbox') input.checked = options[input.id]; else input.value = options[input.id]; });
  fields.forEach(input => { input.checked = options.fields.includes(input.value); });
  refreshExtras();
  byId('preset').value = Object.keys(presets).find(name => Object.entries(presets[name]).every(([k,v]) => options[k] === v)) || 'custom';
  labels();
}
function apply() {
  clearTimeout(pending);
  options.lang = getLanguage();
  const path = broadcastUrl((mode === 'master' ? overlayPath : deckOverlayPath)(options),broadcastKeys[mode]);
  byId('url').value = obsUrl(path, app);
  byId('open').href = byId('url').value;
  // Reserve space for a full history, larger fonts and a stacked cover.
  const textHeight = options.fontSize * 6 + (options.timeline ? 92 : 0);
  const spacing = Object.values(options.fieldStyles).reduce((sum,style)=>sum+(style.marginTop||0)+(style.marginBottom||0),0) + options.elementGap*7;
  const rowHeight = Math.ceil(2 * options.padding + spacing + (options.coverPosition === 'top' ? options.coverSize + textHeight + 20 : Math.max(options.coverSize, textHeight)));
  const height = Math.ceil(16 + rowHeight + (mode === 'master' ? options.history * (rowHeight * options.historyScale + options.gap) : 0));
  byId('preview').style.width = options.width + 16 + 'px';
  byId('preview').style.height = height + 'px';
  if (byId('preview').getAttribute('src') !== path) byId('preview').src = path;
  byId('dimensions').textContent = t('sourceSize', { width: options.width + 16, height });
  save(key(), options);
  if (mode === 'deck') {
    byId('deck-links').replaceChildren(...[1,2,3,4].map(id => {
      const link = document.createElement('a'); link.textContent = t('deck', { id }) + ' ↗';
      link.href = obsUrl(broadcastUrl(deckOverlayPath(stored(id)),broadcastKeys.deck),app); link.target = '_blank'; link.rel = 'noopener'; return link;
    }));
  }
  labels();
}
function changed(event) {
  if (!fields.some(input => input.checked)) { event.target.checked = true; byId('feedback').textContent = t('minField'); return; }
  byId('feedback').textContent = '';
  options = normalize({ ...options, ...Object.fromEntries(controls.map(input => [input.id, input.type === 'checkbox' ? input.checked : input.value])), fields: fields.filter(input => input.checked).map(input => input.value) });
  // Store immediately so changing decks cannot lose a pending edit.
  save(key(), options);
  labels(); byId('preset').value = 'custom';
  clearTimeout(pending); pending = setTimeout(apply, 180);
}
for (const input of [...controls, ...fields]) {
  input.addEventListener('input', changed);
  input.addEventListener('change', event => { changed(event); syncControls(); apply(); });
}
byId('deck').addEventListener('change', () => { clearTimeout(pending); deck = Number(byId('deck').value); save('deckstatus.deck.selected', deck); options = stored(deck); syncControls(); apply(); });
byId('preset').addEventListener('change', () => { const preset = presets[byId('preset').value]; if (preset) { options = normalize({ ...options, ...preset }); syncControls(); apply(); } });
byId('reset').addEventListener('click', () => { options = normalize({ ...defaults, deck, lang: getLanguage() }); syncControls(); apply(); });
byId('all-decks').addEventListener('click', () => { for (const id of [1,2,3,4]) save(key(id), normalize({ ...options, deck: id })); apply(); byId('feedback').textContent = t('appliedAll'); });
byId('copy').addEventListener('click', async () => {
  try { await navigator.clipboard.writeText(byId('url').value); byId('feedback').textContent = t('copied'); }
  catch (_) { byId('url').select(); byId('feedback').textContent = t('copyFallback'); }
});
window.addEventListener('languagechange', () => { byId('feedback').textContent = ''; syncControls(); apply(); });
syncControls(); apply();
// The status line only matters while the page is on screen.
startPolling(async signal => {
  let failure;
  try {
    const response = await fetch(mode === 'master' ? '/api/master' : '/api/state', { cache: 'no-store', signal });
    if (!response.ok) throw new Error('HTTP ' + response.status);
    state = await response.json();
  } catch (error) { state = { status: 'disconnected' }; failure = error; }
  labels();
  if (failure) throw failure;
}, { interval: 1000, timeout: 2500 });
setupPresets({type:mode,read:()=>({...options,deck}),apply:value=>{
  options=normalize({...value,lang:getLanguage()});
  if(mode==='deck'){deck=options.deck;byId('deck').value=deck;save('deckstatus.deck.selected',deck);}
  syncControls();apply();
}});
