import { t, diagnostic, translate, locale } from './i18n.js';
import { appReady } from './navigation.js';
import { poll as startPolling } from './poll.js';
let active = false, busy = false, last = null, canControl = false, fetching = false;
const selected = new Map();
let savedDevices=[],settingsLoaded=false;
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
  const devices=[...(Array.isArray(state.devices)?state.devices:[])];
  const available=new Set(devices.filter(d=>d.selectable).map(d=>d.number));
  for(const device of savedDevices)if(selected.has(device.player)&&!devices.some(d=>d.number===device.player))devices.push({number:device.player,name:device.name,address:'',selectable:true,missing:true});
  const connectedMapping=state.mapping||((state.players||[]).map((player,index)=>({player,deck:index+1})));
  const assignments=connected||connecting?new Map(connectedMapping.map(d=>[d.player,d.deck])):selected;
  byId('auto-connect').disabled=!canControl||busy||!settingsLoaded;
  byId('save-selection').disabled=!canControl||busy||!settingsLoaded;
  byId('connect').disabled=!canControl||busy||connected||connecting||selected.size<1||selected.size>4||[...selected.keys()].some(player=>!available.has(player))||state.runtimeAvailable===false||!settingsLoaded;
  // Keep an open native select intact while status/BPM updates arrive.
  if(document.activeElement?.matches('#devices select'))return;
  const container=byId('devices');const focused=document.activeElement?.dataset?.player;
  container.replaceChildren();
  for(const device of devices) {
    const card=document.createElement('article');card.className='device-card';card.dataset.selected=String(assignments.has(device.number));
    const heading=document.createElement('h3'),address=document.createElement('p');heading.textContent=device.name;address.className='device-address';address.textContent=device.address+' · '+t('prolinkDeviceNumber',{number:device.number});card.append(heading,address);
    if(device.selectable){
      const label=document.createElement('label'),input=document.createElement('input'),text=document.createElement('span');input.type='checkbox';input.dataset.player=device.number;input.checked=assignments.has(device.number);input.disabled=!canControl||busy||connected||connecting||(!input.checked&&selected.size>=4);
      text.textContent=t('prolinkUsePlayer');
      input.addEventListener('change',()=>{if(input.checked){const free=[1,2,3,4].find(deck=>![...selected.values()].includes(deck));if(free)selected.set(device.number,free);}else selected.delete(device.number);render(last);});label.append(input,text);card.append(label);
      if(assignments.has(device.number)){
        const row=document.createElement('label'),caption=document.createElement('span'),select=document.createElement('select');caption.textContent=t('prolinkAssignDeck');row.className='prolink-deck-choice';select.dataset.deckPlayer=device.number;
        for(const deck of [1,2,3,4])if(deck===assignments.get(device.number)||![...assignments.values()].includes(deck))select.add(new Option(t('deck',{id:deck}),deck));
        select.value=assignments.get(device.number);select.disabled=!canControl||busy||connected||connecting;
        select.addEventListener('change',()=>{selected.set(device.number,Number(select.value));select.blur();render(last);});
        select.addEventListener('blur',()=>queueMicrotask(()=>render(last)));row.append(caption,select);card.append(row);
      }
      if(device.missing){const missing=document.createElement('p');missing.className='hint';missing.textContent=t('prolinkDeviceMissing');card.append(missing);}
    } else {const text=document.createElement('p');text.textContent=t(device.supported?'prolinkMixerAutomatic':'prolinkUnsupportedDevice');card.append(text);}
    if(['prolinkAzHelp','prolink3000xHelp','prolinkMixerHelp'].includes(device.guidance)) {const help=document.createElement('p');help.className='hint';help.textContent=t(device.guidance);card.append(help);}
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
  if(state.runtimeAvailable===false)error(t('prolinkRuntimeMissing'));
}
async function poll() {
  if(!active||fetching)return;fetching=true;
  try {if(!settingsLoaded)await loadSelection();const response=await fetch('/api/prolink/devices',{cache:'no-store',signal:AbortSignal.timeout(2500)});if(!response.ok)throw new Error(t('prolinkReadFailed'));render(await response.json());}
  catch(err){error(err.message);byId('connect').disabled=true;byId('prolink-status').textContent=t('disconnected');byId('devices').querySelectorAll('.device-badge').forEach(badge=>badge.dataset.active='false');}
  finally{fetching=false;}
}
async function command(action) {
  if(!active||!canControl||busy)return;busy=true;error('');if(last)render(last);
  try {
    const body={action};if(action==='connect'){await saveSelection();body.mapping=[...selected].map(([player,deck])=>({player,deck}));}
    const response=await fetch('/api/prolink/control',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body),signal:AbortSignal.timeout(4000)});
    const result=await response.json();if(!response.ok)throw new Error(diagnostic(result.error));
    await poll();
  } catch(err){error(err.message);} finally{busy=false;if(last)render(last);}
}
async function saveSelection(){
  const devices=[...selected].map(([player,deck])=>({player,deck,name:last?.devices?.find(d=>d.number===player)?.name||savedDevices.find(d=>d.player===player)?.name}));
  const response=await fetch('/api/prolink/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({autoConnect:byId('auto-connect').checked,devices}),signal:AbortSignal.timeout(4000)});
  const result=await response.json();if(!response.ok)throw Error(diagnostic(result.error));savedDevices=result.devices;
  byId('selection-status').textContent=t('prolinkSelectionSaved');
}
byId('save-selection').addEventListener('click',async()=>{if(!canControl||busy)return;busy=true;render(last);try{await saveSelection();error('');}catch(err){error(err.message);}finally{busy=false;render(last);}});
async function loadSelection(){const response=await fetch('/api/prolink/settings',{cache:'no-store',signal:AbortSignal.timeout(2500)});if(!response.ok)throw Error(t('prolinkReadFailed'));const value=await response.json();savedDevices=value.devices||[];for(const device of savedDevices)selected.set(device.player,device.deck);byId('auto-connect').checked=value.autoConnect===true;settingsLoaded=true;}
for(const action of ['discover','connect','disconnect'])byId(action).addEventListener('click',()=>command(action));
function mode(app){
  active=app?.mode==='prolink'&&app.capabilities?.prolinkSetup===true;canControl=!!app&&app.canControl!==false;
  byId('remote-readonly').hidden=canControl;byId('prolink-content').hidden=!active;byId('mode-unavailable').hidden=active;
  if(last)render(last);
  if(!active||!canControl||!last){for(const action of ['discover','connect','disconnect','save-selection'])byId(action).disabled=true;byId('auto-connect').disabled=true;byId('devices').querySelectorAll('input,select').forEach(input=>input.disabled=true);}
  if(active)poll();
}
window.addEventListener('appmodechange',event=>mode(event.detail));
window.addEventListener('languagechange',()=>{translate();if(last)render(last);});
mode(await appReady);
// setInterval could stack requests on a slow reply; this waits for each run to finish.
startPolling(poll,{interval:1000,timeout:0,immediate:false});
