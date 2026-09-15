import {t,translate} from './i18n.js';
export async function api(path,body){
  const response=await fetch(path,{cache:'no-store',signal:AbortSignal.timeout(10000),...(body===undefined?{}:{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})});
  const result=await response.json();
  if(!response.ok){if(response.status===401&&!['/login','/history'].includes(location.pathname))location.assign('/login');if(result.error==='authPasswordRequired')location.assign('/account/password');throw Error(t(result.error||'portalUnavailable'));}
  return result;
}
const message=document.getElementById('auth-message');
for(const [id,action] of [['login-form',async()=>{const r=await api('/api/auth/login',{username:document.getElementById('username').value,password:document.getElementById('password').value});location.assign(r.user.mustChangePassword?'/account/password':'/');}],['password-form',async()=>{const password=document.getElementById('new-password').value;if(password!==document.getElementById('confirm-password').value)throw Error(t('authPasswordMismatch'));await api('/api/auth/password',{currentPassword:document.getElementById('current-password').value,password});location.assign('/login?changed=1');}]]){
  const form=document.getElementById(id);form?.addEventListener('submit',async event=>{event.preventDefault();const button=form.querySelector('button');button.disabled=true;message.textContent='';try{await action();}catch(error){message.textContent=error.message;}finally{button.disabled=false;}});
}
document.getElementById('auth-logout')?.addEventListener('click',async()=>{await api('/api/auth/logout',{});location.assign('/login');});
if(message&&new URLSearchParams(location.search).has('changed'))message.textContent=t('authPasswordChanged');
translate();
function title(){if(document.getElementById('login-form')||document.getElementById('password-form'))document.title=t(document.getElementById('login-form')?'authSignIn':'authChangePassword')+' · DeckStatus';}title();window.addEventListener('languagechange',title);
