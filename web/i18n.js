const dictionaries = {};
for (const code of ['en', 'de']) {
  const response = await fetch('/locales/' + code + '.json', { cache: 'no-store' });
  if (!response.ok) throw new Error('Language file unavailable: ' + code);
  dictionaries[code] = await response.json();
}
let saved;
try { saved = localStorage.getItem('rb.language'); } catch (_) {}
let language = new URLSearchParams(location.search).get('lang') || saved || 'en';
if (!Object.hasOwn(dictionaries, language)) language = 'en';
export const getLanguage = () => language;
export const locale = () => language === 'de' ? 'de-AT' : 'en-GB';
export function t(key, values = {}) {
  let value = dictionaries[language][key] ?? dictionaries.en[key] ?? key;
  for (const [name, replacement] of Object.entries(values)) value = value.replaceAll('{' + name + '}', String(replacement));
  return value;
}
export function diagnostic(value) {
  if (typeof value !== 'string' || !value.trim()) return '—';
  const key = Object.hasOwn(dictionaries.en, value) ? value : Object.keys(dictionaries.en).find(key => dictionaries.en[key] === value);
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
  language = Object.hasOwn(dictionaries, code) ? code : 'en';
  try { localStorage.setItem('rb.language', language); } catch (_) {}
  translate();
  window.dispatchEvent(new Event('languagechange'));
}
translate();
document.querySelectorAll('[data-language]').forEach(el => el.addEventListener('change', () => setLanguage(el.value)));
