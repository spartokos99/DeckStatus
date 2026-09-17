import {appReady} from './navigation.js';
import {t,translate,getLanguage} from './i18n.js';
import {loadBroadcastKeys,broadcastUrl,obsUrl} from './broadcast.js';
import {componentLabel,setupPresets} from './component-presets.js';
import {creativeTypes,defaults,normalize,controls,reactionControls,presets,componentPath,buildControls} from './creative-options.js';
import {setupMedia} from './media-library.js';
const app=await appReady,type=location.pathname.split('/').pop(),$=id=>document.getElementById(id),keys=await loadBroadcastKeys();
if(creativeTypes.includes(type)){
 let options=defaults(type),media=[];try{options=normalize(type,JSON.parse(localStorage.getItem('deckstatus.creative.'+type)||'{}'));}catch{}
 const style=$('creative-style');style.add(new Option(t('custom'),''));for(const name of Object.keys(presets[type]))style.add(new Option(name,name));
 function preview(){const box=$('creative-preview-box'),frame=$('creative-preview'),path=componentPath(type,{...options,lang:getLanguage()}),scale=box.clientWidth/options.width;box.style.height=Math.min(700,options.height*scale)+'px';frame.style.width=options.width+'px';frame.style.height=options.height+'px';frame.style.transform='scale('+scale+')';if(frame.getAttribute('src')!==path)frame.src=path;$('creative-url').value=obsUrl(broadcastUrl(path,keys[type]),app);$('creative-open').href=$('creative-url').value;try{localStorage.setItem('deckstatus.creative.'+type,JSON.stringify(options));}catch{}}
 function change(name,value){options=normalize(type,{...options,[name]:value});preview();}
 function form(){buildControls($('creative-controls'),controls[type],options,change,t,media);buildControls($('creative-reaction'),reactionControls,options,change,t);$('creative-heading').textContent=componentLabel(type);document.title=componentLabel(type)+' · DeckStatus';translate();preview();}
 style.addEventListener('change',()=>{const preset=presets[type][style.value];if(preset){options=normalize(type,{...options,...preset});form();}});
 $('fx-hint').hidden=type!=='fx';$('creative-media').hidden=type!=='image';
 if(type==='image')setupMedia($('creative-media'),asset=>{options.assetId=asset.id;form();},records=>{media=records;form();});
 setupPresets({type,read:()=>({...options,lang:getLanguage()}),apply:value=>{options=normalize(type,value);form();}});
 $('creative-copy').addEventListener('click',async()=>{try{await navigator.clipboard.writeText($('creative-url').value);$('creative-message').textContent=t('networkCopied');}catch{$('creative-url').select();$('creative-message').textContent=t('copyFallback');}});
 window.addEventListener('languagechange',form);new ResizeObserver(preview).observe($('creative-preview-box'));form();
}

document.querySelectorAll('a[data-i18n="creativeAudioSetup"]').forEach(link=>link.hidden=!app?.capabilities?.admin);
