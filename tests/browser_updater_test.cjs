// Synthetic updater UI: no download, installation, audio or hardware is started.
const assert=require('node:assert/strict'),{withBrowser}=require('./browser_fixture.cjs'),version=require('../tools/version.cjs');
let failure=false,role='admin',publicView=false,posts=[];
const state={current:version,status:'idle',busy:false,error:'',checkedAt:Date.now(),canControl:true,latest:{version:'99.0.0',available:true,downloadable:true},package:null,lastResult:null};
withBrowser((req,res,url)=>{
 if(!url.pathname.startsWith('/api/'))return false;res.setHeader('Content-Type','application/json');
 if(url.pathname==='/api/app')res.end(JSON.stringify({mode:'rekordbox',capabilities:{admin:role==='admin',history:true},user:{username:'fixture',role},update:{available:true,version:'99.0.0'}}));
 else if(url.pathname==='/api/auth/me')res.end(JSON.stringify({user:publicView?null:{username:'fixture',role}}));
 else if(url.pathname==='/api/admin/users')res.end('{"users":[]}');
 else if(url.pathname==='/api/admin/ratings')res.end('{"tracks":[]}');
 else if(url.pathname==='/api/audio/devices')res.end('{"devices":[]}');
 else if(url.pathname==='/api/admin/updater'){
  if(failure){res.statusCode=503;res.end('{"error":"updateUnavailable"}');}
  else if(req.method==='GET')res.end(JSON.stringify(state));
  else{let text='';req.on('data',d=>text+=d);req.on('end',()=>{posts.push(JSON.parse(text));res.end(JSON.stringify(state));});}
 }else{res.statusCode=404;res.end('{}');}return true;
},async({navigate,evaluate,until,call,screenshot})=>{
 let visit=0;const open=lang=>navigate('/admin?lang='+lang+'&visit='+(++visit)+'#updater');
 const click=id=>evaluate(`document.getElementById(${JSON.stringify(id)}).click()`);
 await open('en');await until(()=>evaluate('!document.getElementById("update-check").disabled'),'Updater did not load');
 assert.equal(await evaluate('document.querySelector("[data-pane=updater]").hidden'),false);
 assert.equal(await evaluate('document.getElementById("nav-update").getAttribute("href")'),'/admin#updater');
 assert.equal(await evaluate('document.getElementById("update-current").textContent'),version);
 assert.equal(await evaluate('document.getElementById("update-install").disabled'),true);
 await click('update-check');await until(()=>posts.length===1,'Check not submitted');assert.deepEqual(posts,[{action:'check'}]);
 state.package={version:'99.0.0',sha256:'a'.repeat(64),source:'github'};state.status='ready';
 await until(()=>evaluate('!document.getElementById("update-install").disabled'),'Prepared package not shown');
 assert.equal(await evaluate('document.getElementById("update-sha").textContent'),'a'.repeat(64));
 await evaluate('window.confirm=()=>false');await click('update-install');assert.equal(posts.length,1,'Cancelled confirmation installed');
 await screenshot('admin-updater-en');
 await evaluate('document.querySelector("[data-language]").value="de";document.querySelector("[data-language]").dispatchEvent(new Event("change"))');
 assert.equal(await evaluate('document.getElementById("update-install").textContent'),'Installieren und neu starten');
 assert.ok((await evaluate('document.getElementById("nav-update").textContent')).includes('verfügbar'),'German update label corrupted');
 await call('Emulation.setDeviceMetricsOverride',{width:390,height:844,deviceScaleFactor:1,mobile:false});
 assert.equal(await evaluate('document.documentElement.scrollWidth<=innerWidth'),true,'Updater overflows mobile');
 state.canControl=false;await until(()=>evaluate('document.getElementById("update-install").disabled'),'Remote policy ignored');
 assert.equal(await evaluate('document.getElementById("update-upload").disabled'),true);
 state.canControl=true;await until(()=>evaluate('!document.getElementById("update-check").disabled'),'Controls did not recover');
 failure=true;await until(()=>evaluate('document.getElementById("update-check").disabled'),'Outage left actions enabled');
 failure=false;state.busy=true;state.status='uploading';await until(()=>evaluate('!document.getElementById("update-cancel-upload").hidden'),'Interrupted upload cannot be discarded');
 await click('update-cancel-upload');await until(()=>posts.some(p=>p.action==='cancelUpload'),'Cancel not submitted');
 state.busy=false;state.status='ready';await open('en');await until(()=>evaluate('!document.getElementById("update-install").disabled'),'Package not ready');
 await evaluate('window.confirm=()=>true');await click('update-install');await until(()=>posts.some(p=>p.action==='install'),'Install not submitted');assert.deepEqual(posts.at(-1),{action:'install',confirm:true});
 await until(()=>evaluate('document.getElementById("update-status").textContent.includes("restarting")'),'Restart status missing');
 role='operator';await navigate('/?visit=operator');await until(()=>evaluate('!document.getElementById("nav-update").hidden'),'Operator indicator missing');assert.equal(await evaluate('document.getElementById("nav-update").href'),'https://github.com/spartokos99/DeckStatus/releases/latest');
 publicView=true;await navigate('/history?visit=public');await until(()=>evaluate('document.querySelector("nav").hidden'),'Public history navigation visible');assert.equal(await evaluate('document.getElementById("nav-update").hidden'),true);
 console.log('Updater browser passed: version indicator/admin link, check/prepare/confirmation, EN/DE/mobile, remote/outage controls, interrupted uploads and public/operator visibility.');
}).catch(error=>{console.error(error);process.exitCode=1;});
