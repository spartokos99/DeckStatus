import { t, translate, setLanguage } from './i18n.js';

const header = document.querySelector('header');
const groups = [
  ['navMonitor', [['dashboard','/','dashboard','▦'],['fullHistory','/history','history','◷'],['navApi','/api/state','dashboard','↗']]],
  ['navOverlays', [['deckSettings','/overlay/settings','deckOverlays','▤'],['masterSettings','/master-overlay/settings','masterOverlay','♔'],['waveNav','/waveform/settings','audioWaveform','∿']]],
  ['navSources', [['navRekordbox','/rekordbox/settings','rekordboxSetup','●'],['navProlink','/prolink/settings','prolinkSetup','⌁']]]
];
let app = null;
if (header) {
  header.classList.add('app-header');
  header.innerHTML = '<div class="nav-top"><a class="brand" href="/"><img src="/icon.svg" width="40" height="40" alt=""><span><strong>DeckStatus</strong><small data-i18n="navTagline"></small></span></a><div class="nav-tools"><span class="mode-pill" id="app-mode" role="status"></span><select data-language data-i18n-aria="language"><option value="en">English</option><option value="de">Deutsch</option></select></div></div><nav class="app-nav" data-i18n-aria="navMain"></nav>';
  const nav = header.querySelector('nav');
  for (const [heading, items] of groups) {
    const group = document.createElement('div'); group.className = 'nav-group';
    const label = document.createElement('span'); label.className = 'nav-group-label'; label.dataset.i18n = heading;
    group.append(label);
    const links = document.createElement('div'); links.className = 'nav-links'; group.append(links);
    for (const [key, href, capability, icon] of items) {
      const link = document.createElement('a'); link.dataset.href = href; link.dataset.capability = capability; link.className = 'nav-link';
      const symbol = document.createElement('span'); symbol.className = 'nav-symbol'; symbol.ariaHidden = 'true'; symbol.textContent = icon;
      const text = document.createElement('span'); text.dataset.i18n = key; link.append(symbol,text);
      if (location.pathname === href) link.setAttribute('aria-current','page');
      if (href === '/api/state') { link.target = '_blank'; link.rel = 'noopener'; }
      links.append(link);
    }
    nav.append(group);
  }
  header.querySelector('[data-language]').addEventListener('change', event => setLanguage(event.target.value));
}
function render() {
  if (!header) return;
  translate(header);
  const mode = header.querySelector('#app-mode');
  mode.textContent = app ? t('navMode') + ' · ' + (app.mode === 'prolink' ? 'ProLink' : 'Rekordbox') : t('navUnavailable');
  mode.dataset.mode = app?.mode || 'unknown';
  header.querySelectorAll('[data-capability]').forEach(link => {
    const enabled = app?.capabilities?.[link.dataset.capability] === true;
    link.setAttribute('aria-disabled', String(!enabled));
    if (enabled) { link.href = link.dataset.href; link.removeAttribute('tabindex'); link.removeAttribute('title'); }
    else { link.removeAttribute('href'); link.tabIndex = -1; link.title = t(app ? 'modeUnavailable' : 'navUnavailable'); }
  });
}
window.addEventListener('languagechange',render);
render();
export async function refreshApp() {
  try {
    const response = await fetch('/api/app',{cache:'no-store',signal:AbortSignal.timeout(2500)});
    if (!response.ok) throw new Error('App status unavailable');
    const data = await response.json();
    if (!['rekordbox','prolink'].includes(data.mode) || typeof data.capabilities !== 'object') throw new Error('Invalid mode');
    app = data;
  } catch { app = null; }
  render();
  window.dispatchEvent(new CustomEvent('appmodechange',{detail:app}));
  return app;
}
export const appReady = refreshApp();
setInterval(refreshApp,5000);
