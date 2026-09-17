// Synthetic administration fixture: no WASAPI source is ever opened.
const assert=require('node:assert/strict'),{withBrowser}=require('./browser_fixture.cjs');
let settings={deviceId:'',deviceName:'',autoStart:false},active='',canControl=true,missing=false,posts=[],failure=false;
const devices=[{id:'input-1',name:'Studio interface',kind:'input'},{id:'loop-1',name:'Monitor output',kind:'loopback'},{id:'unsafe',name:'<img src=x onerror=alert(1)>',kind:'input'}];
const state=()=>({status:active?'capturing':'stopped',deviceId:active,deviceName:devices.find(d=>d.id===active)?.name||'',fresh:!!active,left:Array(1024).fill(active?.length?.1:0)});
const describe=()=>({settings,state:state(),canControl,controlError:null});
withBrowser((req,res,url)=>{
 if(!url.pathname.startsWith('/api/'))return false;res.setHeader('Content-Type','application/json');
 if(url.pathname==='/api/app')res.end(JSON.stringify({mode:'rekordbox',canControl,capabilities:{admin:true,scenes:true,audioWaveform:true},user:{username:'admin'}}));
 else if(url.pathname==='/api/admin/users')res.end('{"users":[]}');
 else if(url.pathname==='/api/admin/ratings')res.end('{"tracks":[]}');
 else if(url.pathname==='/api/audio/devices')res.end(JSON.stringify({devices:missing?[]:devices,error:null}));
 else if(url.pathname==='/api/admin/audio'){
  if(failure){res.statusCode=503;res.end('{}');return true;}
  if(req.method==='GET')res.end(JSON.stringify(describe()));
  else{let body='';req.on('data',b=>body+=b);req.on('end',()=>{const command=JSON.parse(body);posts.push(command);if(command.action==='save')settings={deviceId:command.deviceId,deviceName:devices.find(d=>d.id===command.deviceId)?.name||'',autoStart:command.autoStart};if(command.action==='start')active=settings.deviceId;if(command.action==='stop')active='';res.end(JSON.stringify(describe()));});}
 }else{res.statusCode=404;res.end('{}');}return true;
},async({navigate,evaluate,until,delay,call,screenshot})=>{
 const click=id=>evaluate(`document.getElementById(${JSON.stringify('admin-audio-'+id)}).click()`);
 const change=async(id,value)=>evaluate(`(()=>{const input=document.getElementById(${JSON.stringify('admin-audio-'+id)});input.${typeof value==='boolean'?'checked':'value'}=${JSON.stringify(value)};input.dispatchEvent(new Event('change'));})()`);
 const ready=()=>until(()=>evaluate('document.getElementById("admin-audio-device")?.disabled===false'),'Admin audio not ready');
 await navigate('/admin?lang=en#audio');await ready();await until(()=>evaluate('!document.querySelector("[data-pane=audio]").hidden'),'Audio deep link did not select tab');
 assert.equal(posts.length,0);assert.equal(active,'');assert.equal(await evaluate('document.querySelectorAll("#admin-audio-device img").length'),0);
 await change('device','input-1');await change('auto',true);await delay(650);
 assert.equal(await evaluate('document.querySelector("#admin-audio-device").value'),'input-1','Polling overwrote draft');
 assert.equal(await evaluate('document.querySelector("#admin-audio-auto").checked'),true);assert.equal(posts.length,0);
 assert.equal(await evaluate('document.querySelector("#admin-audio-start").disabled'),true,'Unsaved input can start');
 await click('save');await until(()=>settings.autoStart&&settings.deviceId==='input-1','Save missing');
 assert.equal(active,'','Saving autostart began capture');await until(()=>evaluate('!document.querySelector("#admin-audio-start").disabled'),'Start unavailable');
 await click('start');await until(()=>active==='input-1','Start failed');await until(()=>evaluate('!document.querySelector("#admin-audio-stop").disabled'),'Stop unavailable');
 await click('stop');await until(()=>active==='','Stop failed');assert.equal(settings.deviceId,'input-1');assert.equal(settings.autoStart,true);
 await navigate('/admin?lang=en#audio');await ready();assert.equal(await evaluate('document.querySelector("#admin-audio-device").value'),'input-1');assert.equal(active,'','Reload started capture');
 await change('device','loop-1');await change('auto',false);await click('save');await until(()=>settings.deviceId==='loop-1'&&!settings.autoStart,'Switch save failed');
 await until(()=>evaluate('!document.querySelector("#admin-audio-start").disabled'),'Start busy');await click('start');await until(()=>active==='loop-1','Input switch failed');
 await screenshot('admin-audio-en');
 await evaluate('document.querySelector("[data-language]").value="de";document.querySelector("[data-language]").dispatchEvent(new Event("change"))');
 assert.equal(await evaluate('document.querySelector("[data-tab=audio]").textContent'),'Audio-Eingang');
 await call('Emulation.setDeviceMetricsOverride',{width:390,height:844,deviceScaleFactor:1,mobile:false});assert.equal(await evaluate('document.documentElement.scrollWidth<=innerWidth'),true,'Admin audio overflows mobile');
 missing=true;await click('refresh');await until(()=>evaluate('document.querySelector("#admin-audio-device").selectedOptions[0].textContent.includes("Nicht verfügbar")'),'Missing saved device disappeared');
 assert.equal(await evaluate('document.querySelector("#admin-audio-start").disabled'),true);
 canControl=false;await until(()=>evaluate('document.querySelector("#admin-audio-device").disabled'),'Remote policy ignored');
 await evaluate('document.querySelector("#rating-refresh").click()');await delay(200);assert.equal(await evaluate('document.querySelector("#admin-audio-stop").disabled'),true,'Another admin operation enabled remote audio controls');
 canControl=true;missing=false;failure=true;await until(()=>evaluate('document.querySelector("#admin-audio-message").textContent.length>0&&document.querySelector("#admin-audio-device").disabled'),'Outage not shown');failure=false;await ready();
 console.log('Admin audio UI passed: draft preservation, explicit save/start/stop, retained device/autostart, no implicit capture, unavailable devices, remote policy, EN/DE, mobile and recovery.');
}).catch(error=>{console.error(error);process.exitCode=1;});
