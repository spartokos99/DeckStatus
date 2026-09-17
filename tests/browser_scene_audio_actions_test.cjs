// Exercise runtime scene updates with synthetic samples, without controlling capture.
const assert=require('node:assert/strict');
const {withBrowser}=require('./browser_fixture.cjs');
let enabled=false,sequence=0,mutations=0;
const scene={id:'fixture',revision:1,name:'Audio actions',width:1920,height:1080,background:'transparent',items:['deck','master','waveform','text','image','fx'].map((type,index)=>({id:type,type,visible:true,x:(index%3)*500,y:Math.floor(index/3)*300,width:400,height:240,opacity:1,options:{text:'Audio reactive',history:0,effect:'flash',audioEnabled:false,audioGain:2,audioThreshold:0,audioAttack:0,audioRelease:0,reactScale:.5,reactRotation:20}}))};
withBrowser((req,res,url)=>{
 if(!url.pathname.startsWith('/api/'))return false;
 if(req.method!=='GET')mutations++;
 res.setHeader('Content-Type','application/json');
 if(url.pathname==='/api/scene')res.end(JSON.stringify({...scene,items:scene.items.map(item=>({...item,options:{...item.options,audioEnabled:enabled}}))}));
 else if(url.pathname==='/api/audio/state'){const samples=Array.from({length:1024},(_,i)=>Math.sin(i*.2));res.end(JSON.stringify({status:'capturing',fresh:true,sampleAgeMs:0,sequence:++sequence,sampleRate:48000,left:samples,right:samples}));}
 else if(url.pathname==='/api/master')res.end(JSON.stringify({current:null,history:[]}));
 else if(url.pathname==='/api/state')res.end(JSON.stringify({decks:[]}));
 else{res.statusCode=404;res.end('{}');}return true;
},async({navigate,evaluate,until})=>{
 await navigate('/scene?scene=fixture');await until(()=>evaluate('document.querySelectorAll(".scene-item").length===6'),'Components did not render');
 const scaled=scale=>evaluate(`[...document.querySelectorAll('.scene-item')].every(node=>node.style.transform.includes('scale(${scale})'))`);
 await until(()=>scaled(1),'Saved non-reactive state not rendered');
 enabled=true;await until(()=>scaled(1.5),'Runtime enable did not animate all component types');
 enabled=false;await until(()=>scaled(1),'Runtime disable did not restore static transforms');
 enabled=true;await until(()=>scaled(1.5),'Restored enabled state did not reactivate audio');
 assert.equal(mutations,0,'Rendering an audio action controlled capture');
 console.log('Runtime audio enable/disable/restoration passed for all six scene component types, with synthetic samples and no capture mutations.');
}).catch(error=>{console.error(error);process.exitCode=1;});
