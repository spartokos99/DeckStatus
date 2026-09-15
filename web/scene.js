import {renderScene} from './scene-shared.js';
import {broadcastUrl} from './broadcast.js';
const query=new URLSearchParams(location.search),stage=document.getElementById('scene-stage');let disposed=false,timer;
async function poll(){try{const response=await fetch(broadcastUrl('/api/scene?scene='+encodeURIComponent(query.get('scene')||'')),{cache:'no-store',signal:AbortSignal.timeout(4000)});if(!response.ok){if([401,403,404].includes(response.status))stage.replaceChildren();throw Error('Scene unavailable');}const scene=await response.json();renderScene(stage,scene,query.get('key'));document.title=scene.name+' · DeckStatus';}catch{}if(!disposed)timer=setTimeout(poll,1000);}
window.addEventListener('pagehide',()=>{disposed=true;clearTimeout(timer);});await poll();
