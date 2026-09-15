import {t,translate,getLanguage,locale} from './i18n.js';
import {readSetting,writeSetting} from './storage.js';
import {controls,defaults,presets,normalize,overlayUrl} from './waveform-options.js';
const $=id=>document.getElementById(id), key='deckstatus.waveform.options';
let saved={};try{saved=JSON.parse(readSetting(key)||'{}');}catch{}
let options=normalize(saved), devices=[], deviceError, audioState={status:'stopped'}, busy=false, previewTimer;
const main=new Set(['mode','width','height','color','color2','background','opacity','gradient']);
const signal=new Set(['channel','gain','smoothing','gate','minHz','maxHz','historySeconds']);
for(const [name,type,a,b,step] of controls){
  const row=document.createElement('div');row.className='setting';
  const label=document.createElement('label');label.htmlFor=name;
  const text=document.createElement('span');text.dataset.i18n='wave_'+name;label.append(text);
  const out=document.createElement('output');out.id=name+'-value';label.append(out);
  const input=document.createElement(type==='select'?'select':'input');input.id=name;input.dataset.option='';
  if(type==='select')for(const value of a){const option=new Option('',value);option.dataset.i18n=value==='bars'?'waveMode_bars':'wave_'+value;input.add(option);}
  else{input.type=type;if(type==='range'||type==='number'){input.min=a;input.max=b;input.step=step;}}
  row.append(label,input);$(main.has(name)?'main-controls':signal.has(name)?'signal-controls':'detail-controls').append(row);
  input.addEventListener('input',()=>{options=normalize({...options,[name]:type==='checkbox'?input.checked:input.value});$('preset').value='custom';sync(false);});
}
for(const name of ['custom',...Object.keys(presets)]){const option=new Option('',name);option.dataset.i18n=name==='custom'?'custom':'wavePreset_'+name;$('preset').add(option);}
function sync(fill=true){
  for(const [name,type]of controls){const input=$(name);if(fill){if(type==='checkbox')input.checked=options[name];else input.value=options[name];}
    $(name+'-value').textContent=type==='range'?Number(options[name]).toLocaleString(locale()):'';}
  $('maxHz').value=options.maxHz;
  writeSetting(key,JSON.stringify(options));
  const url=overlayUrl(options,getLanguage());$('url').value=url;$('open').href=url;
  $('dimensions').textContent=options.width+' × '+options.height+' px';
  $('preview').width=options.width;$('preview').height=options.height;
  clearTimeout(previewTimer);previewTimer=setTimeout(()=>{$('preview').src=url;},120);
}
$('preset').addEventListener('change',()=>{if(Object.hasOwn(presets,$('preset').value)){options=normalize(presets[$('preset').value]);sync();}});
$('reset').addEventListener('click',()=>{options={...defaults};$('preset').value='mint';sync();});
$('copy').addEventListener('click',async()=>{try{await navigator.clipboard.writeText($('url').value);$('feedback').textContent=t('waveCopied');}catch{$('url').select();$('feedback').textContent=t('waveCopyManual');}});
function renderDevices(){
  const previous=$('device').value||audioState.deviceId||'';$('device').replaceChildren(new Option(t('waveChooseInput'),''));
  for(const kind of ['input','loopback']){
    const group=document.createElement('optgroup');group.label=t(kind==='input'?'waveInputs':'waveOutputs');
    for(const device of devices.filter(d=>d.kind===kind))group.append(new Option(device.name,device.id));
    if(group.children.length)$('device').append(group);
  }
  if(devices.some(d=>d.id===previous))$('device').value=previous;
  renderStatus();
}
function renderStatus(){
  $('start').disabled=busy||!$('device').value;$('stop').disabled=busy||audioState.status==='stopped';$('refresh').disabled=busy;
  const message=deviceError||audioState.error||({stopped:'waveStopped',starting:'waveStarting',capturing:audioState.fresh?'waveCapturing':'waveWaiting',error:'audioError'}[audioState.status]||'audioError');
  $('audio-status').textContent=t(message)+(audioState.deviceName?' · '+audioState.deviceName:'');
  let sum=0;for(const sample of audioState.left||[])if(Number.isFinite(sample))sum+=sample*sample;
  $('level').value=audioState.fresh?Math.min(1,Math.sqrt(sum/Math.max(1,audioState.left?.length||0))*2):0;
}
async function refreshDevices(){
  try{const response=await fetch('/api/audio/devices',{cache:'no-store',signal:AbortSignal.timeout(3000)});if(!response.ok)throw Error();const result=await response.json();devices=Array.isArray(result.devices)?result.devices:[];deviceError=result.error||(devices.length?null:'waveNoDevices');}
  catch{deviceError='waveServerUnavailable';devices=[];}renderDevices();
}
async function source(deviceId){
  busy=true;renderStatus();
  try{const response=await fetch('/api/audio/source',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({deviceId}),signal:AbortSignal.timeout(5000)});
    const result=await response.json();if(!response.ok){deviceError=result.error==='audioDeviceLost'?'audioDeviceLost':'audioError';}else{audioState=result;deviceError=null;}}
  catch{deviceError='waveServerUnavailable';}finally{busy=false;renderStatus();}
}
$('device').addEventListener('change',renderStatus);$('start').addEventListener('click',()=>source($('device').value));$('stop').addEventListener('click',()=>source(''));$('refresh').addEventListener('click',refreshDevices);
let disposed=false,pollTimer;
async function poll(){
  try{const response=await fetch('/api/audio/state',{cache:'no-store',signal:AbortSignal.timeout(2000)});if(!response.ok)throw Error();const state=await response.json();if(!busy){audioState=state;renderStatus();}}
  catch{audioState={status:'error',error:'waveServerUnavailable'};renderStatus();}
  if(!disposed)pollTimer=setTimeout(poll,250);
}
window.addEventListener('pagehide',()=>{disposed=true;clearTimeout(pollTimer);clearTimeout(previewTimer);});
window.addEventListener('languagechange',()=>{translate();renderDevices();sync();document.title=t('waveSettings')+' · DeckStatus';});
translate();sync();await poll();await refreshDevices();
document.title=t('waveSettings')+' · DeckStatus';
