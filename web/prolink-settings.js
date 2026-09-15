import { t, diagnostic, translate, locale } from './i18n.js';
import { appReady } from './navigation.js';
let active = false, busy = false, last = null, canControl = false;
const selected = new Set();
const byId = id => document.getElementById(id);
function error(message) { const el=byId('connection-error');el.hidden=!message;el.textContent=message; }
function render(state) {
  last = state;
  const connecting = state.status === 'connecting', connected = state.status === 'connected';
  byId('prolink-status').textContent = diagnostic(state.message);
  byId('discover').disabled = !canControl || busy || connected || connecting || state.runtimeAvailable === false;
  byId('connect').disabled = busy || connected || connecting || selected.size < 1 || selected.size > 4 || state.runtimeAvailable === false;
  byId('disconnect').disabled = !canControl || busy || state.status === 'stopped';
  const details=byId('network-details');details.replaceChildren();
  for(const [key,value] of [['prolinkInterface',state.networkInterface],['prolinkAddress',state.localAddress],['prolinkVirtualPlayer',state.virtualPlayer]]) {
    if(value===null || value===undefined || value==='')continue;
    const dt=document.createElement('dt'),dd=document.createElement('dd');dt.textContent=t(key);dd.textContent=String(value);details.append(dt,dd);
  }
  const devices=Array.isArray(state.devices)?state.devices:[];
  const available=new Set(devices.filter(d=>d.selectable).map(d=>d.number));
  for(const player of selected)if(!available.has(player)&&!connected&&!connecting)selected.delete(player);
  const container=byId('devices');const focused=document.activeElement?.dataset?.player;
  container.replaceChildren();
  for(const device of devices) {
    const card=document.createElement('article');card.className='device-card';card.dataset.selected=String(connected?device.selected===true:selected.has(device.number));
    const heading=document.createElement('h3'),address=document.createElement('p');heading.textContent=device.name;address.className='device-address';address.textContent=device.address+' · '+t('prolinkDeviceNumber',{number:device.number});card.append(heading,address);
    if(device.selectable){
      const label=document.createElement('label'),input=document.createElement('input'),text=document.createElement('span');input.type='checkbox';input.dataset.player=device.number;input.checked=connected||connecting?device.selected===true:selected.has(device.number);input.disabled=!canControl||busy||connected||connecting;
      const order=(connected||connecting?state.players||[]:[...selected].sort((a,b)=>a-b)).indexOf(device.number);
      text.textContent=order>=0?t('prolinkMappedDeck',{deck:order+1}):t('prolinkUsePlayer');
      input.addEventListener('change',()=>{if(input.checked)selected.add(device.number);else selected.delete(device.number);render(last);});label.append(input,text);card.append(label);
    } else {const text=document.createElement('p');text.textContent=t(device.supported?'prolinkMixerAutomatic':'prolinkUnsupportedDevice');card.append(text);}
    const badges=document.createElement('div');badges.className='device-badges';
    for(const [key,value] of [['prolinkPlaying',device.playing],['prolinkOnAir',device.onAir],['prolinkSync',device.synced],['prolinkMaster',device.master]]){
      if(typeof value!=='boolean')continue;const badge=document.createElement('span');badge.className='device-badge';badge.dataset.active=String(value);badge.textContent=t(key)+' · '+t(value?'prolinkYes':'prolinkNo');badges.append(badge);
    }
    if(Number.isFinite(device.bpm)){const badge=document.createElement('span');badge.className='device-badge';badge.textContent=new Intl.NumberFormat(locale(),{maximumFractionDigits:2}).format(device.bpm)+' BPM';badges.append(badge);}
    if(typeof device.firmware==='string'){const badge=document.createElement('span');badge.className='device-badge';badge.textContent=t('prolinkFirmware')+' '+device.firmware;badges.append(badge);}
    card.append(badges);container.append(card);
  }
  if(focused)container.querySelector('[data-player="'+CSS.escape(focused)+'"]')?.focus({preventScroll:true});
  byId('no-devices').hidden=devices.length>0;
  byId('connect').disabled=!canControl||busy||connected||connecting||selected.size<1||selected.size>4||state.runtimeAvailable===false;
  if(state.runtimeAvailable===false)error(t('prolinkRuntimeMissing'));
}
async function poll() {
  if(!active)return;
  try {const response=await fetch('/api/prolink/devices',{cache:'no-store',signal:AbortSignal.timeout(2500)});if(!response.ok)throw new Error(t('prolinkReadFailed'));render(await response.json());}
  catch(err){error(err.message);byId('connect').disabled=true;byId('prolink-status').textContent=t('disconnected');byId('devices').querySelectorAll('.device-badge').forEach(badge=>badge.dataset.active='false');}
}
async function command(action) {
  if(!active||!canControl||busy)return;busy=true;error('');if(last)render(last);
  try {
    const body={action};if(action==='connect')body.players=[...selected].sort((a,b)=>a-b);
    const response=await fetch('/api/prolink/control',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body),signal:AbortSignal.timeout(4000)});
    const result=await response.json();if(!response.ok)throw new Error(diagnostic(result.error));
    await poll();
  } catch(err){error(err.message);} finally{busy=false;if(last)render(last);}
}
for(const action of ['discover','connect','disconnect'])byId(action).addEventListener('click',()=>command(action));
function mode(app){
  active=app?.mode==='prolink'&&app.capabilities?.prolinkSetup===true;canControl=!!app&&app.canControl!==false;
  byId('remote-readonly').hidden=canControl;byId('prolink-content').hidden=!active;byId('mode-unavailable').hidden=active;
  if(last)render(last);
  if(!active||!canControl||!last){for(const action of ['discover','connect','disconnect'])byId(action).disabled=true;byId('devices').querySelectorAll('[data-player]').forEach(input=>input.disabled=true);}
  if(active)poll();
}
window.addEventListener('appmodechange',event=>mode(event.detail));
window.addEventListener('languagechange',()=>{translate();if(last)render(last);});
mode(await appReady);setInterval(poll,1000);
