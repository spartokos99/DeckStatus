import {t,translate,getLanguage,locale} from './i18n.js';
import {readSetting,writeSetting} from './storage.js';
import {appReady} from './navigation.js';
import {broadcastUrl,loadBroadcastKeys,obsUrl} from './broadcast.js';
import {setupPresets} from './component-presets.js';
const broadcastKeys=await loadBroadcastKeys();
const app=await appReady;
import {controls,defaults,presets,normalize,overlayUrl} from './waveform-options.js';
const $=id=>document.getElementById(id), key='deckstatus.waveform.options';
let saved={};try{saved=JSON.parse(readSetting(key)||'{}');}catch{}
let options=normalize(saved), previewTimer;
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
  const path=broadcastUrl(overlayUrl(options,getLanguage()),broadcastKeys.waveform);$('url').value=obsUrl(path,app);$('open').href=$('url').value;
  $('dimensions').textContent=options.width+' × '+options.height+' px';
  $('preview').width=options.width;$('preview').height=options.height;
  clearTimeout(previewTimer);previewTimer=setTimeout(()=>{$('preview').src=path;},120);
}
$('preset').addEventListener('change',()=>{if(Object.hasOwn(presets,$('preset').value)){options=normalize(presets[$('preset').value]);sync();}});
$('reset').addEventListener('click',()=>{options={...defaults};$('preset').value='mint';sync();});
$('copy').addEventListener('click',async()=>{try{await navigator.clipboard.writeText($('url').value);$('feedback').textContent=t('waveCopied');}catch{$('url').select();$('feedback').textContent=t('waveCopyManual');}});
window.addEventListener('pagehide',()=>clearTimeout(previewTimer));
window.addEventListener('languagechange',()=>{translate();sync();document.title=t('waveSettings')+' · DeckStatus';});
$('audio-admin-link').hidden=!app?.capabilities?.admin;
translate();sync();
document.title=t('waveSettings')+' · DeckStatus';
setupPresets({type:'waveform',read:()=>({...options,lang:getLanguage()}),apply:value=>{options=normalize(value);$('preset').value='custom';sync();}});
