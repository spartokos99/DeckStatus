import {api} from './auth.js';
import {appReady} from './navigation.js';
import {t,translate} from './i18n.js';
import { poll as startPolling } from './poll.js';
const app=await appReady,$=id=>document.getElementById('admin-audio-'+id);
const pane=document.querySelector('[data-pane=audio]');
let settings={deviceId:'',deviceName:'',autoStart:false},devices=[],snapshot={},ready=false,busy=false,canControl=false,generation=0,deviceError='',message='';
const draft=()=>({deviceId:$('device').value,autoStart:$('auto').checked});
const dirty=()=>draft().deviceId!==settings.deviceId||draft().autoStart!==settings.autoStart;
function render(){
 const state=snapshot.state||{status:'stopped'},hasDevice=devices.some(d=>d.id===$('device').value);
 $('device').disabled=$('auto').disabled=!ready||!canControl||busy;
 $('save').disabled=!ready||!canControl||busy||!dirty()||($('auto').checked&&!$('device').value);
 $('start').disabled=!ready||!canControl||busy||dirty()||!hasDevice;
 $('stop').disabled=!ready||!canControl||busy||state.status==='stopped';$('refresh').disabled=busy;
 $('readonly').hidden=canControl;$('pending').hidden=!ready||!dirty();
 const key=snapshot.controlError||state.error||({stopped:'waveStopped',starting:'waveStarting',capturing:state.fresh?'waveCapturing':'waveWaiting',error:'audioError'}[state.status]||'audioError');
 $('status').textContent=t(key)+(state.deviceName?' · '+state.deviceName:'');
 $('message').textContent=message?t(message):deviceError?t(deviceError):'';
 let sum=0;for(const sample of state.left||[])if(Number.isFinite(sample))sum+=sample*sample;
 $('level').value=state.fresh?Math.min(1,Math.sqrt(sum/Math.max(1,state.left?.length||0))*2):0;
}
function renderDevices(id=$('device').value){
 $('device').replaceChildren(new Option(t('waveChooseInput'),''));
 for(const kind of ['input','loopback']){const group=document.createElement('optgroup');group.label=t(kind==='input'?'waveInputs':'waveOutputs');for(const d of devices.filter(d=>d.kind===kind))group.append(new Option(d.name,d.id));if(group.children.length)$('device').append(group);}
 if(id&&!devices.some(d=>d.id===id))$('device').add(new Option(t('audioMissingDevice',{name:settings.deviceName||id}),id));
 $('device').value=id;render();
}
function apply(value,replace=false){const changed=ready&&dirty();snapshot=value;canControl=value.canControl===true;settings=value.settings;if(replace||!changed){renderDevices(settings.deviceId);$('auto').checked=settings.autoStart;}ready=true;render();}
async function refreshDevices(){try{const data=await api('/api/audio/devices');devices=data.devices||[];deviceError=data.error||(!devices.length?'waveNoDevices':'');}catch(error){deviceError=error.message;}renderDevices();}
async function command(action){if(busy||!canControl)return;busy=true;generation++;message='';render();try{const value=await api('/api/admin/audio',action==='save'?{action,...draft()}:{action});apply(value,action==='save');message=action==='save'?'audioSettingsSaved':'';}catch(error){message=error.message;}finally{busy=false;render();}}
for(const id of ['device','auto'])$(id).addEventListener('change',()=>{message='';render();});
for(const action of ['save','start','stop'])$(action).addEventListener('click',()=>command(action));
$('refresh').addEventListener('click',refreshDevices);
// Device state is only live while the Audio tab is actually open.
async function refresh(){
 if(pane?.hidden)return;
 const current=generation;
 try{const value=await api('/api/admin/audio');if(!busy&&current===generation){apply(value);if(message==='waveServerUnavailable')message='';}}
 catch(error){canControl=false;message='waveServerUnavailable';render();throw error;}
}
window.addEventListener('languagechange',()=>{translate();renderDevices();});
render();if(app?.capabilities?.admin){await refreshDevices();await refresh().catch(()=>{});startPolling(refresh,{interval:500,timeout:0,immediate:false});}
