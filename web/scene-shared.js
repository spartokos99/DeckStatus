import {deckOverlayPath,overlayPath,normalize as trackOptions} from './master-options.js';
import {overlayUrl,normalize as waveOptions} from './waveform-options.js';
import {broadcastUrl} from './broadcast.js';
import {t} from './i18n.js';
import {creativeTypes,normalize as creativeOptions} from './creative-options.js';
import {CreativeRenderer} from './creative-renderer.js';
import {ReactiveLoop,Envelope,transform} from './audio-reactivity.js';
const contexts=new WeakMap();
export function source(item,key,scene){
  const options=item.type==='waveform'?waveOptions(item.options):trackOptions(item.options);
  const path=item.type==='deck'?deckOverlayPath(options):item.type==='master'?overlayPath(options):overlayUrl(options,item.options.lang);
  return {url:broadcastUrl(path,key,scene),width:options.width+(item.type==='waveform'?0:16)};
}
export function renderScene(stage,scene,key,editing=false){
  let context=contexts.get(stage);
  if(!context){context={scene,key};context.loop=new ReactiveLoop(()=>context.scene.items.some(i=>i.visible&&i.options.audioEnabled),(signal,now)=>{
    for(const item of context.scene.items){const node=[...stage.children].find(n=>n.dataset.id===item.id);if(!node||!item.visible)continue;const raw=signal.value(item.options.audioBand||'rms',now),value=node.envelope.update(raw,item.options,now);transform(node,item,value);node.renderer?.draw(value,raw,now,signal.fresh);}
  },key,scene.id);contexts.set(stage,context);window.addEventListener('pagehide',()=>context.loop.stop(),{once:true});}
  context.scene=scene;context.key=key;context.loop.key=key;context.loop.scene=scene.id;
  stage.style.width=scene.width+'px';stage.style.height=scene.height+'px';stage.style.background=scene.background;
  const retained=new Set();
  for(const [index,item] of scene.items.entries()){
    retained.add(item.id);let node=[...stage.children].find(child=>child.dataset.id===item.id);
    if(node&&node.dataset.type!==item.type){node.remove();node=null;}
    if(!node){node=document.createElement('div');node.className='scene-item';node.dataset.id=item.id;node.dataset.type=item.type;node.envelope=new Envelope();
      if(creativeTypes.includes(item.type)){const content=document.createElement('div');node.append(content);node.renderer=new CreativeRenderer(content,item.type);}else{const frame=document.createElement('iframe');frame.title=item.type;frame.tabIndex=-1;frame.setAttribute('scrolling','no');node.append(frame);}
      if(editing){const label=document.createElement('span');label.className='scene-item-label';const handle=document.createElement('button');handle.className='scene-handle';handle.type='button';handle.setAttribute('aria-label','Resize');node.append(label,handle);}stage.append(node);}
    node.hidden=!item.visible;node.style.cssText=`position:absolute;left:${item.x}px;top:${item.y}px;width:${item.width}px;height:${item.height}px;opacity:${item.opacity};z-index:${index};`;
    if(node.renderer){if(item.visible)node.renderer.update(creativeOptions(item.type,item.options),key,scene.id);}
    else{const frame=node.querySelector('iframe'),entry=source(item,key,scene.id),scale=item.width/entry.width;
      // Chromium otherwise paints an opaque default canvas when the iframe and
      // its dark-scheme overlay document disagree, even with transparent CSS.
      const expand=['deck','master'].includes(item.type)&&item.options.overflow==='expand';
      node.style.overflow=expand?'visible':'hidden';
      const frameWidth=expand?Math.max(entry.width,(scene.width-Math.max(0,item.x))/scale):entry.width;
      frame.style.cssText=`color-scheme:dark;background:transparent;width:${frameWidth}px;height:${item.height/scale}px;transform:scale(${scale});transform-origin:top left;border:0;pointer-events:none;`;
      if(item.visible){if(frame.getAttribute('src')!==entry.url)frame.src=entry.url;}else frame.removeAttribute('src');}
    transform(node,item,node.envelope.value);
    if(editing){node.querySelector('.scene-item-label').textContent=item.name||(item.type==='deck'?t('deck',{id:item.options.deck||1}):t({master:'masterSettings',waveform:'waveNav',text:'creativeText',image:'creativeImage',fx:'creativeFx'}[item.type]));node.querySelector('.scene-handle').setAttribute('aria-label',t('sceneResize'));}
  }
  for(const node of stage.children)if(!retained.has(node.dataset.id))node.remove();
}
