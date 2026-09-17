const assert=require('node:assert/strict');const {withBrowser}=require('./browser_fixture.cjs');
let viewer=null,pending=null,admin=false,available=true,vote=0,requests=0;
const track='a'.repeat(64),summary=()=>({count:vote?1:0,average:vote||null,mine:viewer?vote:0,distribution:[0,0,Number(vote===3),0,Number(vote===5)]});
withBrowser((req,res,url)=>{
 if(!url.pathname.startsWith('/api/'))return false;res.setHeader('Content-Type','application/json');const send=v=>res.end(JSON.stringify(v)),status=()=>({available,user:viewer,pending});
 if(req.method==='POST'){let bytes='';req.on('data',v=>bytes+=v);req.on('end',()=>{const body=JSON.parse(bytes);
  if(url.pathname==='/api/public/twitch'){if(body.action==='start')pending={code:'VIEW123',url:'https://www.twitch.tv/activate?public=true&device-code=VIEW123'};if(body.action==='poll'){viewer={id:'123',login:'fixture_viewer'};pending=null;}if(body.action==='logout'){viewer=null;pending=null;}return send(status());}
  if(url.pathname==='/api/public/rating'){requests++;assert.ok(viewer,'Anonymous UI submitted a vote');assert.deepEqual(Object.keys(body).sort(),['stars','track']);vote=body.stars;return send(summary());}
  res.statusCode=404;send({});});return true;}
 if(url.pathname==='/api/history')send({entries:[{entryId:1,trackId:1,title:'Night Drive',artist:'Studio North',album:'Test set',id:1,bpm:128,originalBpm:126,key:'Am',startedAt:Date.now(),ratingId:track,rating:summary()}],total:1,status:'demo',viewer,canViewRatings:admin});
 else if(url.pathname==='/api/public/twitch')send(status());
 else if(url.pathname==='/api/auth/me')send({user:admin?{id:'admin',username:'admin',role:'admin',mustChangePassword:false}:null});
 else if(url.pathname==='/api/app')send({mode:'rekordbox',public:!admin,user:admin?{username:'admin'}:null,capabilities:{admin,history:true},canControl:true});
 else if(url.pathname==='/api/admin/ratings')send({tracks:[{id:track,title:'Night Drive',artist:'Studio North',album:'Test set',...summary()}]});
 else if(url.pathname==='/api/admin/ratings/'+track+'/viewers')send({viewers:[{login:'fixture_viewer',stars:vote},{login:'<img src=x onerror=alert(1)>',stars:5}],legacyCount:2});
 else if(url.pathname==='/api/admin/users')send({users:[]});
 else if(url.pathname==='/api/admin/audio')send({settings:{deviceId:'',autoStart:false},state:{status:'stopped'},canControl:true});
 else if(url.pathname==='/api/audio/devices')send({devices:[]});
 else if(url.pathname==='/api/admin/twitch')send({settings:{clientId:'fixtureclient123',revision:0},accounts:{},pending:{},canControl:true});
 else {res.statusCode=404;send({});}return true;
},async({navigate,evaluate,until,call,screenshot})=>{
 const click=id=>evaluate('document.getElementById('+JSON.stringify(id)+').click()');
 await navigate('/history?lang=en');await until(()=>evaluate('document.querySelector("[data-stars]")?.disabled && !document.getElementById("viewer-login").disabled'),'Viewer login missing');
 assert.equal(await evaluate('document.getElementById("history-ratings-link").hidden'),true);
 await click('viewer-login');await until(()=>evaluate('document.getElementById("viewer-code").textContent==="VIEW123"'),'Device code not shown');await screenshot('viewer-sign-in-en');
 await until(()=>evaluate('!document.querySelector("[data-stars]").disabled'),'Verified viewer cannot vote',10000);
 assert.equal(await evaluate('document.getElementById("history-ratings-link").hidden'),true,'Twitch viewer gained admin link');
 await evaluate('document.querySelector("[data-stars=\\"5\\"]").click()');await until(()=>vote===5,'Vote not sent');await until(()=>evaluate('!document.querySelector("[data-stars]").disabled'),'Vote busy');
 await evaluate('document.querySelector("[data-stars=\\"3\\"]").click()');await until(()=>vote===3,'Vote update not sent');assert.equal(requests,2);
 await navigate('/history?lang=de');await until(()=>evaluate('document.getElementById("viewer-status").textContent.includes("Angemeldet als fixture_viewer")'),'German viewer status missing');
 await call('Emulation.setDeviceMetricsOverride',{width:430,height:900,deviceScaleFactor:1,mobile:true});assert.ok(await evaluate('document.documentElement.scrollWidth<=innerWidth+1'),'History overflows mobile');
 await click('viewer-logout');await until(()=>evaluate('document.querySelector("[data-stars]").disabled'),'Logout retained voting');assert.equal(vote,3);
 admin=true;await navigate('/history?lang=en');await until(()=>evaluate('!document.getElementById("history-ratings-link").hidden'),'Admin ratings link missing');assert.equal(await evaluate('document.querySelector("[data-stars]").disabled'),true,'Admin bypassed Twitch login');
 await click('history-ratings-link');await until(()=>evaluate('document.querySelector("#rating-rows button")'),'Ratings not loaded');await evaluate('document.querySelector("#rating-rows button").click()');await until(()=>evaluate('document.querySelectorAll("#rating-viewers-list li").length===2'),'Viewer modal did not load');
 assert.equal(await evaluate('document.querySelector("#rating-viewers-list img")'),null,'Viewer name interpreted as HTML');assert.ok(await evaluate('document.getElementById("rating-viewers-message").textContent.includes("2 older anonymous")'));
 await screenshot('rating-viewers-en');await call('Input.dispatchKeyEvent',{type:'keyDown',key:'Escape',code:'Escape',windowsVirtualKeyCode:27});await call('Input.dispatchKeyEvent',{type:'keyUp',key:'Escape',code:'Escape',windowsVirtualKeyCode:27});await until(()=>evaluate('!document.getElementById("rating-viewers-dialog").open'),'Escape did not close modal');
 available=false;admin=false;await navigate('/history?lang=en');await until(()=>evaluate('document.getElementById("viewer-status").textContent.includes("not available")'),'Unconfigured state missing');
 console.log('Viewer UI passed: Twitch code/login/logout, vote/update, admin-only ratings link, safe modal, legacy votes, EN/DE and mobile.');
}).catch(e=>{console.error(e);process.exitCode=1;});
