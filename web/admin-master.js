import {api} from './auth.js';
import {appReady} from './navigation.js';
import {t,translate,locale} from './i18n.js';
const app=await appReady,$=id=>document.getElementById('admin-master-'+id);
let settings={holdMs:4000,defaultHoldMs:4000,maxHoldMs:30000},ready=false,busy=false,canControl=false,message='';
const draft=()=>Math.round(Number($('hold').value)*1000);
const dirty=()=>ready&&draft()!==settings.holdMs;
const label=ms=>ms?new Intl.NumberFormat(locale(),{maximumFractionDigits:1}).format(ms/1000)+' s':t('masterHoldInstant');
function render(){
 $('hold').disabled=!ready||!canControl||busy;
 $('save').disabled=!ready||!canControl||busy||!dirty();
 $('reset').disabled=!ready||!canControl||busy||draft()===settings.defaultHoldMs;
 $('readonly').hidden=!ready||canControl;$('pending').hidden=!dirty();
 $('value').textContent=ready?label(draft()):'—';
 $('message').textContent=message?t(message):'';
}
// The saved value is the server's; a poll must not overwrite an unsaved draft.
function apply(value,replace=false){
 const changed=dirty();
 settings={holdMs:value.holdMs,defaultHoldMs:value.defaultHoldMs??4000,maxHoldMs:value.maxHoldMs??30000};
 canControl=value.canControl===true&&value.available!==false;
 $('hold').max=String(settings.maxHoldMs/1000);
 if(replace||!changed)$('hold').value=String(settings.holdMs/1000);
 ready=true;render();
}
async function load(){try{apply(await api('/api/admin/master'));if(message==='masterHoldUnavailable')message='';}catch(error){canControl=false;message=error.message;render();}}
$('hold').addEventListener('input',()=>{message='';render();});
$('save').addEventListener('click',async()=>{
 if(busy||!canControl)return;busy=true;message='';render();
 try{apply(await api('/api/admin/master',{holdMs:draft()}),true);message='masterHoldSaved';}
 catch(error){message=error.message;}
 finally{busy=false;render();}
});
$('reset').addEventListener('click',()=>{$('hold').value=String(settings.defaultHoldMs/1000);message='';render();});
window.addEventListener('languagechange',()=>{translate();render();});
render();if(app?.capabilities?.admin)await load();
