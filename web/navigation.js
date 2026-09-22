import { t, translate, setLanguage } from './i18n.js';
import {api} from './auth.js';
import { poll as startPolling } from './poll.js';

const header = document.querySelector('header');
const groups = [
  ['navMonitor', [['dashboard','/','dashboard','▦'],['navApi','/api/state','dashboard','↗']]],
  ['navOverlays', [['navScenes','/scenes','scenes','▧'],['navAutomations','/automations','admin','↯'],['fullHistory','/history','history','◷']]],
  ['navSources', [['navRekordbox','/rekordbox/settings','rekordboxSetup','●'],['navProlink','/prolink/settings','prolinkSetup','⌁'],['navNetwork','/network/settings','networkSettings','⇄']]]
];
const components = [['deckSettings','/overlay/settings','deckOverlays','▤'],['masterSettings','/master-overlay/settings','masterOverlay','♔'],['waveNav','/waveform/settings','audioWaveform','∿'],['creativeText','/components/text','scenes','T'],['creativeImage','/components/image','scenes','▧'],['creativeFx','/components/fx','scenes','✧']];
function navLink([key,href,capability,icon]) {
  const link=document.createElement('a');link.dataset.href=href;link.dataset.capability=capability;link.className='nav-link';
  const symbol=document.createElement('span');symbol.className='nav-symbol';symbol.ariaHidden='true';symbol.textContent=icon;
  const text=document.createElement('span');text.dataset.i18n=key;link.append(symbol,text);
  if(location.pathname===href)link.setAttribute('aria-current','page');
  if(href==='/api/state'){link.target='_blank';link.rel='noopener';}
  return link;
}
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
    for (const item of items) links.append(navLink(item));
    if (heading === 'navOverlays') {
      const dropdown=document.createElement('details');dropdown.className='nav-components';
      dropdown.innerHTML='<summary aria-controls="nav-components-menu" aria-expanded="false"><span class="nav-symbol" aria-hidden="true">▤</span><span data-i18n="navComponents"></span><span class="nav-chevron" aria-hidden="true">⌄</span></summary><div id="nav-components-menu" class="nav-components-menu"></div>';
      const summary=dropdown.querySelector('summary'),menu=dropdown.querySelector('.nav-components-menu');
      for(const item of components)menu.append(navLink(item));
      dropdown.dataset.current=String(components.some(item=>item[1]===location.pathname));
      dropdown.addEventListener('toggle',()=>summary.setAttribute('aria-expanded',String(dropdown.open)));
      dropdown.addEventListener('keydown',event=>{
        if(event.key==='Escape'&&dropdown.open){event.preventDefault();dropdown.open=false;summary.focus();}
        if(event.key==='ArrowDown'&&event.target===summary){event.preventDefault();dropdown.open=true;menu.querySelector('a[href]')?.focus();}
      });
      dropdown.addEventListener('click',event=>{if(event.target.closest('a[href]'))dropdown.open=false;});
      for(const name of ['click','focusin'])document.addEventListener(name,event=>{if(!dropdown.contains(event.target))dropdown.open=false;});
      links.append(dropdown);
    }
    nav.append(group);
  }
  const admin=navLink(['navAdmin','/admin','admin','⚙']);admin.classList.add('nav-standalone');nav.append(admin);
  header.querySelector('[data-language]').addEventListener('change', event => setLanguage(event.target.value));
  const account=document.createElement('a');account.id='nav-account';account.className='nav-account';header.querySelector('.nav-tools').append(account);
  const logout=document.createElement('button');logout.id='nav-logout';logout.dataset.i18n='authSignOut';logout.hidden=true;logout.addEventListener('click',async()=>{try{await api('/api/auth/logout',{});location.assign('/login');}catch(error){logout.title=error.message;}});header.querySelector('.nav-tools').append(logout);
}
function render() {
  if (!header) return;
  translate(header);
  const mode = header.querySelector('#app-mode');
  mode.textContent = app ? t('navMode') + ' · ' + (app.mode === 'prolink' ? 'ProLink' : 'Rekordbox') : t('navUnavailable');
  mode.dataset.mode = app?.mode || 'unknown';
  const publicView=app?.public===true;
  header.querySelector('nav').hidden=publicView;
  header.querySelector('.brand').href=publicView?'/history':'/';
  if(publicView){mode.textContent=t('authPublicHistory');mode.dataset.mode='public';}
  const account=header.querySelector('#nav-account');account.textContent=app?.user?.username||t('authSignIn');account.href=app?.user?'/account/password':'/login';
  header.querySelector('#nav-logout').hidden=!app?.user;
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
    if(location.pathname==='/history'){
      const identity=await fetch('/api/auth/me',{cache:'no-store'});
      if(identity.ok){const user=(await identity.json()).user;if(!user||user.mustChangePassword){app={public:true,user};render();return app;}}
    }
    const response = await fetch('/api/app',{cache:'no-store',signal:AbortSignal.timeout(2500)});
    if(response.status===401&&location.pathname!='/history')location.assign('/login');
    if(response.status===403){const error=await response.json();if(error.error==='authPasswordRequired')location.assign('/account/password');throw Error('Access unavailable');}
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
// Paused while the tab is hidden: a background settings page needs no heartbeat.
startPolling(()=>refreshApp(),{interval:5000,timeout:0,immediate:false});
