import { parseOptions } from './master-options.js';
import { t, getLanguage } from './i18n.js';
import { applyAppearance, makeCard, updateCardContent } from './overlay-shared.js';

const query = new URLSearchParams(location.search);
const options = parseOptions(location.search);
options.lang = getLanguage();
applyAppearance(options);
const list = document.getElementById('tracks');
list.dataset.align = options.align;
const validDeck = query.getAll('deck').length <= 1 && (!query.has('deck') || /^[1-4]$/.test(query.get('deck')));
document.getElementById('error').hidden = validDeck;
let card = null, trackId = null;
const reducedMotion = matchMedia('(prefers-reduced-motion: reduce)');
function render(state) {
  const item = ['connected', 'demo'].includes(state.status) && Array.isArray(state.decks)
    ? state.decks.find(deck => deck && deck.id === options.deck && deck.loaded === true && Number.isSafeInteger(deck.trackId) && deck.trackId > 0 && deck.trackId <= 4294967295) : null;
  if (!item) { card?.remove(); card = null; trackId = null; return; }
  if (trackId !== item.trackId) {
    // One current and at most one leaving card, even during rapid load changes.
    list.querySelectorAll('[aria-hidden]').forEach(node => node.remove());
    if (card) {
      const leaving = card;
      leaving.setAttribute('aria-hidden', 'true');
      Object.assign(leaving.style, { position: 'absolute', top: '0', left: '0' });
      if (options.duration && !reducedMotion.matches) leaving.animate([{ opacity: 1 }, { opacity: 0 }], { duration: options.duration }).finished.then(() => leaving.remove(), () => leaving.remove());
      else leaving.remove();
    }
    card = makeCard(options); list.append(card); trackId = item.trackId;
    // IDs retained for integrations that inspect the standalone deck overlay.
    card.querySelector('[data-field="title"]').id = 'title';
    card.querySelector('[data-value="bpm"]').id = 'bpm';
    card.querySelector('[data-value="originalBpm"]').id = 'original-bpm';
    if (options.duration && !reducedMotion.matches) card.animate([{ opacity: 0, transform: 'translateY(8px)' }, { opacity: 1, transform: 'translateY(0)' }], { duration: options.duration, easing: 'ease-out' });
  }
  card.dataset.trackId = item.trackId;
  card.dataset.current = 'true';
  updateCardContent(card, item, options, { label: t('deck', { id: options.deck }), demo: state.demo === true,
    timeline: true, coverUrl: '/api/decks/' + options.deck + '/cover?trackId=' + item.trackId });
}
async function poll() {
  try {
    const response = await fetch('/api/state', { cache: 'no-store', signal: AbortSignal.timeout(1800) });
    if (!response.ok) throw new Error('HTTP ' + response.status);
    render(await response.json());
  } catch (_) { render({ status: 'disconnected' }); }
  finally { setTimeout(poll, 250); }
}
if (validDeck) poll();
