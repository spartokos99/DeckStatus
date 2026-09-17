// Real EXE + isolated private data. No Rekordbox, ProLink discovery or audio capture.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),net=require('node:net');
const {spawn}=require('node:child_process'),{withBrowser}=require('./browser_fixture.cjs');
const root=path.resolve(process.env.DECKSTATUS_TEST_ROOT||'build/Release'),output=path.resolve('build/test-artifacts');fs.mkdirSync(output,{recursive:true});
const directory=fs.mkdtempSync(path.join(output,'creative portal ')),data=path.join(directory,'data'),password='Creative test passphrase 42!';
let child,log='';const delay=ms=>new Promise(r=>setTimeout(r,ms));
async function launch(port){log='';child=spawn(path.join(root,'DeckStatus.exe'),['--demo','--port',String(port),'--data-dir',data,'--network-config',path.join(directory,'network.json')],{cwd:root,windowsHide:true,stdio:['ignore','pipe','pipe']});let error;child.on('error',e=>error=e);child.stdout.on('data',b=>log+=b);child.stderr.on('data',b=>log+=b);for(let i=0;i<150;i++){if(error)throw error;if(child.exitCode!==null)throw Error('Test EXE exited');try{if((await fetch('http://127.0.0.1:'+port+'/api/auth/me')).ok)return;}catch{}await delay(50);}throw Error('Startup timed out');}
async function stop(){if(!child||child.exitCode!==null)return;const closed=new Promise(r=>child.once('close',r));child.kill();await closed;child=null;}
(async()=>{try{
 const reservation=net.createServer();await new Promise(r=>reservation.listen(0,'127.0.0.1',r));const port=reservation.address().port;await new Promise(r=>reservation.close(r));await launch(port);
 const temporary=log.match(/Temporary password: ([a-f0-9]+)/)?.[1];assert.ok(temporary);
 // An original two-frame looping GIF, including a comment larger than the ordinary API body limit.
 const header=Buffer.from('47494638396101000100800000000000ffffff21ff0b4e45545343415045322e300301000000','hex');
 const frame=color=>Buffer.from('21f904000a0000002c0000000001000100000202'+color+'0100','hex');
 const comment=Buffer.concat([Buffer.from([0x21,0xfe]),...Array.from({length:280},()=>Buffer.concat([Buffer.from([255]),Buffer.alloc(255,65)])),Buffer.from([0])]);
 const gif=Buffer.concat([header,comment,frame('44'),frame('4c'),Buffer.from([0x3b])]);
 const uploadPath=path.join(directory,'Pulse fixture.gif');fs.writeFileSync(uploadPath,gif);
 const fixture=()=>false;fixture.baseUrl='http://127.0.0.1:'+port;
 await withBrowser(fixture,async({navigate,evaluate,until,call,screenshot})=>{
  const fill=(id,value)=>evaluate(`document.getElementById(${JSON.stringify(id)}).value=${JSON.stringify(value)}`);
  const click=selector=>evaluate(`document.querySelector(${JSON.stringify(selector)}).click()`);
  const input=async(id,value)=>{await fill(id,value);await evaluate(`document.getElementById(${JSON.stringify(id)}).dispatchEvent(new Event('input'))`);};
  const post=(url,body)=>evaluate(`(async()=>{const r=await fetch(${JSON.stringify(url)},{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(${JSON.stringify(body)})});return {status:r.status,body:await r.json()};})()`);
  const get=url=>evaluate(`(async()=> (await fetch(${JSON.stringify(url)})).json())()`);
  const signIn=async secret=>{await navigate('/login?lang=en');await fill('username','admin');await fill('password',secret);await evaluate('document.querySelector("#login-form").requestSubmit()');};
  const settings=async type=>{await navigate('/components/'+type+'?lang=en');await until(()=>evaluate('document.getElementById("component-presets")?.getAttribute("aria-busy")==="false"'),'Creative preset UI missing');};
  const save=async name=>{await input('preset-name',name);await click('#preset-create');await until(()=>evaluate('document.getElementById("preset-message").textContent.includes("Preset saved")'),'Creative preset save failed');return evaluate('document.getElementById("saved-preset").value');};
  await signIn(temporary);await until(()=>evaluate('location.pathname==="/account/password"'),'Password change missing');
  assert.equal((await post('/api/media',{action:'upload',name:'blocked.gif',data:gif.toString('base64')})).status,403);
  await fill('current-password',temporary);await fill('new-password',password);await fill('confirm-password',password);await evaluate('document.querySelector("#password-form").requestSubmit()');
  await until(()=>evaluate('location.pathname==="/login"'),'Password change failed');await signIn(password);await until(()=>evaluate('location.pathname==="/"'),'Login failed');
  await settings('text');await input('creative-controls-text','LIVE SESSION\nStudio North');await input('creative-controls-fontSize',96);const textPreset=await save('Session title');
  await input('creative-controls-text','Temporary');await click('#preset-load');assert.equal(await evaluate('document.querySelector("#creative-controls-text").value'),'LIVE SESSION\nStudio North');
  const url=await evaluate('document.querySelector("#creative-url").value');assert.equal(new URL(url).origin,fixture.baseUrl);assert.equal((await fetch(url)).status,200);
  await settings('image');await until(()=>evaluate('document.getElementById("media-upload") && !document.getElementById("media-upload").disabled'),'Media UI missing');
  const document=await call('DOM.getDocument'),upload=await call('DOM.querySelector',{nodeId:document.root.nodeId,selector:'#media-upload'});await call('DOM.setFileInputFiles',{nodeId:upload.nodeId,files:[uploadPath]});
  await until(()=>evaluate('document.querySelectorAll("#media-list article").length===1 && document.querySelector("#creative-controls-assetId").value.length===64'),'GIF upload failed');
  const assets=(await get('/api/media')).media,asset=assets[0];assert.equal(asset.mime,'image/gif');assert.equal(asset.bytes,gif.length);assert.equal('data' in asset,false);
  await until(()=>evaluate('document.querySelector("#creative-preview").contentDocument?.querySelector("img")?.naturalWidth===1'),'Uploaded GIF preview failed');
  const imagePreset=await save('Pulse image');
  const imageUrl=await evaluate('document.querySelector("#creative-url").value'),imageKey=new URL(imageUrl).searchParams.get('key');
  const response=await fetch(fixture.baseUrl+'/api/media/'+asset.id+'?key='+imageKey);assert.equal(response.status,200);assert.deepEqual(Buffer.from(await response.arrayBuffer()),gif,'GIF bytes were re-encoded');
  assert.equal((await fetch(fixture.baseUrl+'/api/media/'+asset.id)).status,401);assert.equal((await fetch(fixture.baseUrl+'/api/media?key='+imageKey)).status,401);
  assert.equal((await post('/api/media',{action:'delete',id:asset.id})).status,409);
  assert.equal((await post('/api/scenes',{padding:'x'.repeat(70000)})).status,413,'Ordinary API body limit changed');
  // Search is opt-in; mock the public service to check filtering/credit and credential policy deterministically.
  const commons=await evaluate(`(async()=>{
   const {searchCommons,commonsUrl}=await import('/media-library.js');const original=window.fetch;let request;
   window.fetch=async(url,options)=>{request={url,options};return new Response(JSON.stringify({query:{pages:{1:{title:'File:Pulse.gif',imageinfo:[{url:'https://upload.wikimedia.org/example.gif',mime:'image/gif',size:100,width:1,height:1,extmetadata:{Artist:{value:'<b>Example artist</b>'},LicenseShortName:{value:'CC0'}}}]},2:{title:'File:Bad.svg',imageinfo:[{url:'https://evil.example/a.svg',mime:'image/svg+xml',size:100,width:1,height:1}]}}}}));};
   try{return {rows:await searchCommons('pulse',true),request,bad:commonsUrl('https://upload.wikimedia.org.evil.example/file')};}finally{window.fetch=original;}
  })()`);
  assert.equal(commons.rows.length,1);assert.equal(commons.rows[0].credit,'Example artist · CC0');assert.equal(commons.request.options.credentials,'omit');assert.ok(commons.request.url.includes('origin=*'));assert.equal(commons.bad,null);
  await screenshot('creative-image-settings-en');
  await call('Emulation.setDeviceMetricsOverride',{width:390,height:844,deviceScaleFactor:1,mobile:false});assert.equal(await evaluate('document.documentElement.scrollWidth<=innerWidth'),true,'Creative settings overflow');
  await call('Emulation.setDeviceMetricsOverride',{width:1440,height:1120,deviceScaleFactor:1,mobile:false});
  await settings('fx');await input('creative-controls-effect','both');const fxPreset=await save('Fog and flash');
  await evaluate('document.querySelector("[data-language]").value="de";document.querySelector("[data-language]").dispatchEvent(new Event("change"))');
  assert.equal(await evaluate('document.querySelector("label[for=creative-reaction-audioEnabled]").textContent'),'Audio-Reaktion aktivieren');
  await navigate('/scenes?lang=en');await until(()=>evaluate('document.getElementById("scene-component").options.length===4 && !document.getElementById("scene-presets-refresh").disabled'),'Creative scene library missing');
  for(const preset of [textPreset,imagePreset,fxPreset]){await fill('scene-component',preset);await evaluate('document.querySelector("#scene-component").dispatchEvent(new Event("change"))');await click('#scene-add-preset');}
  assert.equal(await evaluate('document.querySelector("#item-width").value'),'1920');assert.equal(await evaluate('document.querySelector("#item-height").value'),'1080');assert.equal(await evaluate('document.querySelector("#item-x").value'),'0');
  await input('scene-name','Creative session');await click('#scene-save');await until(()=>evaluate('document.querySelector("#scene-url").value.includes("key=")'),'Creative scene save failed');
  const scene=(await get('/api/scenes')).scenes[0];assert.deepEqual(scene.items.map(i=>i.type),['text','image','fx']);
  assert.equal((await get('/api/audio/state')).status,'stopped','Loading FX started capture');
  const sceneUrl='/scene?scene='+scene.id+'&key='+scene.key;
  await screenshot('creative-scene-editor-en');
  await click('#nav-logout');await until(()=>evaluate('location.pathname==="/login"'),'Logout failed');await navigate(sceneUrl);
  await until(()=>evaluate('document.querySelector(".scene-item img")?.naturalWidth===1 && document.querySelectorAll(".scene-item").length===3'),'Anonymous creative scene failed');
  assert.equal((await fetch(fixture.baseUrl+'/api/media/'+asset.id+'?scene='+scene.id+'&key='+scene.key)).status,200);
  assert.equal((await fetch(fixture.baseUrl+'/api/media?scene='+scene.id+'&key='+scene.key)).status,401);
  assert.equal((await fetch(fixture.baseUrl+'/api/audio/devices?scene='+scene.id+'&key='+scene.key)).status,401);
  await stop();await launch(port);assert.ok(!log.includes('Temporary password:'));
  await navigate(sceneUrl);await until(()=>evaluate('document.querySelector(".scene-item img")?.naturalWidth===1'),'Restart lost image');
  await signIn(password);await until(()=>evaluate('location.pathname==="/"'),'Restart login failed');
  assert.equal((await get('/api/media')).media[0].id,asset.id);assert.equal((await get('/api/presets')).presets.length,3);assert.equal((await get('/api/scenes')).scenes[0].items[0].options.text,'LIVE SESSION\nStudio North');
  assert.equal((await get('/api/audio/state')).status,'stopped');
  console.log('Creative real EXE browser tests passed: text/image/FX presets, GIF upload and original bytes, quotas/auth scopes, Commons filtering, EN/DE/mobile, full-scene FX, anonymous OBS and restart persistence.');
 });
}finally{await stop();assert.equal(path.dirname(path.resolve(directory)),output);fs.rmSync(directory,{recursive:true,force:true});}})().catch(error=>{console.error(error);process.exitCode=1;});
