const assert=require('node:assert/strict');
const {withBrowser}=require('./browser_fixture.cjs');
let current={entryId:2,trackId:42,id:1,loaded:true,title:'Night Drive',artist:'Studio North',album:null,label:'Moon Records',key:null,originalBpm:126.6,bpm:129.7,coverUrl:null,positionMs:null,durationMs:null};
const previous={...current,entryId:1,trackId:41,title:'First Light',artist:'Orbit',label:'Dawn Records'};
const query=value=>new URLSearchParams({infoVersion:2,lang:'en',duration:0,width:500,...value}).toString();
withBrowser((req,res,url)=>{
 if(url.pathname==='/api/master'){res.setHeader('Content-Type','application/json');res.end(JSON.stringify({status:'connected',current,history:[previous]}));return true;}
 if(url.pathname==='/api/state'){res.setHeader('Content-Type','application/json');res.end(JSON.stringify({status:'connected',decks:[current]}));return true;}
 return false;
},async({navigate,evaluate,until,call,delay,screenshot})=>{
 await call('Emulation.setEmulatedMedia',{features:[{name:'prefers-reduced-motion',value:'no-preference'}]});
 await navigate('/master-overlay?'+query({fields:'title,artist,album,label,key,bpm,currentBpm,cover',historyFields:'title,label',bpmInteger:1,hideMissing:1,timeline:1,contentAlign:'center'}));
 await until(()=>evaluate('document.querySelectorAll(".track").length===2'),'Tracks missing');
 assert.deepEqual(await evaluate(`[...document.querySelectorAll('[data-current="true"] [data-field]')].filter(n=>!n.hidden).map(n=>n.dataset.field)`),['title','artist','label','currentBpm','bpm']);
 assert.deepEqual(await evaluate(`[...document.querySelectorAll('[data-current="false"] [data-field]')].filter(n=>!n.hidden).map(n=>n.dataset.field)`),['title','label']);
 assert.equal(await evaluate(`document.querySelector('[data-current="true"] [data-value=originalBpm]').textContent`),'127');
 assert.equal(await evaluate(`document.querySelector('[data-current="true"] [data-value=bpm]').textContent`),'130');
 assert.equal(await evaluate(`getComputedStyle(document.querySelector('.info')).textAlign`),'center');
 assert.equal(await evaluate(`document.querySelector('.timeline').hidden`),true);
 current={...current,title:null,trackId:43};await until(()=>evaluate(`document.querySelector('[data-current="true"] h1').textContent!=='Night Drive'`),'New unknown track inherited old title');
 assert.equal(await evaluate(`document.querySelector('[data-current="true"] h1').hidden`),false);
 current={...current,title:'Night Drive'};
 const tempos=selector=>evaluate(`[...document.querySelectorAll(${JSON.stringify(selector)})].map(card=>[card.querySelector('[data-value=bpm]').textContent,card.querySelector('[data-value=originalBpm]').textContent])`);
 for(const lang of ['en','de'])for(const bpmDecimals of [0,1,2]){
  const separator=lang==='de'?',':'.',expected=[['130','127'],['129'+separator+'7','126'+separator+'6'],['129'+separator+'70','126'+separator+'60']][bpmDecimals];
  for(const route of ['/overlay','/master-overlay']){
   await navigate(route+'?'+query({lang,bpmDecimals,fields:'title,bpm,currentBpm',history:1}));
   await until(()=>evaluate(`document.querySelectorAll('.track').length===${route==='/overlay'?1:2}`),'Precision overlay missing');
   assert.deepEqual(await tempos('.track'),route==='/overlay'?[expected]:[expected,expected],`${route} ${lang}: wrong BPM precision ${bpmDecimals}`);
  }
 }
 current={...current,bpm:129.76,originalBpm:126};
 await navigate('/overlay?'+query({bpmDecimals:1,fields:'title,bpm,currentBpm'}));
 await until(()=>evaluate('document.querySelector(".track")'),'Rounding overlay missing');
 assert.deepEqual(await tempos('.track'),[['129.8','126.0']],'One-decimal rounding or trailing zero missing');
 current={...current,bpm:129.995,originalBpm:126.244};
 await navigate('/overlay?'+query({bpmDecimals:2,fields:'title,bpm,currentBpm'}));
 await until(()=>evaluate('document.querySelector(".track")'),'Two-decimal overlay missing');
 assert.deepEqual(await tempos('.track'),[['130.00','126.24']],'Two-decimal rounding failed');
 for(const value of [null,0,-1,'128']){
  current={...current,bpm:value,originalBpm:value};
  await navigate('/overlay?'+query({bpmDecimals:2,fields:'title,bpm,currentBpm'}));
  await until(()=>evaluate('document.querySelector(".track")'),'Unknown BPM overlay missing');
  assert.deepEqual(await tempos('.track'),[['—','—']],'Unknown BPM became a number');
 }
 current={...current,bpm:129.7,originalBpm:126.6};
 for(const route of ['/overlay/settings','/master-overlay/settings']){
  await navigate(route+'?lang=en&bpmInteger=1');
  await until(()=>evaluate(`document.querySelector('select[data-track-option=bpmDecimals]')?.value==='0'`),'Legacy whole-number setting was not restored');
  assert.deepEqual(await evaluate(`[...document.querySelector('[data-track-option=bpmDecimals]').options].map(o=>[o.value,o.textContent])`),[['0','Whole number'],['1','1 decimal place'],['2','2 decimal places']]);
  for(const bpmDecimals of [0,1,2]){
   await evaluate(`{const input=document.querySelector('[data-track-option=bpmDecimals]');input.value='${bpmDecimals}';input.dispatchEvent(new Event('change'));}`);
   await until(()=>evaluate(`new URL(document.querySelector('#url').value).searchParams.get('bpmDecimals')==='${bpmDecimals}'`),'OBS link lost BPM precision');
   const expected=[['130','127'],['129.7','126.6'],['129.70','126.60']][bpmDecimals];
   await until(()=>evaluate(`{const card=document.querySelector('#preview').contentDocument?.querySelector('.track');JSON.stringify(card?[card.querySelector('[data-value=bpm]').textContent,card.querySelector('[data-value=originalBpm]').textContent]:[])===${JSON.stringify(JSON.stringify(expected))}}`),'Preview did not apply BPM precision');
  }
  await navigate(route+'?lang=de');
  await until(()=>evaluate(`document.querySelector('select[data-track-option=bpmDecimals]')?.value==='2'`),'Saved BPM precision was lost on reload');
  assert.deepEqual(await evaluate(`[...document.querySelector('[data-track-option=bpmDecimals]').options].map(o=>o.textContent)`),['Ganzzahl','1 Kommastelle','2 Kommastellen']);
 }
 await navigate('/overlay?'+query({deck:1,fields:'title,label,bpm',coverPosition:'right'}));
 await until(()=>evaluate('document.querySelector(".track")'),'Deck missing');
 assert.equal(await evaluate(`document.querySelector('[data-field=currentBpm]').hidden`),true);
 assert.equal(await evaluate(`document.querySelector('[data-field=bpm]').hidden`),false);
 // Existing one-switch URLs retain both BPM values.
 await navigate('/overlay?deck=1&fields=title,bpm&duration=0');await until(()=>evaluate('document.querySelector(".track")'),'Legacy deck missing');
 assert.equal(await evaluate(`document.querySelector('[data-field=currentBpm]').hidden`),false);
 const styles={title:{font:'impact',color:'#ff8844',background:'#123456',marginTop:7,marginBottom:11,fontSize:46,fontStyle:'italic',fontWeight:800},label:{font:'verdana'},bpm:{fontSize:28,fontWeight:300,fontStyle:'oblique'}};
 await navigate('/master-overlay?'+query({fields:'title,artist,label,cover',history:0,coverPosition:'right',coverShape:'round',coverSpin:1,coverFit:1,fieldStyles:JSON.stringify(styles)}));
 await until(()=>evaluate(`document.querySelector('.art')?.offsetHeight===document.querySelector('.info')?.offsetHeight`),'Cover did not fit text height');
 assert.equal(await evaluate(`document.querySelector('.art').getBoundingClientRect().left>document.querySelector('.info').getBoundingClientRect().left`),true);
 assert.deepEqual(await evaluate(`(()=>{const s=getComputedStyle(document.querySelector('h1'));return [s.color,s.backgroundColor,s.marginTop,s.marginBottom,s.fontFamily.includes('Impact')];})()`),['rgb(255, 136, 68)','rgb(18, 52, 86)','7px','11px',true]);
 assert.deepEqual(await evaluate(`(()=>{const s=getComputedStyle(document.querySelector('h1'));return [s.fontSize,s.fontStyle,s.fontWeight];})()`),['46px','italic','800']);
 assert.deepEqual(await evaluate(`(()=>{const s=getComputedStyle(document.querySelector('[data-field=bpm] strong'));return [s.fontSize,s.fontStyle,s.fontWeight];})()`),['28px','oblique','300']);
 assert.equal(await evaluate(`getComputedStyle(document.querySelector('.art')).borderRadius`),'50%');
 assert.equal(await evaluate(`document.querySelector('.art img').hidden && getComputedStyle(document.querySelector('.disc')).animationName==='cover-spin'`),true,'Fallback or spinning cover missing');
 await screenshot('track-design-cover-right');
 current={...current,title:'An exceptionally long track title — '+ 'Beyond the horizon '.repeat(8),artist:'Studio North and Friends '.repeat(8)};
 await navigate('/master-overlay?'+query({history:0,width:320,fields:'title,artist',overflow:'slide',contentAlign:'right'}));
 await until(()=>evaluate(`document.querySelector('h1')?.classList.contains('scrolling')`),'Long text did not scroll');
 assert.equal(await evaluate(`getComputedStyle(document.querySelector('h1')).textAlign`),'left','Scrolling must start with the beginning of the title even in a right-aligned design');
 await until(()=>evaluate(`document.querySelector('h1 span').getAnimations()[0]?.startTime!=null`),'Scroll animation did not start');
 const start=await evaluate(`window.titleAnimation=document.querySelector('h1 span').getAnimations()[0];window.titleNode=document.querySelector('h1 span');titleAnimation.startTime`);
 await delay(700);assert.equal(await evaluate(`window.titleNode===document.querySelector('h1 span') && titleAnimation===document.querySelector('h1 span').getAnimations()[0] && titleAnimation.startTime===${start}`),true,'Polling restarted scrolling');
 await navigate('/master-overlay?'+query({history:1,width:320,fields:'title,artist',overflow:'expand'}));
 await until(()=>evaluate(`document.querySelector('.card')?.getBoundingClientRect().width>900`),'Container did not expand for long text');
 assert.equal(await evaluate(`document.querySelector('h1').scrollWidth<=document.querySelector('h1').clientWidth+1`),true,'Expanded text remains clipped');
 await navigate('/master-overlay/settings?'+query({history:0,width:320,fields:'title,artist',overflow:'expand'}));
 await until(()=>evaluate(`document.querySelector('#preview')?.offsetWidth>900`),'Expanded settings preview remained clipped');
 assert.equal(await evaluate(`document.querySelector('#preview').offsetWidth>=document.querySelector('#preview').contentDocument.querySelector('.card').offsetWidth`),true);
 await call('Emulation.setEmulatedMedia',{features:[{name:'prefers-reduced-motion',value:'reduce'}]});
 await navigate('/overlay?'+query({deck:1,fields:'title,cover',overflow:'slide',coverShape:'round',coverSpin:1}));
 await until(()=>evaluate('document.querySelector(".track")'),'Reduced motion deck missing');
 assert.equal(await evaluate(`getComputedStyle(document.querySelector('.disc')).animationName`),'none');
 const normalized=await evaluate(`(async()=>{const m=await import('/master-options.js');const n=m.normalize({infoVersion:2,historyFields:[],fieldStyles:${JSON.stringify(styles)},coverPosition:'right',coverSpin:true});return [n,m.parseOptions('?'+m.optionQuery(n))];})()`);
 assert.deepEqual(normalized[0],normalized[1],'Option URL roundtrip changed the design');
 const precision=await evaluate(`(async()=>{const m=await import('/master-options.js');return {
  legacy:[{},...['1',1,true,'true'].map(bpmInteger=>({bpmInteger})),{bpmInteger:false},{bpmInteger:'0'}].map(o=>m.normalize(o).bpmDecimals),
  explicit:[0,1,2].map(bpmDecimals=>{const o=m.normalize({bpmDecimals,bpmInteger:true});return [o.bpmDecimals,m.parseOptions('?'+m.optionQuery(o)).bpmDecimals,'bpmInteger' in o];}),
  invalid:[null,-1,3,1.5,'invalid'].map(bpmDecimals=>m.normalize({bpmDecimals}).bpmDecimals)
 };})()`);
 assert.deepEqual(precision.legacy,[2,0,0,0,0,2,2],'Legacy BPM options changed precision');
 assert.deepEqual(precision.explicit,[[0,0,false],[1,1,false],[2,2,false]],'Explicit precision must override legacy flags and round-trip without them');
 assert.deepEqual(precision.invalid,[2,2,2,2,2],'Invalid precision did not normalize safely');
 console.log('Track design passed: 0/1/2 BPM decimal places, rounding/trailing zeros, EN/DE selects/previews/persistence, legacy BPM options, label, missing values, independent history fields, cover fallback/fit/right/round/rotation, field styles, scrolling continuity, expansion and reduced motion.');
}).catch(error=>{console.error(error);process.exitCode=1;});
