import {api} from './auth.js';
import {t,locale} from './i18n.js';
import {poll} from './poll.js';
const $=id=>document.getElementById(id);
let state=null,working=false,restarting=false;
const visible=()=>!document.querySelector('[data-pane=updater]').hidden;
function render(){
  if(!state)return;
  $('update-current').textContent=state.current;
  $('update-latest').textContent=state.latest?.version||t('updateNotChecked');
  $('update-checked').textContent=state.checkedAt?new Date(state.checkedAt).toLocaleString(locale()):'—';
  $('update-status').textContent=t(restarting?'updateRestarting':state.error||(state.status==='idle'?(state.latest?.available?'updateAvailable':state.latest?'updateUpToDate':'updateNotChecked'):'updateStatus_'+state.status));
  $('update-readonly').hidden=state.canControl;
  const busy=working||state.busy||restarting,locked=busy||!state.canControl;
  $('update-check').disabled=locked;
  $('update-download').disabled=locked||!state.latest?.available||!state.latest?.downloadable;
  $('update-upload').disabled=locked;
  $('update-file').disabled=locked;
  $('update-install').disabled=locked||!state.package||state.status!=='ready';
  $('update-cancel-upload').hidden=state.status!=='uploading'||working;
  $('update-cancel-upload').disabled=!state.canControl;
  $('update-package').hidden=!state.package;
  if(state.package){$('update-package-version').textContent=state.package.version;$('update-sha').textContent=state.package.sha256;$('update-source').textContent=t(state.package.source==='github'?'updateVerifiedGithub':'updateUploadedPackage');}
  $('update-last-result').textContent=state.lastResult?t(state.lastResult.status)+(state.lastResult.backup?' · '+t('updateBackupLocation')+' '+state.lastResult.backup:''):'';
}
async function refresh(){if(!visible()||working)return;try{state=await api('/api/admin/updater');render();}catch(error){if(state){state.canControl=false;render();}$('update-status').textContent=restarting?t('updateRestarting'):error.message;}}
async function command(action,extra={}){
  if(working)return;working=true;$('update-message').textContent='';render();
  try{await api('/api/admin/updater',{action,...extra});if(action==='install')restarting=true;}
  catch(error){$('update-message').textContent=error.message;}
  finally{working=false;await refresh();}
}
$('update-check').addEventListener('click',()=>command('check'));
$('update-download').addEventListener('click',()=>command('download'));
$('update-cancel-upload').addEventListener('click',()=>command('cancelUpload'));
$('update-install').addEventListener('click',()=>{if(state?.package&&confirm(t('updateConfirm',{version:state.package.version})))command('install',{confirm:true});});
$('update-upload').addEventListener('click',async()=>{
  const file=$('update-file').files[0];if(working||!file)return;
  if(!file.name.toLowerCase().endsWith('.zip')||file.size<=0||file.size>512*1024*1024){$('update-message').textContent=t('updateTooLarge');return;}
  working=true;render();$('update-message').textContent='';$('update-progress').hidden=false;$('update-progress').value=0;
  let upload;
  try{
    upload=await api('/api/admin/updater',{action:'beginUpload',size:file.size});
    for(let offset=0;offset<file.size;offset+=upload.chunkSize){
      const response=await fetch('/api/admin/updater/upload?id='+encodeURIComponent(upload.id)+'&offset='+offset,{method:'POST',headers:{'Content-Type':'application/octet-stream'},body:file.slice(offset,offset+upload.chunkSize),signal:AbortSignal.timeout(30000)});
      const result=await response.json();if(!response.ok)throw Error(t(result.error));
      $('update-progress').value=result.received/file.size;
    }
    await api('/api/admin/updater',{action:'finishUpload',id:upload.id});
  }catch(error){$('update-message').textContent=error.message;if(upload)try{await api('/api/admin/updater',{action:'cancelUpload'});}catch{}}
  finally{working=false;$('update-progress').hidden=true;await refresh();}
});
document.querySelector('[data-tab=updater]').addEventListener('click',refresh);
window.addEventListener('languagechange',render);
poll(refresh,{interval:2000,timeout:0});
