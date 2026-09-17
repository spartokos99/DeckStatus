import {api} from './auth.js';
import {t,translate} from './i18n.js';
const $=id=>document.getElementById(id),pane=document.querySelector('[data-pane=twitch]');
let state,config,draft=false,busy=false,disposed=false;
function controls(){pane.querySelectorAll('input,button').forEach(node=>node.disabled=busy||!state?.canControl);$('twitch-dirty').hidden=!draft;$('twitch-readonly').hidden=!!state?.canControl;}
function render(){if(!state)return;$('twitch-error').textContent=t(state.error||'');for(const role of ['streamer','bot']){
  $('twitch-'+role+'-name').textContent=state.accounts?.[role]?.login||t('twitchNotLinked');
  const container=$('twitch-'+role+'-pending');container.replaceChildren();const pending=state.pending?.[role];
  if(pending){const code=document.createElement('code'),link=document.createElement('a');code.className='twitch-code';code.textContent=pending.code;link.textContent=t('twitchOpenAuth');link.href=pending.url;link.target='_blank';link.rel='noopener noreferrer';container.append(code,link);}
}controls();}
async function load(reload=false){state=await api('/api/admin/twitch');if(!config||reload||!draft){config={clientId:state.settings.clientId,revision:state.settings.revision};$('twitch-client').value=config.clientId;draft=false;}render();}
async function run(fn){if(busy)return;busy=true;controls();try{$('twitch-message').textContent='';await fn();}catch(error){$('twitch-message').textContent=error.message;}finally{busy=false;controls();}}
$('twitch-client').addEventListener('input',()=>{config.clientId=$('twitch-client').value.trim();draft=true;controls();});
$('twitch-save').addEventListener('click',()=>run(async()=>{await api('/api/admin/twitch',{action:'saveConnection',settings:config});await load(true);$('twitch-message').textContent=t('portalSaved');}));
$('twitch-reload').addEventListener('click',()=>{if(!draft||confirm(t('sceneDiscard')))run(()=>load(true));});
for(const role of ['streamer','bot'])for(const action of ['authorize','unlink'])$('twitch-'+role+'-'+action).addEventListener('click',()=>run(async()=>{if(draft)throw Error(t('twitchSaveFirst'));await api('/api/admin/twitch',{action,role});$('twitch-message').textContent=t('twitchQueued');}));
window.addEventListener('languagechange',()=>{translate();render();});window.addEventListener('beforeunload',event=>{if(draft){event.preventDefault();event.returnValue='';}});window.addEventListener('pagehide',()=>disposed=true);
translate();await run(()=>load());
async function poll(){if(disposed)return;if(!busy&&!pane.hidden)try{await load();}catch(error){$('twitch-message').textContent=error.message;}if(!disposed)setTimeout(poll,2000);}setTimeout(poll,2000);
