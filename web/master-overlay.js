import { parseOptions } from './master-options.js';
import {broadcastUrl} from './broadcast.js';
import { getLanguage, t } from './i18n.js';
import { applyAppearance, makeCard, updateCardContent } from './overlay-shared.js';

const options = parseOptions(location.search);
options.lang = getLanguage();
applyAppearance(options);
const list = document.getElementById('tracks');
list.dataset.align = options.align;
const cards = new Map();
const reducedMotion = matchMedia('(prefers-reduced-motion: reduce)');
const valid = item => item && Number.isSafeInteger(item.entryId) && item.entryId > 0 &&
  Number.isSafeInteger(item.trackId) && item.trackId > 0 && item.trackId <= 4294967295;
let structure = '';
let lastHistory = [];
let lastDemo = false;

function createCard() { return makeCard(options); }

function updateCard(card, item, index, isCurrent, demo) {
  card.dataset.current = String(isCurrent);
  card.dataset.trackId = String(item.trackId);
  card.dataset.entryId = String(item.entryId);
  updateCardContent(card, item, options, {
    label: t(isCurrent ? 'masterLabel' : 'previousLabel', { id: item.id, index }),
    demo, timeline: isCurrent, coverUrl: '/api/master/covers/' + item.trackId
  });
}

function layoutCards() {
  const width = list.clientWidth;
  for (const card of cards.values()) {
    if (card.hasAttribute('aria-hidden')) continue;
    const scale = card.dataset.current === 'true' ? 1 : options.historyScale;
    const content = card.querySelector('.card');
    content.style.width = width + 'px';
    card.style.setProperty('--track-scale', String(scale));
    card.style.width = width * scale + 'px';
    // Reserve the scaled height in normal flow so small cards leave no empty slots.
    card.style.height = content.offsetHeight * scale + 'px';
  }
}

let layoutWidth = 0;
new ResizeObserver(() => {
  if (layoutWidth === list.clientWidth) return;
  layoutWidth = list.clientWidth;
  layoutCards();
}).observe(list);

function animate(card, keyframes, duration, finished) {
  if (!duration || !card.animate) { if (finished) finished(); return; }
  const animation = card.animate(keyframes, { duration, easing: 'cubic-bezier(.22, 1, .36, 1)' });
  animation.finished.then(() => { if (finished) finished(); }, () => {});
}

function render(data) {
  const current = ['connected', 'demo'].includes(data.status) && valid(data.current) ? data.current : null;
  const history = Array.isArray(data.history) ? data.history.filter(valid).slice(0, options.history) : [];
  lastHistory = history;
  lastDemo = data.demo === true;
  const rows = [...(current ? [{ item: current, current: true, rank: 0 }] : []),
    ...history.map((item, index) => ({ item, current: false, rank: index + 1 }))];
  const key = row => row.item.entryId + ':' + row.item.trackId;
  const signature = rows.map(row => key(row) + ':' + row.current).join('|');
  // Live BPM and late metadata update in-place; they never restart an animation.
  if (signature === structure) {
    for (const row of rows) updateCard(cards.get(key(row)), row.item, row.rank, row.current, lastDemo);
    layoutCards();
    return;
  }
  structure = signature;
  const before = new Map([...cards].map(([id, card]) => [id, {
    rect: card.getBoundingClientRect(), opacity: getComputedStyle(card).opacity
  }]));
  const duration = reducedMotion.matches ? 0 : options.duration;
  const desired = new Set(rows.map(key));
  for (const [id, card] of cards) {
    if (desired.has(id) || !card.hasAttribute('aria-hidden')) card.getAnimations().forEach(animation => animation.cancel());
  }
  const origin = list.getBoundingClientRect();
  for (const [id, card] of cards) if (!desired.has(id)) {
    // Already leaving: let its animation finish, even during a stream of changes.
    if (card.hasAttribute('aria-hidden')) continue;
    const previous = before.get(id);
    const scale = 'scale(' + previous.rect.width / card.offsetWidth + ',' + previous.rect.height / card.offsetHeight + ')';
    Object.assign(card.style, { position: 'absolute', top: previous.rect.top - origin.top + 'px', left: previous.rect.left - origin.left + 'px' });
    card.setAttribute('aria-hidden', 'true');
    const exit = options.align === 'right' ? 24 : -24;
    animate(card, [{ opacity: previous.opacity, transform: 'translateX(0) ' + scale }, { opacity: 0, transform: 'translateX(' + exit + 'px) ' + scale }], duration, () => {
      card.remove(); cards.delete(id);
    });
  }
  for (const row of rows) {
    const id = key(row);
    let card = cards.get(id);
    if (!card) { card = createCard(); cards.set(id, card); }
    card.removeAttribute('aria-hidden');
    Object.assign(card.style, { position: '', top: '', left: '', width: '' });
    updateCard(card, row.item, row.rank, row.current, lastDemo);
    list.append(card);
  }
  layoutCards();
  for (const row of rows) {
    const id = key(row), card = cards.get(id), previous = before.get(id);
    const rect = card.getBoundingClientRect();
    animate(card, [
      { transform: previous ? 'translate(' + (previous.rect.left - rect.left) + 'px,' + (previous.rect.top - rect.top) + 'px) scale(' + previous.rect.width / rect.width + ',' + previous.rect.height / rect.height + ')' : 'translateY(-22px)', opacity: previous ? previous.opacity : 0 },
      { transform: 'translate(0,0) scale(1,1)', opacity: 1 }
    ], duration);
  }
}

async function poll() {
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 1800);
  try {
    const response = await fetch(broadcastUrl('/api/master'), { cache: 'no-store', signal: controller.signal });
    if (!response.ok) throw new Error('HTTP ' + response.status);
    const data = await response.json();
    if (!data || !Array.isArray(data.history)) throw new Error('Invalid master feed');
    render(data);
  } catch (_) { render({ status: 'disconnected', current: null, history: lastHistory, demo: lastDemo }); }
  finally { clearTimeout(timeout); setTimeout(poll, 250); }
}
poll();
