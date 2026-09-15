// OBS links carry only a read capability. Never send it to another origin.
const query=new URLSearchParams(location.search);
export function broadcastUrl(path,key=query.get('key'),scene=query.get('scene')){
  const url=new URL(path,location.origin);if(url.origin!==location.origin)throw Error('Invalid broadcast origin');
  if(key){url.searchParams.set('key',key);if(scene)url.searchParams.set('scene',scene);}return url.pathname+url.search;
}
export async function loadBroadcastKeys(){const r=await fetch('/api/broadcast',{cache:'no-store'});if(!r.ok)return {};return r.json();}
