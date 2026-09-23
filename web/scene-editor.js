import {api} from './auth.js';
import {appReady} from './navigation.js';
import {obsUrl} from './broadcast.js';
import {t,translate,getLanguage} from './i18n.js';
import {defaults as trackDefaults,presets as trackPresets,normalize as trackOptions,fieldNames,fonts} from './master-options.js';
import {trackExtras} from './track-controls.js';
import {poll} from './poll.js';
import {defaults as waveDefaults,presets as wavePresets,controls as waveControls,normalize as waveOptions} from './waveform-options.js';
import {renderScene} from './scene-shared.js';
import {listPresets,componentLabel} from './component-presets.js';
import {creativeTypes,defaults as creativeDefaults,normalize as creativeOptions,presets as creativePresets,controls as creativeControls,reactionOptions,reactionControls,buildControls} from './creative-options.js';
import {listMedia} from './media-library.js';
const app=await appReady;
const $=id=>document.getElementById(id),stage=$('scene-stage');let scenes=[],scene=null,selected='',dirty=false,zoom=1,busy=false,drag;
let componentPresets=[],media=[];
const item=()=>scene?.items.find(row=>row.id===selected);
const id=()=>crypto.randomUUID?crypto.randomUUID():Date.now().toString(36)+Math.random().toString(36).slice(2);
const copy=value=>JSON.parse(JSON.stringify(value));
const fresh=()=>({name:t('sceneUntitled'),width:1920,height:1080,background:'transparent',items:[]});
function status(){ $('scene-save-state').textContent=t(dirty?'sceneUnsaved':'sceneSaved');for(const control of document.querySelectorAll('.scene-toolbar button,.scene-toolbar select,.scene-properties input,.scene-properties textarea,.scene-properties select,.scene-properties button,.scene-add button,.scene-library button,.scene-library select'))control.disabled=busy||control.dataset.layerBoundary==='true'||control.dataset.optionDisabled==='true';stage.inert=busy;$('scene-save').disabled=busy||!scene;$('scene-delete').disabled=busy||!scene?.id;$('scene-add-preset').disabled=busy||!scene||scene.items.length>=32||!componentPresets.some(p=>p.id===$('scene-component').value);for(const button of document.querySelectorAll('[data-add]'))button.disabled=busy||!scene||scene.items.length>=32;}
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
function layers(){
  const container=$('scene-layers');container.replaceChildren();
  if(!scene.items.length){const empty=document.createElement('p');empty.className='scene-layers-empty';empty.textContent=t('sceneLayersEmpty');container.append(empty);return;}
  for(const [index,entry] of [...scene.items].reverse().entries()){
    const row=document.createElement('div');row.className='scene-layer';row.dataset.id=entry.id;row.dataset.hidden=String(!entry.visible);row.dataset.selected=String(entry.id===selected);
    const select=document.createElement('button');select.className='scene-layer-select';select.type='button';select.setAttribute('aria-pressed',String(entry.id===selected));select.addEventListener('click',()=>{if(!busy)selectItem(entry.id);});
    const icon=document.createElement('span');icon.className='scene-layer-icon';icon.setAttribute('aria-hidden','true');icon.textContent={deck:'▣',master:'◈',waveform:'≋',text:'T',image:'▧',fx:'✦'}[entry.type];
    const label=document.createElement('span'),title=document.createElement('strong'),detail=document.createElement('small');label.className='scene-layer-title';const kind=entry.type==='deck'?t('deck',{id:entry.options.deck||1}):componentLabel(entry.type);title.textContent=entry.name||kind;detail.textContent=kind+' · '+(entry.visible?t('sceneVisible'):t('sceneHidden'));label.append(title,detail);select.append(icon,label);
    const actions=document.createElement('div');actions.className='scene-layer-actions';
    const action=(symbol,key,fn,disabled=false)=>{const button=document.createElement('button');button.type='button';button.textContent=symbol;button.title=t(key);button.setAttribute('aria-label',t(key)+' · '+(entry.name||kind));button.dataset.layerBoundary=String(disabled);button.disabled=busy||disabled;button.addEventListener('click',()=>{if(!busy)fn();});actions.append(button);return button;};
    const visibility=action(entry.visible?'◉':'○',entry.visible?'sceneHideLayer':'sceneShowLayer',()=>{entry.visible=!entry.visible;changed();selectItem(selected);});visibility.dataset.visibility=entry.id;visibility.setAttribute('aria-pressed',String(entry.visible));
    for(const [symbol,key,offset,disabled] of [['↑','sceneForward',1,index===0],['↓','sceneBackward',-1,index===scene.items.length-1]])action(symbol,key,()=>{const i=scene.items.indexOf(entry);[scene.items[i],scene.items[i+offset]]=[scene.items[i+offset],scene.items[i]];changed();layers();},disabled);
    row.append(select,actions);container.append(row);
  }
}
const normalizeComponent=(type,options)=>creativeTypes.includes(type)?creativeOptions(type,options):{...(type==='waveform'?{...waveOptions(options),lang:options.lang==='de'?'de':'en'}:trackOptions(options)),...reactionOptions(options)};
function presetList(){const select=$('scene-component'),previous=select.value;select.replaceChildren(new Option(t(componentPresets.length?'presetChoose':'presetEmpty'),''));for(const type of ['deck','master','waveform',...creativeTypes]){const group=document.createElement('optgroup');group.label=componentLabel(type);for(const preset of componentPresets.filter(p=>p.type===type))group.append(new Option(preset.name,preset.id));if(group.children.length)select.append(group);}select.value=componentPresets.some(p=>p.id===previous)?previous:'';status();}
async function reloadPresets(){componentPresets=await listPresets();presetList();if(item())selectItem(selected);}
function addComponent(type,value,name,presetId=''){
  if(busy||!scene||scene.items.length>=32)return;
  const options=normalizeComponent(type,value),textHeight=options.fontSize*6+(options.timeline?92:0);
  const spacing=Object.values(options.fieldStyles||{}).reduce((sum,style)=>sum+(style.marginTop||0)+(style.marginBottom||0),0)+(options.elementGap||0)*7;
  const rowHeight=2*options.padding+spacing+(options.coverPosition==='top'?options.coverSize+textHeight+20:Math.max(options.coverSize,textHeight));
  const height=type==='waveform'||creativeTypes.includes(type)?options.height:Math.min(4320,Math.ceil(16+rowHeight+(type==='master'?options.history*(rowHeight*options.historyScale+options.gap):0)));
  const entry={id:id(),type,x:type==='fx'?0:40,y:type==='fx'?0:40,width:type==='fx'?scene.width:options.width+(type==='waveform'||creativeTypes.includes(type)?0:16),height:type==='fx'?scene.height:height,opacity:1,rotation:0,visible:true,options};
  if(name)entry.name=name;entry.presetId=presetId;
  scene.items.push(entry);selected=entry.id;changed();selectItem(selected);
}
$('scene-component').addEventListener('change',status);
$('scene-add-preset').addEventListener('click',()=>{const preset=componentPresets.find(p=>p.id===$('scene-component').value);if(preset)addComponent(preset.type,copy(preset.options),preset.name,preset.id);});
$('scene-presets-refresh').addEventListener('click',()=>run(reloadPresets));
const geometry=[['x',-7680,7680,1],['y',-4320,4320,1],['width',32,7680,1],['height',32,4320,1],['opacity',0,1,.05],['rotation',-360,360,1]];
for(const [name,min,max,step] of geometry){const row=document.createElement('div'),label=document.createElement('label'),input=document.createElement('input');label.htmlFor='item-'+name;label.dataset.i18n='scene_'+name;input.id=label.htmlFor;input.type='number';input.min=min;input.max=max;input.step=step;input.addEventListener('input',()=>{if(input.checkValidity()&&item()){item()[name]=Number(input.value);changed();}});row.append(label,input);$('item-geometry').append(row);}
const trackControls=[['deck','number',1,4,1],['history','number',0,50,1],['width','number',320,1600,10],['historyScale','number',.2,1,.05],['align','select',['left','center','right']],['timeline','checkbox'],['badges','checkbox'],['font','select',Object.keys(fonts)],['fontSize','number',14,40,1],['coverSize','number',32,180,1],['padding','number',4,48,1],['gap','number',0,40,1],['duration','number',0,2000,50],['background','color'],['textColor','color'],['mutedColor','color'],['accent','color'],['opacity','number',0,100,1],['radius','number',0,40,1],['border','number',0,10,1],['shadow','checkbox']];
function optionControls(){const entry=item(),container=$('item-options');container.replaceChildren();if(!entry)return;
  const link=$('item-source-preset');link.replaceChildren(new Option(t('presetIndependent'),''));for(const preset of componentPresets.filter(p=>p.type===entry.type))link.add(new Option(preset.name,preset.id));link.value=entry.presetId||'';
  $('item-link-help').hidden=!entry.presetId;
  $('item-design').disabled=!!entry.presetId;
  let deckRow=$('item-deck-row');
  if(!deckRow){deckRow=document.createElement('label');deckRow.id='item-deck-row';deckRow.append(document.createElement('span'),document.createElement('select'));$('item-design').before(deckRow);}
  deckRow.hidden=entry.type!=='deck';deckRow.firstElementChild.textContent=t('sceneOption_deck');
  const deckSelect=deckRow.lastElementChild;deckSelect.id='option-deck';deckSelect.replaceChildren(...[1,2,3,4].map(id=>new Option(t('deck',{id}),id)));deckSelect.value=entry.options.deck||1;
  deckSelect.onchange=()=>{entry.options={...entry.options,deck:Number(deckSelect.value)};changed();layers();};
  buildControls($('scene-reaction'),reactionControls,reactionOptions(entry.options),(name,value)=>{entry.options=normalizeComponent(entry.type,{...entry.options,[name]:value});changed();},t);
  if(creativeTypes.includes(entry.type)){buildControls(container,creativeControls[entry.type].filter(([name])=>name!=='width'&&name!=='height'),creativeOptions(entry.type,entry.options),(name,value)=>{entry.options=normalizeComponent(entry.type,{...entry.options,[name]:value});changed();},t,media);return;}
  for(const [name,type,a,b,step] of entry.type==='waveform'?waveControls:trackControls){if(name==='deck'||(['history','historyScale'].includes(name)&&entry.type!=='master'))continue;
    const label=document.createElement('label'),input=document.createElement(type==='select'?'select':'input');label.htmlFor='option-'+name;label.textContent=t((entry.type==='waveform'?'wave_':'sceneOption_')+name);input.id=label.htmlFor;
    if(type==='select')for(const value of a)input.add(new Option(t('sceneValue_'+value)===('sceneValue_'+value)?value:t('sceneValue_'+value),value));else{input.type=type;if(type==='range'||type==='number'){input.min=a;input.max=b;input.step=step;}}
    if(type==='checkbox')input.checked=entry.options[name];else input.value=entry.options[name];
    input.addEventListener('input',()=>{if(!input.checkValidity())return;entry.options=normalizeComponent(entry.type,{...entry.options,[name]:type==='checkbox'?input.checked:input.value});changed();if(name==='deck')layers();});container.append(label,input);
  }
  if(entry.type!=='waveform'){
    entry.options=normalizeComponent(entry.type,entry.options);
    for(const name of fieldNames){const label=document.createElement('label'),input=document.createElement('input');input.type='checkbox';input.checked=entry.options.fields.includes(name);input.addEventListener('change',()=>{const values=new Set(entry.options.fields);input.checked?values.add(name):values.delete(name);if(!values.size){input.checked=true;return;}entry.options.fields=[...values];changed();});label.append(input,document.createTextNode(' '+t(name)));container.append(label);}
    trackExtras(container,{read:()=>entry.options,write:value=>{entry.options=normalizeComponent(entry.type,value);changed();},master:entry.type==='master'});
  }
}
$('item-source-preset').addEventListener('change',()=>{const entry=item();if(!entry)return;const preset=componentPresets.find(p=>p.id===$('item-source-preset').value);entry.presetId=preset?.id||'';if(preset)entry.options=normalizeComponent(entry.type,{...copy(preset.options),...(entry.type==='deck'?{deck:entry.options.deck}: {})});changed();selectItem(selected);});
function selectItem(value){selected=value;const entry=item();$('item-fields').hidden=!entry;if(entry){for(const [name] of geometry)$('item-'+name).value=entry[name]??0;$('item-visible').checked=entry.visible;$('item-name').value=entry.name||'';const presets=creativeTypes.includes(entry.type)?creativePresets[entry.type]:entry.type==='waveform'?wavePresets:trackPresets;$('item-preset').replaceChildren(new Option(t('custom'),''));for(const name of Object.keys(presets))$('item-preset').add(new Option(name,name));optionControls();}layers();draw();}
for(const name of ['name','width','height'])$('scene-'+name).addEventListener('input',()=>{const input=$('scene-'+name);if(!input.checkValidity())return;scene[name]=name==='name'?input.value:Number(input.value);changed();});
$('scene-preset').addEventListener('change',()=>{if($('scene-preset').value==='custom')return;[scene.width,scene.height]=$('scene-preset').value.split('x').map(Number);changed();fillDocument();});
for(const name of ['transparent','background'])$('scene-'+name).addEventListener('input',()=>{scene.background=$('scene-transparent').checked?'transparent':$('scene-background').value;changed();});
$('item-visible').addEventListener('change',()=>{if(item()){item().visible=$('item-visible').checked;changed();layers();}});
$('item-preset').addEventListener('change',()=>{const entry=item();if(!entry)return;const presets=creativeTypes.includes(entry.type)?creativePresets[entry.type]:entry.type==='waveform'?wavePresets:trackPresets,preset=presets[$('item-preset').value];if(preset){entry.options=normalizeComponent(entry.type,{...entry.options,...preset});changed();optionControls();}});
document.querySelectorAll('[data-add]').forEach(button=>button.addEventListener('click',()=>{const type=button.dataset.add;addComponent(type,{...(creativeTypes.includes(type)?creativeDefaults(type):type==='waveform'?waveDefaults:trackDefaults),...(type==='master'?{history:3}:{}),lang:getLanguage()});}));
$('item-name').addEventListener('input',()=>{if(item()){item().name=$('item-name').value;changed();layers();}});
$('item-fill').addEventListener('click',()=>{if(item()){Object.assign(item(),{x:0,y:0,width:scene.width,height:scene.height});changed();selectItem(selected);}});
$('item-save-preset').addEventListener('click',()=>run(async()=>{const entry=item();if(!entry)return;const options=normalizeComponent(entry.type,{...entry.options,...(creativeTypes.includes(entry.type)?{width:entry.width,height:entry.height}:{})});await api('/api/presets',{action:'save',preset:{name:entry.name||componentLabel(entry.type),type:entry.type,options}});await reloadPresets();$('scene-message').textContent=t('presetSaved');}));
$('scene-media-refresh').addEventListener('click',()=>run(async()=>{media=await listMedia();optionControls();}));
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
translate();await run(async()=>{media=await listMedia();await reload();await reloadPresets();});
poll(async()=>{
  if(busy||dirty||!scene?.id)return;
  const id=scene.id,revision=scene.revision,values=(await api('/api/scenes')).scenes,current=values.find(value=>value.id===id);
  if(!current||current.revision===revision)return;
  const presets=await listPresets();
  // A user may start editing or switch scenes while either request is in flight.
  if(busy||dirty||scene?.id!==id||scene.revision!==revision)return;
  scenes=values;scene=copy(current);componentPresets=presets;presetList();fillDocument();
},{interval:3000,immediate:false});

document.querySelectorAll('a[data-i18n="creativeAudioSetup"]').forEach(link=>link.hidden=!app?.capabilities?.admin);
