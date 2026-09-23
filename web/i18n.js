import { readSetting, writeSetting } from './storage.js';

// Only the language actually being displayed blocks startup. Renderer documents
// (overlay, waveform, scene, component) never load a second file at all; the
// application pages prefetch the other language in the background so switching
// stays instant. Responses are revalidated, so a repeat visit costs one 304.
const dictionaries = {};
const requests = {};
const supported = ['en', 'de'];

function load(code) {
  if (dictionaries[code]) return Promise.resolve(dictionaries[code]);
  if (!requests[code]) requests[code] = fetch('/locales/' + code + '.json')
    .then(response => { if (!response.ok) throw new Error('Language file unavailable: ' + code); return response.json(); })
    .then(data => (dictionaries[code] = data))
    .catch(error => { delete requests[code]; throw error; });
  return requests[code];
}

let saved;
try { saved = readSetting('deckstatus.language'); } catch (_) {}
let language = new URLSearchParams(location.search).get('lang') || saved || 'en';
if (!supported.includes(language)) language = 'en';
await load(language);

let reverseEnglish;
const english = () => dictionaries.en;

export const getLanguage = () => language;
export const locale = () => language === 'de' ? 'de-AT' : 'en-GB';

export function t(key, values = {}) {
  let value = dictionaries[language]?.[key] ?? english()?.[key] ?? key;
  for (const [name, replacement] of Object.entries(values)) value = value.replaceAll('{' + name + '}', String(replacement));
  return value;
}

// Server diagnostics arrive as English text or as a key. The reverse index is built
// once instead of scanning every translation on each render.
export function diagnostic(value) {
  if (typeof value !== 'string' || !value.trim()) return '—';
  const source = english();
  if (!source) return value;
  if (Object.hasOwn(source, value)) return t(value);
  // Preserve technical profile/RVA details after a translated diagnostic.
  const details = value.indexOf(' [');
  if (details > 0 && Object.hasOwn(source, value.slice(0, details)))
    return t(value.slice(0, details)) + value.slice(details);
  if (!reverseEnglish) {
    reverseEnglish = new Map();
    for (const [key, text] of Object.entries(source)) if (!reverseEnglish.has(text)) reverseEnglish.set(text, key);
  }
  const key = reverseEnglish.get(value);
  return key ? t(key) : value;
}

export function translate(root = document) {
  document.documentElement.lang = language;
  root.querySelectorAll('[data-i18n]').forEach(el => { el.textContent = t(el.dataset.i18n); });
  root.querySelectorAll('[data-i18n-aria]').forEach(el => { el.setAttribute('aria-label', t(el.dataset.i18nAria)); });
  root.querySelectorAll('[data-i18n-title]').forEach(el => { el.title = t(el.dataset.i18nTitle); });
  root.querySelectorAll('[data-language]').forEach(el => { el.value = language; });
}

export function setLanguage(code) {
  const next = supported.includes(code) ? code : 'en';
  const apply = () => {
    language = next;
    try { writeSetting('deckstatus.language', language); } catch (_) {}
    translate();
    window.dispatchEvent(new Event('languagechange'));
  };
  // The prefetched language applies in the same task, so a switch never flashes.
  if (dictionaries[next]) { apply(); return Promise.resolve(); }
  return load(next).then(apply, () => {});
}

translate();
document.querySelectorAll('[data-language]').forEach(el => el.addEventListener('change', () => setLanguage(el.value)));
// Application pages own a header or a language selector; renderer documents own neither.
if (document.querySelector('header, [data-language]')) {
  for (const code of supported) if (code !== language) load(code).catch(() => {});
}
