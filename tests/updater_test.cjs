// Real HTTP upload/restart/rollback tests. Only the owned --demo installation is executed.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),net=require('node:net'),http=require('node:http'),crypto=require('node:crypto');
const {spawn}=require('node:child_process'),{isolatedStore,authenticate}=require('./exe_auth.cjs'),version=require('../tools/version.cjs');
const store=isolatedStore('updater space'),root=path.join(store.folder,'installed app'),data=path.join(store.folder,'private data'),network=path.join(store.folder,'network config.json');
const fixture=path.resolve('tests/updater_fixture.ps1'),helper=path.resolve('tools/DeckStatus.Update.ps1'),settings=path.join(store.folder,'fixture.json');
const delay=ms=>new Promise(resolve=>setTimeout(resolve,ms));let cookie='',port,child,locker,log='';
const config={testRoot:store.folder,source:path.resolve(process.env.DECKSTATUS_TEST_ROOT||'build/Release'),prolink:path.resolve('build/prolink-package')};
fs.writeFileSync(settings,JSON.stringify(config));
function ps(file,args=[]){const proc=spawn('powershell.exe',['-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',file,...args],{windowsHide:true,stdio:['pipe','pipe','pipe']});return proc;}
async function run(file,args=[]){return new Promise((resolve,reject)=>{const proc=ps(file,args);let output='';proc.stdout.on('data',d=>output+=d);proc.stderr.on('data',d=>output+=d);proc.on('error',reject);proc.on('close',code=>resolve({code,output}));});}
async function until(check,message,timeout=90000){const end=Date.now()+timeout;while(Date.now()<end){if(await check())return;await delay(150);}throw Error(message);}
async function request(url,body,options={}){
 const payload=body===undefined?undefined:Buffer.isBuffer(body)?body:Buffer.from(JSON.stringify(body));
 return new Promise((resolve,reject)=>{const req=http.request('http://127.0.0.1:'+port+url,{method:body===undefined?'GET':'POST',headers:{...(payload?{'Content-Type':Buffer.isBuffer(body)?'application/octet-stream':'application/json','Content-Length':payload.length}:{}),...(cookie?{Cookie:cookie}:{}),...options.headers},signal:AbortSignal.timeout(15000)},res=>{let text='';res.on('data',d=>text+=d);res.on('end',()=>{const set=res.headers['set-cookie']?.[0];if(set)cookie=set.split(';')[0];try{resolve({status:res.statusCode,data:JSON.parse(text)});}catch(error){reject(error);}});});req.on('error',reject);req.end(payload);});
}
const command=(action,extra={})=>request('/api/admin/updater',{action,...extra});
async function state(){return (await request('/api/admin/updater')).data;}
async function prepared(){await until(async()=>!(await state()).busy,'Preparation did not finish');const value=await state();assert.equal(value.error,'');assert.equal(value.status,'ready');return value;}
async function upload(zip){const bytes=fs.readFileSync(zip),start=await command('beginUpload',{size:bytes.length});assert.equal(start.status,202);const {id,chunkSize}=start.data;
 assert.equal((await request('/api/admin/updater/upload?id='+id+'&offset=1',bytes.subarray(0,10))).status,409,'Out-of-order chunk accepted');
 for(let offset=0;offset<bytes.length;offset+=chunkSize){assert.equal((await request('/api/admin/updater/upload?id='+id+'&offset='+offset,bytes.subarray(offset,offset+chunkSize))).status,200);}
 assert.equal((await command('finishUpload',{id})).status,202);return prepared();}
