const assert=require('node:assert/strict'),{withBrowser}=require('./browser_fixture.cjs');
const fresh=()=>({id:1,trackId:51,entryId:1,sourceSession:'first-session',loaded:true,statusAvailable:true,metadataAvailable:true,
 title:'Night Drive',artist:'Studio North',album:'After Hours',bpm:128,originalBpm:126,positionMs:5000,durationMs:240000,playing:true,isMaster:true});
let track=fresh(),status='connected',available=true,httpStatus=200,coverCalls=0;
withBrowser((req,res,url)=>{
 if(['/api/state','/api/master'].includes(url.pathname)){
  res.statusCode=httpStatus;res.setHeader('Content-Type','application/json');
  const master=url.pathname==='/api/master';
  const item=track&&{...track,coverUrl:master?'/api/master/covers/'+track.trackId:'/api/decks/1/cover?trackId='+track.trackId};
  res.end(JSON.stringify(master?{status,masterAvailable:available,current:item,history:[]}:{status,decks:item?[item]:[]}));return true;
 }
 if(url.pathname.includes('/cover')){
  ++coverCalls;res.setHeader('Content-Type','image/svg+xml');res.end('<svg xmlns="http://www.w3.org/2000/svg" width="32" height="32"><rect width="32" height="32" fill="#44aa88"/></svg>');return true;
 }
 return false;
},async({navigate,evaluate,until})=>{
 for(const route of ['/overlay','/master-overlay']){
  track=fresh();status='connected';available=true;httpStatus=200;
  await navigate(route+'?lang=en&duration=0&history=0&badges=0&timeline=1&infoVersion=2&fields=title,artist,bpm,currentBpm,cover');
  await until(()=>evaluate('document.querySelector(".track img")?.complete && !document.querySelector(".track img").hidden && !document.querySelector(".timeline").hidden'),'Initial track/artwork/timeline missing');
  await evaluate('window.savedCard=document.querySelector(".track");window.savedImage=savedCard.querySelector("img")');
  const covers=coverCalls;
  track={...track,title:null,artist:'',originalBpm:null,bpm:null};
  await until(()=>evaluate('document.querySelector("[data-value=bpm]").textContent==="—"'),'Missing live BPM was cached as current');
  assert.deepEqual(await evaluate('[document.querySelector("h1").textContent,document.querySelector(".artist").textContent,document.querySelector("[data-value=originalBpm]").textContent]'),['Night Drive','Studio North','126.00']);
  httpStatus=503;
  await until(()=>evaluate('savedCard.dataset.cached==="true"'),'HTTP outage was not cached');
  assert.equal(await evaluate('!savedCard.querySelector(".tag").hidden && !savedCard.querySelector("[data-role=label]").hidden && savedCard.querySelector(".timeline").hidden'),true,'Cached data looks live or advances a timeline');
  assert.match(await evaluate('savedCard.querySelector("[data-role=label]").textContent'),/cached/i);
  assert.equal(await evaluate('savedCard===document.querySelector(".track") && savedImage===savedCard.querySelector("img") && savedImage.complete && !savedImage.hidden'),true,'Outage rebuilt card or cleared artwork');
  assert.equal(coverCalls,covers,'Outage re-requested loaded artwork');
  assert.equal(await evaluate('document.getAnimations().length'),0,'Outage restarted animation');
  httpStatus=200;track=fresh();
  await until(()=>evaluate('savedCard.dataset.cached==="false"&&!savedCard.querySelector(".timeline").hidden'),'Recovery failed');
  assert.equal(await evaluate('savedCard===document.querySelector(".track")'),true,'Recovery rebuilt card');
  track={...fresh(),sourceSession:'second-session',title:null,artist:null,originalBpm:null};
  await until(()=>evaluate('document.querySelector(".track")!==savedCard'),route+': server restart reused the old card/artwork');
  assert.equal(await evaluate('document.querySelector("h1").textContent.includes("Night Drive") || document.querySelector("[data-value=originalBpm]").textContent!=="—"'),false,'Server restart inherited metadata through a reused numeric ID');
  track=fresh();
  await until(()=>evaluate('document.querySelector("h1").textContent==="Night Drive"'),'Initial source did not return');
  await evaluate('window.savedCard=document.querySelector(".track")');
  available=false;track.statusAvailable=false;
  await until(()=>evaluate('savedCard.dataset.cached==="true"'),'Missing individual player sample was treated as unloaded');
  available=true;track=fresh();
  await until(()=>evaluate('savedCard.dataset.cached==="false"'),'Player sample did not recover');
  status='stale';await until(()=>evaluate('savedCard.dataset.cached==="true"'),'Stale source was not cached');
  await until(()=>evaluate('document.querySelector(".track")===null'),'Cache did not expire after ten seconds',12000);
  status='connected';track={...fresh(),trackId:52,entryId:2,title:null,artist:null,originalBpm:null,bpm:null};
  await until(()=>evaluate('document.querySelector(".track")?.dataset.trackId==="52"'),'New track missing');
  assert.equal(await evaluate('document.querySelector("h1").textContent.includes("Night Drive") || document.querySelector("[data-value=originalBpm]").textContent!=="—"'),false,'Unknown/new track inherited cached metadata');
  track=route==='/overlay'?{id:1,loaded:false,statusAvailable:true}:null;
  await until(()=>evaluate('!document.querySelector(".track")'),'Confirmed unload/end did not clear immediately');
  track=fresh();await until(()=>evaluate('document.querySelector(".track")'),'Track did not reappear');
  httpStatus=403;await until(()=>evaluate('!document.querySelector(".track")'),'Revoked access retained cached data');
  httpStatus=200;await until(()=>evaluate('document.querySelector(".track")'),'Access recovery failed');
  httpStatus=401;await until(()=>evaluate('!document.querySelector(".track")'),'Expired session retained cached data');
 }
 const result=await evaluate(`(async()=>{
  const {TrackCache}=await import('/overlay-shared.js'),c=new TrackCache(),old={trackId:1,title:'Known',originalBpm:126,bpm:128};
  c.read(old,false,100);
  const boundary=c.read(null,true,10100),expired=c.read(null,true,10101);
  const afterExpiry=c.read({trackId:1,title:null,originalBpm:null,bpm:null},false,10102);
  c.read(old,false,20000);
  const unknown=c.read({trackId:2,title:null,originalBpm:null,bpm:null},false,20001);
  c.read(null,false,20002);
  return {boundary,expired,afterExpiry,unknown,cleared:c.read(null,true,20003)};
 })()`);
 assert.deepEqual(result.boundary,{trackId:1,title:'Known',originalBpm:126,bpm:128,dataCached:true,playing:null,isMaster:null});
 assert.equal(result.expired,null);assert.equal(result.cleared,null);
 assert.equal(result.afterExpiry.title,null);assert.equal(result.afterExpiry.originalBpm,null);
 assert.equal(result.unknown.title,null);assert.equal(result.unknown.bpm,null);
 console.log('Track cache passed: exact ten-second bound, static metadata only, identity/session separation, preserved DOM/artwork, visible stale label, frozen timeline, recovery, explicit stop and access revocation.');
}).catch(error=>{console.error(error);process.exitCode=1;});
