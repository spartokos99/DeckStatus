import {api} from './auth.js';
import {appReady} from './navigation.js';
import {t,translate} from './i18n.js';
const app=await appReady,$=id=>document.getElementById('admin-master-'+id);
const defaults={detectionBeats:128,interruptBeats:16,onlyAfterEnd:false,useOnAir:true};
let settings={...defaults},ready=false,busy=false,canControl=false,onAirAvailable=false,message='';
const values=()=>({detectionBeats:Number($('beats').value),interruptBeats:Number($('interrupt').value),
  onlyAfterEnd:$('end').checked,useOnAir:onAirAvailable?$('onair').checked:settings.useOnAir});
const dirty=()=>ready&&Object.entries(values()).some(([key,value])=>value!==settings[key]);
function fill(value){$('beats').value=value.detectionBeats;$('interrupt').value=value.interruptBeats;$('end').checked=value.onlyAfterEnd;$('onair').checked=onAirAvailable&&value.useOnAir;}
function valid(){const v=values();return $('beats').value!==''&&$('interrupt').value!==''&&Number.isInteger(v.detectionBeats)&&v.detectionBeats>=1&&v.detectionBeats<=1024&&
  Number.isInteger(v.interruptBeats)&&v.interruptBeats>=0&&v.interruptBeats<=1024;}
function render(){
 const disabled=!ready||!canControl||busy;
 $('beats').disabled=disabled||$('end').checked;
 $('interrupt').disabled=$('end').disabled=disabled;
 $('onair').disabled=disabled||!onAirAvailable;
 $('rekordbox').hidden=onAirAvailable;
 $('save').disabled=disabled||!dirty()||!valid();
 $('reset').disabled=disabled||Object.entries(values()).every(([key,value])=>key==='useOnAir'&&!onAirAvailable||value===defaults[key]);
 $('readonly').hidden=!ready||canControl;$('pending').hidden=!dirty();
 $('message').textContent=message?t(message):'';
}
function apply(value){
 settings={...defaults,...value};onAirAvailable=value.onAirAvailable===true;
 canControl=value.canControl===true&&value.available!==false;
 fill(settings);ready=true;render();
}
for(const id of ['beats','interrupt','end','onair'])$(id).addEventListener('input',()=>{message='';render();});
$('save').addEventListener('click',async()=>{
 if(busy||!canControl||!valid())return;
 const draft=values();busy=true;message='';render();
 try{apply(await api('/api/admin/master',draft));message='masterDetectionSaved';}
 catch(error){message=error.message;}
 finally{busy=false;render();}
});
$('reset').addEventListener('click',()=>{fill(defaults);message='';render();});
window.addEventListener('languagechange',()=>{translate();render();});
render();
if(app?.capabilities?.admin){
 try{apply(await api('/api/admin/master'));}
 catch(error){message=error.message;render();}
}
