import {api} from './auth.js';
import {appReady} from './navigation.js';
import {t,translate,locale} from './i18n.js';
await appReady;
const $=id=>document.getElementById(id),pane=document.querySelector('[data-pane=automations]'),endpoint='/api/admin/automations';
let state,config,scenes=[],selected='',actionIndex=0,draft=false,busy=false,disposed=false;
const clone=value=>JSON.parse(JSON.stringify(value));
const types=['show','hide','toggle','audioOn','audioOff','text','opacity','x','y','rotation','chat'];
const triggers=['reward','command','contains','exact','raid','online','offline'];
const label=(prefix,value)=>t('twitch'+prefix+value[0].toUpperCase()+value.slice(1));
const freshAction=()=>({action:'show',target:{scene:'',item:''},value:null,duration:60});
const fresh=()=>({id:crypto.randomUUID?.()||Date.now().toString(36)+Math.random().toString(36).slice(2),name:t('twitchNewRule'),enabled:true,trigger:'reward',match:'',role:'everyone',cooldown:5,minimum:0,actions:[freshAction()]});
const rule=()=>config?.rules.find(row=>row.id===selected),action=()=>rule()?.actions[actionIndex];
function controls(){const allowed=state?.canControl&&!busy;pane.querySelectorAll('input,select,textarea,button').forEach(node=>node.disabled=!allowed||node.dataset.unavailable==='true');$('twitch-readonly').hidden=!!state?.canControl;$('twitch-dirty').hidden=!draft;$('twitch-delete-rule').disabled=!allowed||!rule();$('twitch-new-rule').disabled=!allowed||config?.rules.length>=64;$('automation-add-action').disabled=!allowed||!rule()||rule().actions.length>=16;}
function changed(){draft=true;controls();}
function renderRules(){const list=$('twitch-rules');list.replaceChildren();for(const r of config.rules){
  const row=document.createElement('div');row.className='twitch-rule';row.dataset.selected=String(r.id===selected);row.dataset.disabled=String(!r.enabled);
  const select=document.createElement('button'),name=document.createElement('strong'),detail=document.createElement('small');name.textContent=r.name;detail.textContent=label('Trigger',r.trigger)+' · '+t('automationActionCount',{count:r.actions.length});select.append(name,detail);select.addEventListener('click',()=>selectRule(r.id));
  const toggle=document.createElement('button');toggle.textContent=r.enabled?'◉':'○';toggle.setAttribute('aria-label',t(r.enabled?'twitchDisableRule':'twitchEnableRule')+' · '+r.name);toggle.setAttribute('aria-pressed',String(r.enabled));toggle.addEventListener('click',()=>{r.enabled=!r.enabled;changed();renderRules();});row.append(select,toggle);list.append(row);
}if(!config.rules.length){const p=document.createElement('p');p.textContent=t('twitchNoRules');list.append(p);}controls();}
function renderActions(){const list=$('automation-actions');list.replaceChildren();const r=rule();if(!r)return;
  r.actions.forEach((a,index)=>{
    const row=document.createElement('div');row.className='twitch-rule';row.dataset.selected=String(index===actionIndex);row.dataset.actionIndex=index;
    const select=document.createElement('button'),title=document.createElement('strong'),detail=document.createElement('small');title.textContent=(index+1)+'. '+label('Action',a.action);
    const scene=scenes.find(s=>s.id===a.target.scene),item=scene?.items.find(i=>i.id===a.target.item);
    detail.textContent=a.action==='chat'?a.value:(scene?.name||t('twitchChooseScene'))+' / '+(item?.name||item?.type||t('twitchChooseLayer'))+' · '+a.duration+' s';select.append(title,detail);select.addEventListener('click',()=>selectAction(index));row.append(select);
    const button=(symbol,key,disabled,fn)=>{const b=document.createElement('button');b.textContent=symbol;b.title=t(key);b.setAttribute('aria-label',t(key)+' '+(index+1));b.dataset.unavailable=String(disabled);b.addEventListener('click',()=>{if(busy||disabled)return;fn();changed();selectAction(actionIndex);renderRules();});row.append(b);};
    button('↑','automationMoveUp',index===0,()=>{[r.actions[index-1],r.actions[index]]=[r.actions[index],r.actions[index-1]];actionIndex=index-1;});
    button('↓','automationMoveDown',index===r.actions.length-1,()=>{[r.actions[index+1],r.actions[index]]=[r.actions[index],r.actions[index+1]];actionIndex=index+1;});
    button('＋','automationDuplicateAction',r.actions.length>=16,()=>{r.actions.splice(index+1,0,clone(a));actionIndex=index+1;});
    button('×','automationRemoveAction',r.actions.length===1,()=>{r.actions.splice(index,1);actionIndex=Math.min(index,r.actions.length-1);});list.append(row);
  });controls();
}
function targetItems(){const a=action();if(!a)return;const scene=scenes.find(s=>s.id===$('twitch-scene').value);$('twitch-item').replaceChildren(new Option(t('twitchChooseLayer'),''));for(const item of scene?.items||[])if(a.action!=='text'||item.type==='text')$('twitch-item').add(new Option(item.name||item.type,item.id));$('twitch-item').value=a.target.item;}
function formVisibility(){const r=rule(),a=action();if(!r||!a)return;
  $('twitch-reward-picker').hidden=r.trigger!=='reward';$('twitch-match-row').hidden=['raid','online','offline'].includes(r.trigger);$('twitch-role-row').hidden=!['command','contains','exact'].includes(r.trigger);$('twitch-minimum-row').hidden=r.trigger!=='raid';
  $('twitch-target-row').hidden=a.action==='chat';$('twitch-duration-row').hidden=a.action==='chat';$('twitch-value-row').hidden=['show','hide','toggle','audioOn','audioOff'].includes(a.action);$('twitch-value').hidden=!['text','chat'].includes(a.action);$('twitch-number').hidden=['text','chat'].includes(a.action);$('twitch-value').maxLength=a.action==='chat'?450:2000;
  const bounds={opacity:[0,1,.05],x:[-7680,7680,1],y:[-4320,4320,1],rotation:[-360,360,1]}[a.action]||[0,1,1];[$('twitch-number').min,$('twitch-number').max,$('twitch-number').step]=bounds;
  $('twitch-match-help').textContent=t(r.trigger==='reward'?'twitchRewardHelp':r.trigger==='command'?'twitchCommandHelp':'twitchMatchHelp');
}
function selectAction(index){actionIndex=index;const a=action();$('automation-action-fields').hidden=!a;if(a){$('twitch-action').value=a.action;$('twitch-duration').value=a.duration;$('twitch-value').value=typeof a.value==='string'?a.value:'';$('twitch-number').value=typeof a.value==='number'?a.value:0;$('twitch-scene').replaceChildren(new Option(t('twitchChooseScene'),''));for(const s of scenes)$('twitch-scene').add(new Option(s.name,s.id));$('twitch-scene').value=a.target.scene;targetItems();formVisibility();}renderActions();}
function selectRule(id){selected=id;const r=rule();$('twitch-rule-form').hidden=!r;if(r){for(const name of ['name','trigger','match','role','cooldown','minimum'])$('twitch-'+name).value=r[name];selectAction(0);}renderRules();}
function renderStatus(){if(!state)return;$('twitch-status').textContent=t(state.connection);$('twitch-error').textContent=t(state.error||'');
  const select=$('twitch-reward-list'),previous=select.value;select.replaceChildren(new Option(t('twitchRewardManual'),''));for(const reward of state.rewards||[])select.add(new Option(reward.title+' · '+reward.cost,reward.id));select.value=previous;
  $('twitch-subscriptions').textContent=Object.entries(state.subscriptions||{}).map(([name,status])=>name+': '+t(status)).join('\n');$('twitch-log').textContent=[...(state.log||[])].reverse().map(row=>new Date(row.time*1000).toLocaleTimeString(locale())+' · '+row.rule+' · '+t(row.result)).join('\n');controls();
}
async function run(fn){if(busy)return;busy=true;controls();try{$('twitch-message').textContent='';await fn();}catch(error){$('twitch-message').textContent=error.message;}finally{busy=false;controls();}}
async function load(reload=false){state=await api(endpoint);if(!config||reload){config=clone(state.settings);$('twitch-enabled').checked=config.enabled;draft=false;selectRule(config.rules.some(r=>r.id===selected)?selected:config.rules[0]?.id||'');}renderStatus();}
$('twitch-enabled').addEventListener('input',()=>{config.enabled=$('twitch-enabled').checked;changed();});
for(const field of ['name','trigger','match','role','cooldown','minimum'])$('twitch-'+field).addEventListener('input',()=>{const r=rule(),node=$('twitch-'+field);if(!r||!node.checkValidity())return;r[field]=['cooldown','minimum'].includes(field)?Number(node.value):node.value;changed();formVisibility();renderRules();});
$('twitch-action').addEventListener('input',()=>{const a=action();if(!a)return;a.action=$('twitch-action').value;a.value=['text','chat'].includes(a.action)?'{user}':a.action==='opacity'?1:['x','y','rotation'].includes(a.action)?0:null;changed();selectAction(actionIndex);});
for(const field of ['value','number','duration'])$('twitch-'+field).addEventListener('input',()=>{const a=action(),node=$('twitch-'+field);if(!a||!node.checkValidity())return;a[field==='duration'?'duration':'value']=field==='value'?node.value:Number(node.value);changed();renderActions();});
$('twitch-scene').addEventListener('change',()=>{action().target={scene:$('twitch-scene').value,item:''};targetItems();changed();renderActions();});$('twitch-item').addEventListener('change',()=>{action().target.item=$('twitch-item').value;changed();renderActions();});
$('automation-add-action').addEventListener('click',()=>{if(!rule()||rule().actions.length>=16)return;rule().actions.push(freshAction());changed();selectAction(rule().actions.length-1);renderRules();});
$('twitch-rewards').addEventListener('click',()=>run(async()=>{await api(endpoint,{action:'rewards'});$('twitch-message').textContent=t('twitchQueued');}));
$('twitch-reward-list').addEventListener('change',()=>{if(rule()){$('twitch-match').value=$('twitch-reward-list').value;rule().match=$('twitch-match').value;changed();}});
$('twitch-new-rule').addEventListener('click',()=>{const r=fresh();config.rules.push(r);changed();selectRule(r.id);});
$('twitch-delete-rule').addEventListener('click',()=>{config.rules=config.rules.filter(r=>r.id!==selected);changed();selectRule(config.rules[0]?.id||'');});
$('twitch-save').addEventListener('click',()=>run(async()=>{await api(endpoint,{action:'save',settings:config});await load(true);$('twitch-message').textContent=t('portalSaved');}));
$('twitch-reload').addEventListener('click',()=>{if(!draft||confirm(t('sceneDiscard')))run(async()=>{scenes=(await api('/api/scenes')).scenes;await load(true);});});
$('twitch-reset').addEventListener('click',()=>run(async()=>{await api(endpoint,{action:'reset'});await load();$('twitch-message').textContent=t('twitchResetDone');}));
$('twitch-test').addEventListener('click',()=>run(async()=>{if(draft)throw Error(t('twitchSaveFirst'));const result=await api(endpoint,{action:'test',event:{type:$('twitch-test-type').value,user:'TestViewer',message:$('twitch-test-message').value,reward:$('twitch-test-reward').value,viewers:10,role:$('twitch-test-role').value}});$('twitch-test-results').textContent=result.results.length?result.results.map(row=>row.rule+' · '+(row.index||1)+'. '+label('Action',row.action)+'\n'+(row.error?t(row.error):row.message||JSON.stringify(row.value))).join('\n\n'):t('twitchNoMatches');}));
function localize(){translate();document.title=t('navAutomations')+' · DeckStatus';for(const [name,values,prefix] of [['trigger',triggers,'Trigger'],['action',types,'Action'],['role',['everyone','moderator','broadcaster'],'Role'],['test-role',['everyone','moderator','broadcaster'],'Role']]){const node=$('twitch-'+name),value=node.value;node.replaceChildren(...values.map(v=>new Option(label(prefix,v),v)));node.value=value||values[0];}if(config){const index=actionIndex;selectRule(selected);if(rule())selectAction(index);}renderStatus();}
window.addEventListener('languagechange',localize);window.addEventListener('beforeunload',event=>{if(draft){event.preventDefault();event.returnValue='';}});window.addEventListener('pagehide',()=>disposed=true);
localize();await run(async()=>{scenes=(await api('/api/scenes')).scenes;await load();});
async function poll(){if(disposed)return;if(!busy)try{await load();}catch(error){$('twitch-message').textContent=error.message;}if(!disposed)setTimeout(poll,2000);}setTimeout(poll,2000);
