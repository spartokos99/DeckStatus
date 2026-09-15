import {t,translate,diagnostic} from './i18n.js';
import {appReady} from './navigation.js';
const $=id=>document.getElementById(id);
let state=null,busy=false,initialized=false,failed=false,disposed=false,timer,feedback='';
function message(key=''){feedback=key;$('network-feedback').textContent=key?t(key):'';}
function accessChanged(){
  const local=$('network-access').value==='local';
  $('network-interface-row').hidden=$('network-access').value!=='interface';
  $('network-control').disabled=local;
}
function adapters(preferred=$('network-interface').value){
  const select=$('network-interface');select.replaceChildren();
  for(const item of state?.interfaces||[])select.add(new Option(item.name+' · '+item.address,item.address));
  if(preferred&&!Array.from(select.options).some(option=>option.value===preferred))select.add(new Option(preferred+' · '+t('networkAdapterMissing'),preferred));
  if(preferred)select.value=preferred;
}
function fill(){
  const saved=state.saved;
  $('network-access').value=saved.bind.startsWith('127.')?'local':saved.bind==='0.0.0.0'?'lan':'interface';
  adapters($('network-access').value==='interface'?saved.bind:'');
  $('network-port').value=saved.port;$('network-control').checked=saved.allowRemoteControl;accessChanged();
}
function render(){
  $('network-fields').disabled=busy||failed||!state?.canConfigure;
  if(!state)return;
  $('network-local-only').hidden=state.canConfigure;
  $('network-restart').hidden=!state.restartRequired;$('network-overrides').hidden=!state.commandLineOverrides;
  const active=state.active,local=active.bind.startsWith('127.');
  $('network-active').textContent=t(local?'networkLocal':active.bind==='0.0.0.0'?'networkLan':'networkInterface');
  const details=$('network-details');details.replaceChildren();
  for(const [key,value] of [['networkBind',active.bind],['networkPort',active.port],['networkRemoteControl',t(active.allowRemoteControl?'networkAllowed':'networkDisabled')]]){
    const dt=document.createElement('dt'),dd=document.createElement('dd');dt.textContent=t(key);dd.textContent=String(value);details.append(dt,dd);
  }
  const container=$('network-urls');container.replaceChildren();
  for(const url of state.urls||[]){
    if(!state.canConfigure&&new URL(url).hostname.startsWith('127.'))continue;
    const row=document.createElement('div'),input=document.createElement('input'),copy=document.createElement('button');
    row.className='network-url';input.readOnly=true;input.value=url;input.setAttribute('aria-label',t('networkServerAddress'));copy.textContent=t('copyUrl');copy.type='button';
    copy.addEventListener('click',async()=>{try{await navigator.clipboard.writeText(url);message('networkCopied');}catch{input.select();message('waveCopyManual');}});
    row.append(input,copy);container.append(row);
  }
  $('network-no-lan').hidden=!local&&(state.urls||[]).some(url=>!new URL(url).hostname.startsWith('127.'));
  $('network-feedback').textContent=feedback?t(feedback):'';
}
async function load(){
  try{
    const response=await fetch('/api/network',{cache:'no-store',signal:AbortSignal.timeout(4000)});
    if(!response.ok)throw Error('networkUnavailable');
    state=await response.json();failed=false;$('network-error').hidden=true;
    if(!initialized){fill();initialized=true;}else adapters();render();
  }catch(error){failed=true;$('network-error').textContent=diagnostic(error.message);$('network-error').hidden=false;render();}
}
$('network-access').addEventListener('change',accessChanged);
$('network-save').addEventListener('click',async()=>{
  if(busy||failed||!state?.canConfigure||!$('network-port').reportValidity())return;
  const access=$('network-access').value;
  const body={bind:access==='local'?'127.0.0.1':access==='lan'?'0.0.0.0':$('network-interface').value,port:Number($('network-port').value),allowRemoteControl:$('network-control').checked};
  busy=true;message('');render();
  try{
    const response=await fetch('/api/network',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body),signal:AbortSignal.timeout(5000)});
    const result=await response.json();if(!response.ok)throw Error(result.error||'networkSaveFailed');
    state=result;$('network-error').hidden=true;message(result.restartRequired?'networkSavedRestart':'networkSaved');
  }catch(error){$('network-error').textContent=diagnostic(error.message);$('network-error').hidden=false;}
  finally{busy=false;render();}
});
window.addEventListener('languagechange',()=>{translate();adapters();render();document.title=t('navNetwork')+' · DeckStatus';});
window.addEventListener('pagehide',()=>{disposed=true;clearTimeout(timer);});
async function poll(){if(!busy)await load();if(!disposed)timer=setTimeout(poll,5000);}
await appReady;translate();document.title=t('navNetwork')+' · DeckStatus';await poll();
