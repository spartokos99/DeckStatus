import {deckOverlayPath,overlayPath,normalize as trackOptions} from './master-options.js';
import {overlayUrl,normalize as waveOptions} from './waveform-options.js';
import {broadcastUrl} from './broadcast.js';
import {t} from './i18n.js';
export function source(item,key,scene){
  const options=item.type==='waveform'?waveOptions(item.options):trackOptions(item.options);
  const path=item.type==='deck'?deckOverlayPath(options):item.type==='master'?overlayPath(options):overlayUrl(options,item.options.lang);
  return {url:broadcastUrl(path,key,scene),width:options.width+(item.type==='waveform'?0:16)};
}
export function renderScene(stage,scene,key,editing=false){
  stage.style.width=scene.width+'px';stage.style.height=scene.height+'px';stage.style.background=scene.background;
  const retained=new Set();
  for(const [index,item] of scene.items.entries()){
    retained.add(item.id);let node=[...stage.children].find(child=>child.dataset.id===item.id);
    if(!node){node=document.createElement('div');node.className='scene-item';node.dataset.id=item.id;const frame=document.createElement('iframe');frame.title=item.type;frame.tabIndex=-1;frame.setAttribute('scrolling','no');node.append(frame);if(editing){const label=document.createElement('span');label.className='scene-item-label';const handle=document.createElement('button');handle.className='scene-handle';handle.type='button';handle.setAttribute('aria-label','Resize');node.append(label,handle);}stage.append(node);}
    node.hidden=!item.visible;node.style.cssText=`position:absolute;left:${item.x}px;top:${item.y}px;width:${item.width}px;height:${item.height}px;opacity:${item.opacity};z-index:${index};`;
    const frame=node.querySelector('iframe'),entry=source(item,key,scene.id),scale=item.width/entry.width;
    frame.style.cssText=`width:${entry.width}px;height:${item.height/scale}px;transform:scale(${scale});transform-origin:top left;border:0;pointer-events:none;`;
    if(frame.getAttribute('src')!==entry.url)frame.src=entry.url;
    if(editing){node.querySelector('.scene-item-label').textContent=item.name||(item.type==='deck'?t('deck',{id:item.options.deck||1}):item.type==='master'?'Master':t('waveNav'));node.querySelector('.scene-handle').setAttribute('aria-label',t('sceneResize'));}
  }
  for(const node of stage.children)if(!retained.has(node.dataset.id))node.remove();
}
