import {creativeTypes,normalize} from './creative-options.js';
import {CreativeRenderer} from './creative-renderer.js';
import {Envelope,ReactiveLoop,transform} from './audio-reactivity.js';
const type=location.pathname.split('/').pop(),query=new URLSearchParams(location.search);
if(creativeTypes.includes(type)){
 const options=normalize(type,Object.fromEntries(query)),node=document.getElementById('component'),renderer=new CreativeRenderer(document.getElementById('creative-content'),type),envelope=new Envelope();node.style.width=options.width+'px';node.style.height=options.height+'px';renderer.update(options,query.get('key'),query.get('scene'));
 const loop=new ReactiveLoop(()=>options.audioEnabled,(signal,now)=>{const raw=signal.value(options.audioBand,now),value=envelope.update(raw,options,now);transform(node,{options,opacity:1},value);renderer.draw(value,raw,now,signal.fresh);},query.get('key'),query.get('scene'));window.addEventListener('pagehide',()=>loop.stop(),{once:true});
}
