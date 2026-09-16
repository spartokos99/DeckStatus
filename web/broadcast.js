// OBS links carry only a read capability. Never send it to another origin.
const query=new URLSearchParams(location.search);
export function broadcastUrl(path,key=query.get('key'),scene=query.get('scene')){
  const url=new URL(path,location.origin);if(url.origin!==location.origin)throw Error('Invalid broadcast origin');
  if(key){url.searchParams.set('key',key);if(scene)url.searchParams.set('scene',scene);}return url.pathname+url.search;
}
export async function loadBroadcastKeys(){const r=await fetch('/api/broadcast',{cache:'no-store'});if(!r.ok)return {};return r.json();}
// The copy/open links target OBS on the DeckStatus PC. Renderers keep same-origin API/iframe URLs.
export function obsUrl(path,app){
  const url=new URL(path,location.origin);
  if(url.origin!==location.origin)throw Error('Invalid broadcast origin');
  const base=new URL(app?.obsBaseUrl||location.origin);
  if(app?.obsBaseUrl&&(base.protocol!=='http:'||base.hostname!=='127.0.0.1'||base.username||base.password||base.pathname!=='/'||base.search||base.hash))throw Error('Invalid local OBS address');
  return new URL(url.pathname+url.search,base).href;
}
