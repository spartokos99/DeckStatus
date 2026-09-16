import {api} from './auth.js';
import {appReady} from './navigation.js';
import {obsUrl} from './broadcast.js';
import {t,translate,getLanguage} from './i18n.js';
import {defaults as trackDefaults,presets as trackPresets,normalize as trackOptions} from './master-options.js';
import {defaults as waveDefaults,presets as wavePresets,controls as waveControls,normalize as waveOptions} from './waveform-options.js';
import {renderScene} from './scene-shared.js';
import {listPresets,componentLabel} from './component-presets.js';
const app=await appReady;
const $=id=>document.getElementById(id),stage=$('scene-stage');let scenes=[],scene=null,selected='',dirty=false,zoom=1,busy=false,drag;
let componentPresets=[];
const item=()=>scene?.items.find(row=>row.id===selected);
const id=()=>crypto.randomUUID?crypto.randomUUID():Date.now().toString(36)+Math.random().toString(36).slice(2);
const copy=value=>JSON.parse(JSON.stringify(value));
const fresh=()=>({name:t('sceneUntitled'),width:1920,height:1080,background:'transparent',items:[]});
function status(){ $('scene-save-state').textContent=t(dirty?'sceneUnsaved':'sceneSaved');for(const control of document.querySelectorAll('.scene-toolbar button,.scene-toolbar select,.scene-properties input,.scene-properties select,.scene-properties button,.scene-add button,.scene-library button,.scene-library select'))control.disabled=busy;stage.inert=busy;$('scene-save').disabled=busy||!scene;$('scene-delete').disabled=busy||!scene?.id;$('scene-add-preset').disabled=busy||!scene||scene.items.length>=32||!componentPresets.some(p=>p.id===$('scene-component').value);for(const button of document.querySelectorAll('[data-add]'))button.disabled=busy||!scene||scene.items.length>=32;}
function changed(){dirty=true;status();draw();}
function fit(){if(!scene)return;zoom=$('scene-viewport').clientWidth/scene.width;$('scene-viewport').style.aspectRatio=scene.width+'/'+scene.height;stage.style.transform='scale('+zoom+')';}
function draw(){if(!scene)return;
  // Unsaved/new layers preview with authenticated cookies; OBS uses the saved scene key.
  renderScene(stage,scene,undefined,true);fit();for(const node of stage.children)node.dataset.selected=String(node.dataset.id===selected);
  $('scene-url').value=scene.id?obsUrl('/scene?scene='+scene.id+'&key='+scene.key,app):'';$('scene-open').href=$('scene-url').value||'#';$('scene-copy').disabled=!scene.id;$('scene-rotate').disabled=!scene.id;
  $('scene-dimensions').textContent=scene.width+' × '+scene.height+' px · '+t('sceneObsSize');
}
function list(){const select=$('scene-list');select.replaceChildren(new Option(t('sceneDraft'),''));for(const entry of scenes)select.add(new Option(entry.name,entry.id));select.value=scene?.id||'';}
function fillDocument(){if(!scene)return;for(const name of ['name','width','height'])$('scene-'+name).value=scene[name];$('scene-transparent').checked=scene.background==='transparent';$('scene-background').value=scene.background==='transparent'?'#0b1215':scene.background;const size=scene.width+'x'+scene.height;$('scene-preset').value=[...$('scene-preset').options].some(o=>o.value===size)?size:'custom';list();layers();selectItem(selected);draw();status();}
function layers(){const container=$('scene-layers');container.replaceChildren();for(const entry of [...scene.items].reverse()){const row=document.createElement('div');row.className='scene-layer';const button=document.createElement('button');button.type='button';button.textContent=(entry.name?entry.name+' · ':'')+(entry.type==='deck'?t('deck',{id:entry.options.deck||1}):componentLabel(entry.type))+(entry.visible?'':' · '+t('sceneHidden'));button.setAttribute('aria-pressed',String(entry.id===selected));button.addEventListener('click',()=>selectItem(entry.id));row.append(button);container.append(row);}}
const normalizeComponent=(type,options)=>type==='waveform'?{...waveOptions(options),lang:options.lang==='de'?'de':'en'}:trackOptions(options);
function presetList(){const select=$('scene-component'),previous=select.value;select.replaceChildren(new Option(t(componentPresets.length?'presetChoose':'presetEmpty'),''));for(const type of ['deck','master','waveform']){const group=document.createElement('optgroup');group.label=componentLabel(type);for(const preset of componentPresets.filter(p=>p.type===type))group.append(new Option(preset.name,preset.id));if(group.children.length)select.append(group);}select.value=componentPresets.some(p=>p.id===previous)?previous:'';status();}
async function reloadPresets(){componentPresets=await listPresets();presetList();}
function addComponent(type,value,name){
  if(busy||!scene||scene.items.length>=32)return;
  const options=normalizeComponent(type,value),textHeight=options.fontSize*6+(options.timeline?92:0);
  const rowHeight=2*options.padding+(options.layout==='stacked'?options.coverSize+textHeight+20:Math.max(options.coverSize,textHeight));
  const height=type==='waveform'?options.height:Math.min(4320,Math.ceil(16+rowHeight+(type==='master'?options.history*(rowHeight*options.historyScale+options.gap):0)));
  const entry={id:id(),type,x:40,y:40,width:options.width+(type==='waveform'?0:16),height,opacity:1,visible:true,options};
  if(name)entry.name=name;
  scene.items.push(entry);selected=entry.id;changed();selectItem(selected);
}
$('scene-component').addEventListener('change',status);
$('scene-add-preset').addEventListener('click',()=>{const preset=componentPresets.find(p=>p.id===$('scene-component').value);if(preset)addComponent(preset.type,copy(preset.options),preset.name);});
$('scene-presets-refresh').addEventListener('click',()=>run(reloadPresets));
const geometry=[['x',-7680,7680,1],['y',-4320,4320,1],['width',32,7680,1],['height',32,4320,1],['opacity',0,1,.05]];
for(const [name,min,max,step] of geometry){const row=document.createElement('div'),label=document.createElement('label'),input=document.createElement('input');label.htmlFor='item-'+name;label.dataset.i18n='scene_'+name;input.id=label.htmlFor;input.type='number';input.min=min;input.max=max;input.step=step;input.addEventListener('input',()=>{if(input.checkValidity()&&item()){item()[name]=Number(input.value);changed();}});row.append(label,input);$('item-geometry').append(row);}
const trackControls=[['deck','number',1,4,1],['history','number',0,50,1],['width','number',320,1600,10],['historyScale','number',.2,1,.05],['align','select',['left','center','right']],['layout','select',['horizontal','stacked']],['timeline','checkbox'],['badges','checkbox'],['font','select',['system','serif','mono']],['fontSize','number',14,40,1],['coverSize','number',32,180,1],['padding','number',4,48,1],['gap','number',0,40,1],['duration','number',0,2000,50],['background','color'],['textColor','color'],['mutedColor','color'],['accent','color'],['opacity','number',0,100,1],['radius','number',0,40,1],['border','number',0,10,1],['shadow','checkbox']];
function optionControls(){const entry=item(),container=$('item-options');container.replaceChildren();if(!entry)return;
  for(const [name,type,a,b,step] of entry.type==='waveform'?waveControls:trackControls){if((name==='deck'&&entry.type!=='deck')||(['history','historyScale'].includes(name)&&entry.type!=='master'))continue;
    const label=document.createElement('label'),input=document.createElement(type==='select'?'select':'input');label.htmlFor='option-'+name;label.textContent=t((entry.type==='waveform'?'wave_':'sceneOption_')+name);input.id=label.htmlFor;
    if(type==='select')for(const value of a)input.add(new Option(t('sceneValue_'+value)===('sceneValue_'+value)?value:t('sceneValue_'+value),value));else{input.type=type;if(type==='range'||type==='number'){input.min=a;input.max=b;input.step=step;}}
    if(type==='checkbox')input.checked=entry.options[name];else input.value=entry.options[name];
    input.addEventListener('input',()=>{if(!input.checkValidity())return;entry.options=normalizeComponent(entry.type,{...entry.options,[name]:type==='checkbox'?input.checked:input.value});changed();if(name==='deck')layers();});container.append(label,input);
  }
  if(entry.type!=='waveform')for(const name of ['title','artist','album','bpm','key','cover']){const label=document.createElement('label'),input=document.createElement('input');input.type='checkbox';input.checked=entry.options.fields.includes(name);input.addEventListener('change',()=>{const values=new Set(entry.options.fields);input.checked?values.add(name):values.delete(name);if(!values.size){input.checked=true;return;}entry.options.fields=[...values];changed();});label.append(input,document.createTextNode(' '+t(name)));container.append(label);}
}
function selectItem(value){selected=value;const entry=item();$('item-fields').hidden=!entry;if(entry){for(const [name] of geometry)$('item-'+name).value=entry[name];$('item-visible').checked=entry.visible;const presets=entry.type==='waveform'?wavePresets:trackPresets;$('item-preset').replaceChildren(new Option(t('custom'),''));for(const name of Object.keys(presets))$('item-preset').add(new Option(name,name));optionControls();}layers();draw();}
for(const name of ['name','width','height'])$('scene-'+name).addEventListener('input',()=>{const input=$('scene-'+name);if(!input.checkValidity())return;scene[name]=name==='name'?input.value:Number(input.value);changed();});
$('scene-preset').addEventListener('change',()=>{if($('scene-preset').value==='custom')return;[scene.width,scene.height]=$('scene-preset').value.split('x').map(Number);changed();fillDocument();});
for(const name of ['transparent','background'])$('scene-'+name).addEventListener('input',()=>{scene.background=$('scene-transparent').checked?'transparent':$('scene-background').value;changed();});
$('item-visible').addEventListener('change',()=>{if(item()){item().visible=$('item-visible').checked;changed();layers();}});
$('item-preset').addEventListener('change',()=>{const entry=item();if(!entry)return;const presets=entry.type==='waveform'?wavePresets:trackPresets,preset=presets[$('item-preset').value];if(preset){entry.options=normalizeComponent(entry.type,{...entry.options,...preset});changed();optionControls();}});
document.querySelectorAll('[data-add]').forEach(button=>button.addEventListener('click',()=>{const type=button.dataset.add;addComponent(type,{...(type==='waveform'?waveDefaults:trackDefaults),...(type==='master'?{history:3}:{}),lang:getLanguage()});}));
for(const [name,offset] of [['up',1],['down',-1]])$('item-'+name).addEventListener('click',()=>{const index=scene.items.findIndex(row=>row.id===selected),next=index+offset;if(index<0||next<0||next>=scene.items.length)return;[scene.items[index],scene.items[next]]=[scene.items[next],scene.items[index]];changed();layers();});
$('item-delete').addEventListener('click',()=>{scene.items=scene.items.filter(row=>row.id!==selected);selected='';changed();selectItem('');});
$('item-duplicate').addEventListener('click',()=>{if(!item()||scene.items.length>=32)return;const entry={...copy(item()),id:id(),x:item().x+20,y:item().y+20};scene.items.push(entry);changed();selectItem(entry.id);});
const discard=()=>!dirty||confirm(t('sceneDiscard'));
async function run(action){if(busy)return;busy=true;status();try{$('scene-message').textContent='';await action();}catch(error){$('scene-message').textContent=error.message;}finally{busy=false;status();}}
async function reload(){scenes=(await api('/api/scenes')).scenes;scene=copy(scenes.find(s=>s.id===scene?.id)||scenes[0]||fresh());selected='';dirty=!scene.id;fillDocument();}
$('scene-list').addEventListener('change',()=>{if(!discard()){list();return;}scene=copy(scenes.find(s=>s.id===$('scene-list').value)||fresh());selected='';dirty=!scene.id;fillDocument();});
$('scene-new').addEventListener('click',()=>{if(!discard())return;scene=fresh();selected='';dirty=true;fillDocument();});
$('scene-duplicate').addEventListener('click',()=>{scene={...copy(scene),name:scene.name+' · '+t('sceneCopyName')};delete scene.id;delete scene.key;delete scene.revision;dirty=true;fillDocument();});
$('scene-save').addEventListener('click',()=>run(async()=>{scene=await api('/api/scenes',{action:'save',id:scene.id||'',revision:scene.revision||0,scene});scenes=scenes.filter(row=>row.id!==scene.id);scenes.push(copy(scene));dirty=false;fillDocument();}));
$('scene-delete').addEventListener('click',()=>{if(!scene.id||!confirm(t('sceneDeleteConfirm')))return;run(async()=>{await api('/api/scenes',{action:'delete',id:scene.id,revision:scene.revision});scene=null;await reload();});});
$('scene-reload').addEventListener('click',()=>{if(discard())run(reload);});
$('scene-rotate').addEventListener('click',()=>{if(!scene.id||!confirm(t('sceneRotateConfirm')))return;run(async()=>{if(dirty)throw Error(t('sceneSaveFirst'));scene=await api('/api/scenes',{action:'rotate',id:scene.id,revision:scene.revision});scenes=scenes.filter(s=>s.id!==scene.id);scenes.push(copy(scene));draw();});});
$('scene-copy').addEventListener('click',async()=>{try{await navigator.clipboard.writeText($('scene-url').value);$('scene-message').textContent=t('networkCopied');}catch{$('scene-url').select();$('scene-message').textContent=t('copyFallback');}});
stage.addEventListener('pointerdown',event=>{const node=event.target.closest('.scene-item');if(!node)return;selectItem(node.dataset.id);const entry=item();drag={id:entry.id,x:event.clientX,y:event.clientY,start:copy(entry),resize:event.target.classList.contains('scene-handle')};stage.setPointerCapture(event.pointerId);event.preventDefault();});
stage.addEventListener('pointermove',event=>{if(!drag)return;const entry=item();if(!entry||entry.id!==drag.id)return;const dx=(event.clientX-drag.x)/zoom,dy=(event.clientY-drag.y)/zoom,snap=$('scene-snap').checked?10:1,round=n=>Math.round(n/snap)*snap;if(drag.resize){entry.width=Math.max(32,Math.min(7680,round(drag.start.width+dx)));entry.height=Math.max(32,Math.min(4320,round(drag.start.height+dy)));}else{entry.x=Math.max(-7680,Math.min(7680,round(drag.start.x+dx)));entry.y=Math.max(-4320,Math.min(4320,round(drag.start.y+dy)));}for(const [name] of geometry)$('item-'+name).value=entry[name];changed();});
for(const event of ['pointerup','pointercancel','lostpointercapture'])stage.addEventListener(event,()=>drag=null);
stage.addEventListener('keydown',event=>{const entry=item();if(!entry||!['ArrowLeft','ArrowRight','ArrowUp','ArrowDown'].includes(event.key))return;event.preventDefault();const step=event.shiftKey?10:1;if(event.key==='ArrowLeft')entry.x-=step;if(event.key==='ArrowRight')entry.x+=step;if(event.key==='ArrowUp')entry.y-=step;if(event.key==='ArrowDown')entry.y+=step;changed();selectItem(selected);});
new ResizeObserver(fit).observe($('scene-viewport'));
window.addEventListener('beforeunload',event=>{if(dirty){event.preventDefault();event.returnValue='';}});
window.addEventListener('languagechange',()=>{translate();fillDocument();presetList();});
translate();await run(async()=>{await reload();await reloadPresets();});
