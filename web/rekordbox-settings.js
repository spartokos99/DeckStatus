import {t, diagnostic} from './i18n.js';
import {appReady} from './navigation.js';
import { poll as startPolling } from './poll.js';
let enabled=false,last;
function render(state){last=state;document.getElementById('connection-status').textContent=diagnostic(state.message);const details=document.getElementById('connection-details');details.replaceChildren();for(const[key,value]of [['version',state.version],['cover',state.artworkStatus]]){const dt=document.createElement('dt'),dd=document.createElement('dd');dt.textContent=t(key);dd.textContent=diagnostic(value);details.append(dt,dd);}}
async function poll(){if(!enabled)return;try{const response=await fetch('/api/rekordbox/status',{cache:'no-store',signal:AbortSignal.timeout(2500)});if(!response.ok)throw new Error();render(await response.json());}catch{document.getElementById('connection-status').textContent=t('disconnected');}}
function mode(app){enabled=app?.mode==='rekordbox';document.getElementById('rekordbox-content').hidden=!enabled;document.getElementById('mode-unavailable').hidden=enabled;if(enabled)poll();}
window.addEventListener('appmodechange',event=>mode(event.detail));window.addEventListener('languagechange',()=>{if(last)render(last);});mode(await appReady);startPolling(poll,{interval:1000,timeout:0,immediate:false});
