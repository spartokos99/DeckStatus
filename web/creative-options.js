export const creativeTypes=['text','image','fx'];
export const reactionDefaults={audioEnabled:false,audioBand:'rms',audioGain:2,audioThreshold:.2,audioAttack:50,audioRelease:350,reactScale:0,reactX:0,reactY:0,reactRotation:0,reactOpacity:0};
export const reactionControls=[['audioEnabled','checkbox'],['audioBand','select',['rms','peak','bass','mid','high']],['audioGain','number',.1,20,.1],['audioThreshold','number',0,1,.01],['audioAttack','number',0,1000,10],['audioRelease','number',0,3000,10],['reactScale','number',-.9,3,.05],['reactX','number',-2000,2000,5],['reactY','number',-2000,2000,5],['reactRotation','number',-360,360,1],['reactOpacity','number',-1,1,.05]];
const common=[['width','number',32,7680,1],['height','number',32,4320,1]];
export const controls={
 text:[...common,['text','textarea'],['font','select',['system','serif','mono']],['fontSize','number',10,300,1],['bold','checkbox'],['italic','checkbox'],['align','select',['left','center','right']],['verticalAlign','select',['top','center','bottom']],['color','color'],['background','color'],['backgroundOpacity','number',0,100,1],['padding','number',0,200,1],['radius','number',0,200,1]],
 image:[...common,['assetId','asset'],['fit','select',['contain','cover','fill']],['radius','number',0,200,1]],
 fx:[...common,['effect','select',['flash','fog','both']],['color','color'],['color2','color'],['intensity','number',0,1,.05],['density','number',4,100,1],['speed','number',0,3,.05],['flashDuration','number',30,1000,10],['cooldown','number',100,3000,10]]
};
export function defaults(type){return {...reactionDefaults,width:type==='fx'?1920:640,height:type==='fx'?1080:240,lang:'en',...(type==='text'?{text:'Your text',font:'system',fontSize:64,bold:true,italic:false,align:'center',verticalAlign:'center',color:'#ffffff',background:'#10231c',backgroundOpacity:0,padding:20,radius:0}:type==='image'?{assetId:'',fit:'contain',radius:0}:{effect:'fog',color:'#9bf6d3',color2:'#638cff',intensity:.55,density:36,speed:.4,flashDuration:120,cooldown:400,audioEnabled:true})};}
const clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
export function boundedText(value,limit){const encoder=new TextEncoder();let result='',bytes=0;for(const char of String(value).toWellFormed()){bytes+=encoder.encode(char).length;if(bytes>limit)break;result+=char;}return result;}
function normalizeControls(raw,defs,list){const out={...defs};for(const [name,type,a,b] of list){const v=raw?.[name];if(v===undefined)continue;if(type==='number'){const n=Number(v);if(Number.isFinite(n))out[name]=clamp(n,a,b);}else if(type==='checkbox')out[name]=v===true||v==='true'||v==='1';else if(type==='select'){if(a.includes(v))out[name]=v;}else if(type==='color'){if(/^#[a-f0-9]{6}$/i.test(v))out[name]=v;}else if(type==='textarea'){// Bound UTF-8 bytes, as the server does.
 out[name]=boundedText(v,2048);
 }else if(type==='asset')out[name]=/^[a-f0-9]{64}$/.test(v)?v:'';}return out;}
export function reactionOptions(raw={}){return normalizeControls(raw,reactionDefaults,reactionControls);}
export function normalize(type,raw={}){return {...normalizeControls(raw,defaults(type),[...controls[type],...reactionControls]),lang:raw.lang==='de'?'de':'en'};}
export const presets={text:{Headline:{fontSize:96,bold:true,backgroundOpacity:0},Caption:{fontSize:36,backgroundOpacity:80,padding:20},Pulse:{audioEnabled:true,reactScale:.2,audioBand:'bass'}},image:{Logo:{fit:'contain'},Background:{width:1920,height:1080,fit:'cover'},Bounce:{audioEnabled:true,reactY:-60,reactRotation:8,audioBand:'bass'}},fx:{'Soft fog':{effect:'fog',intensity:.4,speed:.3,density:32},'Bass flash':{effect:'flash',audioBand:'bass',intensity:.5,flashDuration:100,cooldown:500},'Neon storm':{effect:'both',color:'#be77ff',color2:'#50ffe1',intensity:.6,density:60,cooldown:400}}};
export function componentPath(type,options){return '/component/'+type+'?'+new URLSearchParams(normalize(type,options));}

// Reused by standalone settings and scene properties; no user-supplied markup.
export function buildControls(container,list,options,onChange,t,media=[]){container.replaceChildren();for(const [name,type,a,b,step] of list){const label=document.createElement('label'),input=document.createElement(type==='select'||type==='asset'?'select':type==='textarea'?'textarea':'input');input.id=container.id+'-'+name;input.dataset.option=name;label.htmlFor=input.id;label.textContent=t('creative_'+name);
 if(type==='select')for(const value of a)input.add(new Option(t('creativeValue_'+value),value));
 else if(type==='asset'){input.add(new Option(t('mediaChoose'),''));for(const asset of media)input.add(new Option(asset.name,asset.id));}
 else if(type==='textarea'){input.rows=4;input.maxLength=2048;}
 else{input.type=type;if(type==='number'){input.min=a;input.max=b;input.step=step;}}
 if(type==='checkbox')input.checked=options[name];else input.value=options[name]??'';
 input.addEventListener('input',()=>{if(!input.checkValidity())return;onChange(name,type==='checkbox'?input.checked:type==='number'?Number(input.value):input.value);});container.append(label,input);
 }}