async function helperCase(archive,expected,current=version){
 const job=path.join(root,'DeckStatus.update','job-'+crypto.randomBytes(16).toString('hex'));fs.mkdirSync(job,{recursive:true});fs.copyFileSync(archive,path.join(job,'package.zip'));
 fs.writeFileSync(path.join(job,'plan.json'),JSON.stringify({root,data,network,current}));
 const result=await run(helper,['-Action','Prepare','-Plan',path.join(job,'plan.json')]);
 const answer=JSON.parse(fs.readFileSync(path.join(job,'result.json')));assert.equal(answer.error,expected,JSON.stringify(answer));assert.equal(result.code,expected?1:0);return job;
}
async function releaseCases(zip){
 const mock=path.join(store.folder,'mock-network.ps1'),releaseFile=path.join(store.folder,'release.json'),checksum=path.join(store.folder,'checksum.txt');
 const literal=value=>"'"+value.replaceAll("'","''")+"'";
 const original=fs.readFileSync(helper,'utf8');assert.ok(original.includes('function Latest {'));
 // Replace only transport in a disposable helper; release selection, digests and preparation stay real.
 fs.writeFileSync(mock,original.replace('function Latest {',`function Request-File([string]$url,[string]$destination,[long]$limit) {
 $file=if ($url.EndsWith('/latest')) { ${literal(releaseFile)} } elseif ($url.EndsWith('.sha256')) { ${literal(checksum)} } else { ${literal(zip)} }
 Copy-Item -LiteralPath $file -Destination $destination -Force
}
function Latest {`));
 const assetVersion=version,name='DeckStatus-'+assetVersion+'-win-x64.zip',prefix='https://github.com/spartokos99/DeckStatus/releases/download/v'+assetVersion+'/';
 const digest=crypto.createHash('sha256').update(fs.readFileSync(zip)).digest('hex');
 const release={tag_name:'v'+assetVersion,draft:false,prerelease:false,assets:[{name,state:'uploaded',size:fs.statSync(zip).size,browser_download_url:prefix+name,digest:'sha256:'+digest}]};
 async function check(action,current,fixture){
  fs.writeFileSync(releaseFile,JSON.stringify(fixture));const job=path.join(root,'DeckStatus.update','job-'+crypto.randomBytes(16).toString('hex'));fs.mkdirSync(job,{recursive:true});
  fs.writeFileSync(path.join(job,'plan.json'),JSON.stringify({root,data,network,current}));
  const result=await run(mock,['-Action',action,'-Plan',path.join(job,'plan.json')]);const answer=JSON.parse(fs.readFileSync(path.join(job,'result.json')));assert.equal(result.code,answer.ok?0:1,result.output);return answer;
 }
 for(const [current,remote,available] of [['2.3.1','v2.3.1',false],['2.3.1','v2.2.0',false],['2.3.1','v2.3.10',true],['2.9.0','v2.10.0',true]]){const result=await check('Check',current,{...release,tag_name:remote});assert.equal(result.ok,true);assert.equal(result.latest.available,available);}
 assert.equal((await check('Check',version,{...release,prerelease:true})).error,'updateInvalidRelease');
 assert.equal((await check('Check',version,{...release,tag_name:'v2.4.0-rc1'})).error,'updateInvalidVersion');
 assert.equal((await check('Download','0.0.0',release)).package?.version,version);
 const corrupted=structuredClone(release);corrupted.assets[0].digest='sha256:'+'0'.repeat(64);
 assert.equal((await check('Download','0.0.0',corrupted)).error,'updateChecksum');
 const sidecar=structuredClone(release);delete sidecar.assets[0].digest;sidecar.assets.push({name:name+'.sha256',state:'uploaded',browser_download_url:prefix+name+'.sha256'});fs.writeFileSync(checksum,digest+'  '+name+'\n');
 assert.equal((await check('Download','0.0.0',sidecar)).package?.version,version);
 const foreign=structuredClone(release);foreign.assets[0].browser_download_url='https://example.invalid/'+name;assert.equal((await check('Download','0.0.0',foreign)).error,'updateNoDownload');
 fs.writeFileSync(releaseFile,JSON.stringify({...release,tag_name:'v99.0.0'}));return mock;
}
(async()=>{try{
 const built=await run(fixture,['-Action','Package','-Config',settings]);assert.equal(built.code,0,built.output);
 fs.cpSync(path.join(store.folder,'package'),root,{recursive:true});fs.writeFileSync(path.join(root,'keep.txt'),'unmanaged');fs.writeFileSync(path.join(root,'web','obsolete.txt'),'old');
 const zip=path.join(store.folder,'release.zip');
 await helperCase(zip,undefined);
 await helperCase(zip,'updateDowngrade','999999.0.0');
 for(const [name,entry,error] of [['traversal','web/../../escaped.txt','updateUnsafeArchive'],['private','web/portal.json','updateUnsafeArchive'],['absolute','C:/file','updateUnsafeArchive'],['duplicate','WEB/index.html','updateUnsafeArchive'],['missing','web/extra.txt','updateInvalidPackage']]){
  const archive=path.join(store.folder,name+'.zip'),cfg=path.join(store.folder,name+'.json');
  fs.writeFileSync(cfg,JSON.stringify({...config,archive,entries:['web/index.html','web/locales/en.json','web/locales/de.json','README.md',entry]}));
  const made=await run(fixture,['-Action','Archive','-Config',cfg]);assert.equal(made.code,0,made.output);await helperCase(archive,error);
 }
 assert.equal(fs.existsSync(path.join(root,'escaped.txt')),false);
 const mock=await releaseCases(zip);fs.copyFileSync(mock,path.join(root,'DeckStatus.Update.ps1'));
 const reservation=net.createServer();await new Promise(resolve=>reservation.listen(0,'127.0.0.1',resolve));port=reservation.address().port;await new Promise(resolve=>reservation.close(resolve));
 fs.writeFileSync(network,JSON.stringify({bind:'127.0.0.1',port,allowRemoteControl:false,publicDomain:'updater.invalid'}));
 child=spawn(path.join(root,'DeckStatus.exe'),['--demo','--port',String(port),'--data-dir',data,'--network-config',network],{cwd:root,windowsHide:true,stdio:['ignore','pipe','pipe'],env:{...process.env,DECKSTATUS_NO_UPDATE_CHECK:'1'}});
 child.stdout.on('data',d=>log+=d);child.stderr.on('data',d=>log+=d);child.on('error',error=>{throw error;});
 await until(async()=>{try{return (await request('/api/auth/me')).status===200;}catch{return false;}},'Demo did not start');
 for(const endpoint of ['/api/admin/updater','/api/admin/updater/upload?id=x&offset=0'])assert.equal((await request(endpoint,endpoint.includes('upload')?Buffer.from('test'):undefined)).status,401);
 const temporary=log.match(/Temporary password: ([a-f0-9]+)/)?.[1];assert.ok(temporary);assert.equal((await request('/api/auth/login',{username:'admin',password:temporary})).status,200);
 assert.equal((await command('check')).status,403,'Pending password allowed updater');
 await authenticate(request,log);
 assert.equal((await command('install',{confirm:true})).status,400);
 assert.equal((await command('beginUpload',{size:536870913})).status,413);
 assert.equal((await request('/api/admin/updater',{action:'check'},{headers:{Origin:'https://evil.invalid'}})).status,403);
 assert.equal((await request('/api/admin/updater',undefined,{headers:{Host:'evil.invalid'}})).status,403);
 assert.equal((await request('/api/admin/updater',{action:'check'},{headers:{Host:'updater.invalid',Origin:'https://updater.invalid'}})).status,403,'Proxy bypassed remote policy');
 assert.equal((await command('check')).status,202);await until(async()=>!(await state()).busy,'Release check did not finish');assert.equal((await state()).error,'');
 assert.equal((await request('/api/app')).data.update.version,'99.0.0');assert.equal((await request('/api/app')).data.update.available,true);assert.ok(log.includes('[Updater] Update available 99.0.0'));
 assert.equal((await command('check')).status,429,'Release check was not rate limited');fs.copyFileSync(helper,path.join(root,'DeckStatus.Update.ps1'));
 assert.equal((await request('/api/admin/users',{action:'save',username:'operator',password:'Temporary test operator 42!',role:'operator'})).status,200);
 const adminCookie=cookie;cookie='';assert.equal((await request('/api/auth/login',{username:'operator',password:'Temporary test operator 42!'})).status,200);
 await request('/api/auth/password',{currentPassword:'Temporary test operator 42!',password:'Changed test operator 42!'});await request('/api/auth/login',{username:'operator',password:'Changed test operator 42!'});
 assert.equal((await command('check')).status,403,'Operator allowed update');cookie=adminCookie;
 const preset=await request('/api/presets',{action:'save',preset:{name:'Preserved master',type:'master',options:{history:3}}});assert.equal(preset.status,200);
 const scene=await request('/api/scenes',{action:'save',scene:{name:'Preserved scene',width:1920,height:1080,background:'transparent',items:[{id:'master',type:'master',presetId:preset.data.id,x:20,y:30,width:640,height:500,opacity:1,visible:true,options:{history:3}}]}});assert.equal(scene.status,200);
 assert.equal((await request('/api/admin/master',{holdMs:7000})).status,200);
 fs.mkdirSync(path.join(data,'media'),{recursive:true});fs.writeFileSync(path.join(data,'media','private-fixture.bin'),'private media');
 const originalStore=fs.readFileSync(path.join(data,'portal.json')),originalNetwork=fs.readFileSync(network);
 const packageInfo=await upload(zip);assert.equal(packageInfo.package.version,version);assert.equal(packageInfo.package.sha256,crypto.createHash('sha256').update(fs.readFileSync(zip)).digest('hex'));
 assert.equal((await command('install')).status,400,'Install accepted without confirmation');
 // A local disk modification between preparation and installation is caught before shutdown.
 const jobs=fs.readdirSync(path.join(root,'DeckStatus.update')).filter(n=>n.startsWith('job-'));
 const latestJob=jobs.map(n=>path.join(root,'DeckStatus.update',n)).filter(p=>fs.existsSync(path.join(p,'plan.json'))).sort((a,b)=>fs.statSync(b).mtimeMs-fs.statSync(a).mtimeMs)[0];
 fs.appendFileSync(path.join(latestJob,'stage','web','index.html'),'tamper');
 await command('install',{confirm:true});await until(async()=>!(await state()).busy,'Tamper check did not finish');assert.equal((await state()).error,'updateChecksum');assert.equal(child.exitCode,null);
 await upload(zip);
 const before=fs.readFileSync(path.join(root,'DeckStatus.exe'));
 assert.equal((await command('install',{confirm:true})).status,202);
 await until(()=>child.exitCode!==null,'Old host did not exit');
 await until(async()=>{try{return (await request('/api/auth/me')).status===200;}catch{return false;}},'Updated host did not restart');cookie='';await authenticate(request,'');
 await until(()=>JSON.parse(fs.readFileSync(path.join(root,'DeckStatus.update','last-result.json'))).status==='updateInstalled','Install result missing');
 assert.deepEqual(fs.readFileSync(path.join(data,'portal.json')),originalStore);assert.deepEqual(fs.readFileSync(network),originalNetwork);
 assert.equal(fs.readFileSync(path.join(data,'media','private-fixture.bin'),'utf8'),'private media');assert.equal(fs.existsSync(path.join(root,'web','obsolete.txt')),false);assert.equal(fs.readFileSync(path.join(root,'keep.txt'),'utf8'),'unmanaged');
 assert.deepEqual(fs.readFileSync(path.join(root,'DeckStatus.exe')),before);
 assert.equal((await request('/api/audio/state')).data.status,'stopped');assert.equal((await request('/api/presets')).data.presets[0].id,preset.data.id);
 assert.equal((await request('/api/scenes')).data.scenes[0].key,scene.data.key);assert.equal((await request('/api/admin/master')).data.holdMs,7000);
 const installed=(await state()).lastResult;assert.equal(installed.status,'updateInstalled');assert.ok(fs.existsSync(path.join(installed.backup,'data-backup','portal.json')));
 await until(()=>fs.existsSync(path.join(installed.backup,'result.json')),'Apply helper did not complete');
 // Lock the DLL so replacement fails after the EXE was swapped. The old installation must return.
 await upload(zip);locker=ps(fixture,['-Action','Lock','-Config',settings]);let locked='';locker.stdout.on('data',d=>locked+=d);await until(()=>locked.includes('locked'),'Lock fixture failed');
 assert.equal((await command('install',{confirm:true})).status,202);
 await until(()=>{try{return JSON.parse(fs.readFileSync(path.join(root,'DeckStatus.update','last-result.json'))).status==='updateRolledBack';}catch{return false;}},'Rollback did not finish');
 await until(async()=>{try{cookie='';await authenticate(request,'');return true;}catch{return false;}},'Rolled-back host did not restart');
 assert.deepEqual(fs.readFileSync(path.join(root,'DeckStatus.exe')),before);assert.deepEqual(fs.readFileSync(path.join(data,'portal.json')),originalStore);assert.deepEqual(fs.readFileSync(network),originalNetwork);
 console.log('Updater passed: numeric stable release selection, verified download/digest/sidecar, API/log indicator, ZIP validation/downgrade, authentication/roles/origin/remote policy, chunk order, tamper rejection, explicit install, real restart, preserved stores/media/network, managed file replacement and locked-file rollback.');
}finally{
 if(locker){locker.stdin.end('\n');await new Promise(resolve=>locker.exitCode!==null?resolve():locker.once('close',resolve));}
 const stopped=await run(fixture,['-Action','Stop','-Config',settings]);assert.equal(stopped.code,0,stopped.output);
 // Give the owned helper time to release its final result/lock handles before cleanup.
 await delay(1000);store.cleanup();
}})().catch(error=>{console.error(error);process.exitCode=1;});
