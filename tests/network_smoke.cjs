// Real EXE startup/restart checks with isolated settings. No Rekordbox injection or audio capture.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),net=require('node:net'),os=require('node:os');
const {spawn,spawnSync}=require('node:child_process');
const {isolatedStore,authenticate}=require('./exe_auth.cjs');
const root=path.resolve(process.argv[2]||'build/Release'),exe=path.join(root,'DeckStatus.exe');
const artifacts=path.resolve('build/test-artifacts');fs.mkdirSync(artifacts,{recursive:true});
const store=isolatedStore('network smoke'),folder=store.folder,config=path.join(folder,'DeckStatus.network.json');
const delay=ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function freePort(){const server=net.createServer();await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));const port=server.address().port;await new Promise(resolve=>server.close(resolve));return port;}
async function run(args,port,test){
 const child=spawn(exe,[...args,'--network-config',config,'--data-dir',store.data],{cwd:root,windowsHide:true,stdio:['ignore','pipe','pipe']});
 let log='',error,cookie='';child.stdout.on('data',data=>log+=data);child.stderr.on('data',data=>log+=data);child.on('error',value=>error=value);
 const closed=new Promise(resolve=>child.on('close',resolve));
 const request=async(route,body,host='127.0.0.1',publicHost='')=>{
  const base='http://'+host+':'+port;
  const response=await fetch(base+route,{signal:AbortSignal.timeout(4000),headers:{Cookie:cookie,...(publicHost?{Host:publicHost}:{}),...(body?{'Content-Type':'application/json','Origin':publicHost?'https://'+publicHost:base}:{})},...(body?{method:'POST',body:JSON.stringify(body)}:{})});
  const set=response.headers.getSetCookie().find(value=>value.startsWith('deckstatus_session='));if(set)cookie=set.split(';')[0];
  return {status:response.status,data:await response.json()};
 };
 try{
  let ready=false;for(let i=0;i<100;i++){if(error)throw error;if(child.exitCode!==null)throw Error('Test server exited during startup');try{await request('/api/auth/me');ready=true;break;}catch{await delay(50);}}
  assert.ok(ready,'Server did not start');await authenticate(request,log);await test(request);
 }finally{if(child.exitCode===null)child.kill();await closed;}
}
(async()=>{
 try{
  const initial=await freePort(),next=await freePort();
  await run(['--demo','--port',String(initial)],initial,async request=>{
   const settings=(await request('/api/network')).data;assert.equal(settings.active.bind,'127.0.0.1');assert.equal(settings.active.allowRemoteControl,false);
   assert.equal(fs.existsSync(config),false,'Starting wrote configuration');
   assert.equal((await request('/api/app')).data.mode,'rekordbox');
   const saved=await request('/api/network',{bind:'0.0.0.0',port:next,allowRemoteControl:false,publicDomain:'deckstatus.example'});assert.equal(saved.status,200);assert.equal(saved.data.restartRequired,true);
   assert.equal(saved.data.active.bind,'127.0.0.1');assert.equal(saved.data.active.port,initial);
  });
  await run(['--mode','prolink'],next,async request=>{
   const settings=(await request('/api/network')).data;assert.equal(settings.active.bind,'0.0.0.0');assert.equal(settings.active.port,next);assert.equal(settings.restartRequired,false);
   assert.equal((await request('/api/app')).data.mode,'prolink');assert.equal((await request('/api/prolink/devices')).data.status,'stopped');
   assert.equal((await request('/api/audio/state')).data.status,'stopped');
   const address=Object.values(os.networkInterfaces()).flat().find(item=>item.family==='IPv4'&&!item.internal)?.address;
   if(address){const state=await request('/api/state',null,address);assert.equal(state.status,200);assert.equal(state.data.mode,'prolink');console.log('Actual host LAN address accepted by wildcard listener.');}
   // A same-machine LAN request is local; a configured domain request is always remote.
   assert.equal((await request('/api/prolink/settings',null,'127.0.0.1','deckstatus.example')).status,200);
   assert.equal((await request('/api/prolink/settings',{autoConnect:false,devices:[]},'127.0.0.1','deckstatus.example')).status,403,'Remote read-only connection settings allowed writes');
  });
  const overridden=await freePort();
  const address=Object.values(os.networkInterfaces()).flat().find(item=>item.family==='IPv4'&&!item.internal)?.address;
  if(address){
   const interfacePort=await freePort();
   fs.writeFileSync(config,JSON.stringify({bind:address,port:interfacePort,allowRemoteControl:false,publicDomain:'deckstatus.example'}));
   await run(['--demo'],interfacePort,async request=>{
    for(const host of ['127.0.0.1','localhost',address]){
     assert.equal((await request('/api/state',null,host)).status,200,'Selected interface lost local/LAN access');
     const app=(await request('/api/app',null,host)).data;assert.equal(app.obsBaseUrl,'http://127.0.0.1:'+interfacePort);
    }
    const settings=(await request('/api/network')).data;assert.equal(settings.active.publicDomain,'deckstatus.example');
   });
   fs.writeFileSync(config,JSON.stringify({bind:'0.0.0.0',port:next,allowRemoteControl:false}));
   console.log('Selected LAN interface, localhost and 127.0.0.1 work together with a configured domain.');
  }
  await run(['--demo','--bind','127.0.0.1','--port',String(overridden),'--allow-remote-control'],overridden,async request=>{
   const settings=(await request('/api/network')).data;assert.equal(settings.active.bind,'127.0.0.1');assert.equal(settings.active.allowRemoteControl,true);assert.equal(settings.saved.bind,'0.0.0.0');assert.equal(settings.saved.port,next);assert.equal(settings.commandLineOverrides,true);
  });
  const invalid=spawnSync(exe,['--demo','--bind','evil.example','--network-config',config,'--data-dir',store.data],{cwd:root,windowsHide:true,encoding:'utf8',timeout:5000});assert.equal(invalid.status,1);
  assert.equal(JSON.parse(fs.readFileSync(config,'utf8')).bind,'0.0.0.0');
  console.log('Network EXE smoke passed: local default, saved bind/port after restart, ProLink mode, LAN URL, CLI overrides, invalid address and audio off.');
 }finally{store.cleanup();}
})().catch(error=>{console.error(error);process.exitCode=1;});
