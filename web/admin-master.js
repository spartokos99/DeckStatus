import {api} from './auth.js';
import {appReady} from './navigation.js';
import {t,translate,locale} from './i18n.js';
const app=await appReady,$=id=>document.getElementById('admin-master-'+id);
const defaults={holdMs:4000,prolinkMethod:'tempo',detectionBeats:128,interruptBeats:16,useOnAir:true};
let settings={...defaults,defaultHoldMs:4000,maxHoldMs:30000},ready=false,busy=false,canControl=false,message='';
const prolink=app?.mode==='prolink';
const draft=()=>Math.round(Number($('hold').value)*1000);
const values=()=>({holdMs:draft(),prolinkMethod:$('method').value,detectionBeats:Number($('beats').value),interruptBeats:Number($('interrupt').value),useOnAir:$('onair').checked});
const dirty=()=>ready&&Object.entries(values()).some(([k,v])=>v!==settings[k]);
function fill(value){$('hold').value=String(value.holdMs/1000);$('method').value=value.prolinkMethod;$('beats').value=value.detectionBeats;$('interrupt').value=value.interruptBeats;$('onair').checked=value.useOnAir;}
const label=ms=>ms?new Intl.NumberFormat(locale(),{maximumFractionDigits:1}).format(ms/1000)+' s':t('masterHoldInstant');
function render(){
 $('hold').disabled=!ready||!canControl||busy;
 for(const id of ['method','beats','interrupt','onair'])$(id).disabled=!ready||!canControl||busy||!prolink;
 $('prolink').hidden=!prolink;$('smart').hidden=$('method').value!=='smart';$('tempo').hidden=prolink&&$('method').value==='smart';
 $('save').disabled=!ready||!canControl||busy||!dirty()||!$('beats').checkValidity()||!$('interrupt').checkValidity();
 $('reset').disabled=!ready||!canControl||busy||(prolink?Object.entries(values()).every(([k,v])=>v===defaults[k]):draft()===settings.defaultHoldMs);
 $('readonly').hidden=!ready||canControl;$('pending').hidden=!dirty();
 $('value').textContent=ready?label(draft()):'—';
 $('message').textContent=message?t(message):'';
}
// The saved value is the server's; a poll must not overwrite an unsaved draft.
function apply(value,replace=false){
 const changed=dirty();
 settings={...defaults,...value,defaultHoldMs:value.defaultHoldMs??4000,maxHoldMs:value.maxHoldMs??30000};
 canControl=value.canControl===true&&value.available!==false;
 $('hold').max=String(settings.maxHoldMs/1000);
 if(replace||!changed)fill(settings);
 ready=true;render();
}
async function load(){try{apply(await api('/api/admin/master'));if(message==='masterHoldUnavailable')message='';}catch(error){canControl=false;message=error.message;render();}}
$('hold').addEventListener('input',()=>{message='';render();});
for(const id of ['method','beats','interrupt','onair'])$(id).addEventListener('input',()=>{message='';render();});
$('save').addEventListener('click',async()=>{
 if(busy||!canControl)return;busy=true;message='';render();
 try{apply(await api('/api/admin/master',prolink?values():{holdMs:draft()}),true);message='masterHoldSaved';}
 catch(error){message=error.message;}
 finally{busy=false;render();}
});
$('reset').addEventListener('click',()=>{fill(prolink?defaults:{...settings,holdMs:settings.defaultHoldMs});message='';render();});
window.addEventListener('languagechange',()=>{translate();render();});
render();if(app?.capabilities?.admin)await load();
