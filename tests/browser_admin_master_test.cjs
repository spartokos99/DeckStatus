// Synthetic administration fixture; no hardware or network discovery.
const assert=require('node:assert/strict'),{withBrowser}=require('./browser_fixture.cjs');
const defaults={detectionBeats:128,interruptBeats:16,onlyAfterEnd:false,useOnAir:true};
let settings={...defaults},canControl=true,failure=false,posts=[],mode='rekordbox';
const describe=()=>({...settings,available:true,canControl,onAirAvailable:mode==='prolink',playbackSource:mode==='prolink'?'device':'position'});
withBrowser((req,res,url)=>{
 if(!url.pathname.startsWith('/api/'))return false;res.setHeader('Content-Type','application/json');
 if(url.pathname==='/api/app')res.end(JSON.stringify({mode,canControl,capabilities:{admin:true,scenes:true,audioWaveform:true},user:{username:'admin'}}));
 else if(url.pathname==='/api/admin/users')res.end('{"users":[]}');
 else if(url.pathname==='/api/admin/ratings')res.end('{"tracks":[]}');
 else if(url.pathname==='/api/audio/devices')res.end('{"devices":[],"error":null}');
 else if(url.pathname==='/api/admin/audio')res.end(JSON.stringify({settings:{deviceId:'',deviceName:'',autoStart:false},state:{status:'stopped'},canControl,controlError:null}));
 else if(url.pathname==='/api/admin/master'){
  if(failure){res.statusCode=503;res.end('{"error":"masterSettingsUnavailable"}');return true;}
  if(req.method==='GET')res.end(JSON.stringify(describe()));
  else{let body='';req.on('data',chunk=>body+=chunk);req.on('end',()=>{
   const command=JSON.parse(body);posts.push(command);
   assert.deepEqual(Object.keys(command).sort(),Object.keys(defaults).sort(),'Old method or hold settings still sent');
   settings={...settings,...command};res.end(JSON.stringify(describe()));});}
 }else{res.statusCode=404;res.end('{}');}return true;
},async({navigate,evaluate,until,delay,call,screenshot})=>{
 const $=id=>'document.getElementById('+JSON.stringify('admin-master-'+id)+')';
 const set=async(id,value)=>evaluate(`{const input=${$(id)};if(input.type==='checkbox')input.checked=${JSON.stringify(value)};else input.value=${JSON.stringify(String(value))};input.dispatchEvent(new Event('input'));}`);
 const ready=()=>until(()=>evaluate($('interrupt')+'?.disabled===false'),'Detection controls stayed disabled');
 let visit=0;const open=lang=>navigate('/admin?lang='+lang+'&visit='+(++visit)+'#master');
 await open('en');await ready();
 await until(()=>evaluate('!document.querySelector("[data-pane=master]").hidden'),'Master tab missing');
 assert.equal(await evaluate('document.querySelector("#admin-master-hold, #admin-master-method")'),null);
 assert.equal(await evaluate($('beats')+'.value'),'128');assert.equal(await evaluate($('interrupt')+'.value'),'16');
 assert.equal(await evaluate($('onair')+'.disabled && !'+$('onair')+'.checked && !'+$('rekordbox')+'.hidden'),true);
 assert.equal(await evaluate($('save')+'.disabled'),true);assert.deepEqual(posts,[]);
 await set('beats',64);await delay(650);
 assert.equal(settings.detectionBeats,128,'Draft saved without confirmation');
 assert.equal(await evaluate($('pending')+'.hidden'),false);
 await evaluate($('save')+'.click()');await until(()=>settings.detectionBeats===64,'Save failed');await ready();
 assert.deepEqual(posts,[{...defaults,detectionBeats:64}]);
 await open('en');await ready();assert.equal(await evaluate($('beats')+'.value'),'64');
 await set('end',true);assert.equal(await evaluate($('beats')+'.disabled'),true,'End-only did not disable smart timing');
 assert.equal(await evaluate($('interrupt')+'.disabled'),false);
 await evaluate($('save')+'.click()');await until(()=>settings.onlyAfterEnd,'End-only not persisted');await ready();
 await set('end',false);
 for(const [id,value] of [['beats',''],['beats',0],['beats',1025],['beats',1.5],['interrupt',-1],['interrupt',1025],['interrupt','']]){
  await set(id,value);assert.equal(await evaluate($('save')+'.disabled'),true,'Invalid input was savable');
  await set('beats',64);await set('interrupt',16);
 }
 await screenshot('admin-master-en');
 await evaluate('document.querySelector("[data-language]").value="de";document.querySelector("[data-language]").dispatchEvent(new Event("change"))');
 assert.equal(await evaluate('document.querySelector("[data-tab=master]").textContent'),'Master-Erkennung');
 assert.equal(await evaluate('document.querySelector("[data-i18n=masterOnlyAfterEnd]").textContent'),'Erst nach Ende des letzten Tracks melden');
 await call('Emulation.setDeviceMetricsOverride',{width:390,height:844,deviceScaleFactor:1,mobile:false});
 assert.equal(await evaluate('document.documentElement.scrollWidth<=innerWidth'),true,'Detection settings overflow mobile');
 canControl=false;await open('de');
 await until(()=>evaluate($('beats')+'.disabled&&'+$('readonly')+'.hidden===false'),'Remote policy ignored');
 canControl=true;failure=true;await open('de');
 await until(()=>evaluate($('message')+'.textContent.length>0&&'+$('interrupt')+'.disabled'),'Outage not shown');
 failure=false;mode='prolink';await open('en');await ready();
 assert.equal(await evaluate($('onair')+'.disabled || !'+$('rekordbox')+'.hidden'),false);
 await set('onair',false);await set('interrupt',0);
 await evaluate($('save')+'.click()');await until(()=>settings.useOnAir===false&&settings.interruptBeats===0,'On-Air/interrupt settings not saved');await ready();
 await open('de');await ready();assert.equal(await evaluate($('onair')+'.checked'),false);
 await evaluate($('reset')+'.click()');
 assert.equal(await evaluate($('reset')+'.disabled'),true,'Reset failed');
 await evaluate($('save')+'.click()');await until(()=>JSON.stringify(settings)===JSON.stringify(defaults),'Defaults not saved');await ready();
 await call('Emulation.setDeviceMetricsOverride',{width:1440,height:1120,deviceScaleFactor:1,mobile:false});
 await screenshot('admin-master-prolink');
 console.log('Single detection UI passed: four settings in both modes, end-only, capability warning, draft/save/reset, EN/DE, bounds, mobile and remote policy.');
}).catch(error=>{console.error(error);process.exitCode=1;});
