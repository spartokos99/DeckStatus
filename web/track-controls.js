import {fieldNames, styledFields, fonts} from './master-options.js';
import {t} from './i18n.js';

export function fontOptions(select, inherit = false) {
  select.replaceChildren();
  if (inherit) {const option=new Option(t('trackInherit'), '');option.dataset.i18n='trackInherit';select.add(option);}
  for (const key of Object.keys(fonts)) {const option=new Option(['system','serif','mono'].includes(key) ? t(key) : {arial:'Arial',calibri:'Calibri',tahoma:'Tahoma',verdana:'Verdana',trebuchet:'Trebuchet MS',impact:'Impact',palatino:'Palatino Linotype'}[key], key);if(['system','serif','mono'].includes(key))option.dataset.i18n=key;select.add(option);}
}

// Shared by the component settings and independent layers in the scene editor.
export function trackExtras(root, {read, write, master}) {
  root.classList.add('track-extras');
  const inputs = new Map(), historyInputs = [], styleInputs = new Map();
  const section = key => { const details=document.createElement('details'),summary=document.createElement('summary');summary.dataset.i18n=key;summary.textContent=t(key);details.append(summary);root.append(details);return details; };
  const row = (parent, key, type, values) => {
    const label=document.createElement('label'),span=document.createElement('span'),input=document.createElement(type==='select'?'select':'input');
    span.dataset.i18n=key;span.textContent=t(key);label.className='track-control';
    if(type==='select')for(const value of values){const option=new Option(t(value),value);option.dataset.i18n=value;input.add(option);}
    else {input.type=type;if(type==='number'){input.min=0;input.max=64;input.step=1;}}
    input.dataset.trackOption=key;label.append(span,input);parent.append(label);return input;
  };
  const info=section('trackInfoOptions');
  for(const name of ['bpmInteger','hideMissing'])inputs.set(name,row(info,name,'checkbox'));
  if(master){
    const same=row(info,'historySameFields','checkbox');inputs.set('historySameFields',same);
    const group=document.createElement('div');group.className='fields';info.append(group);
    for(const name of fieldNames){const label=document.createElement('label'),input=document.createElement('input');input.type='checkbox';input.value=name;input.dataset.historyField=name;const span=document.createElement('span');span.dataset.i18n=name;span.textContent=t(name);label.append(input,span);group.append(label);historyInputs.push(input);input.addEventListener('change',()=>{write({...read(),historyFields:historyInputs.filter(node=>node.checked).map(node=>node.value)});refresh();});}
  }
  const cover=section('trackCoverOptions');
  inputs.set('coverPosition',row(cover,'coverPosition','select',['left','top','right']));
  inputs.set('coverShape',row(cover,'coverShape','select',['square','round']));
  for(const name of ['coverSpin','coverFit'])inputs.set(name,row(cover,name,'checkbox'));
  const layout=section('trackLayoutOptions');
  inputs.set('contentAlign',row(layout,'contentAlign','select',['left','center','right']));
  inputs.set('overflow',row(layout,'overflow','select',['ellipsis','slide','expand']));
  inputs.set('elementGap',row(layout,'elementGap','number'));inputs.get('elementGap').max=40;
  const help=document.createElement('p');help.className='hint';help.dataset.i18n='trackOverflowHelp';help.textContent=t('trackOverflowHelp');layout.append(help);
  const styles=section('trackElementStyle'),field=row(styles,'trackElement','select',styledFields);
  field.dataset.trackStyleField='';
  for(const name of ['color','background']){
    const toggle=row(styles,name==='color'?'trackCustomColor':'trackCustomBackground','checkbox');
    const input=row(styles,name==='color'?'textColor':'background','color');styleInputs.set(name,{toggle,input});
    const change=()=>{const next=structuredClone(read().fieldStyles),style=next[field.value]||{};if(toggle.checked)style[name]=input.value;else delete style[name];next[field.value]=style;write({...read(),fieldStyles:next});refreshStyle();};
    toggle.addEventListener('change',change);input.addEventListener('input',change);
  }
  const font=row(styles,'font','select',[]);fontOptions(font,true);styleInputs.set('font',font);
  const size=row(styles,'fontSize','number');size.min=8;size.max=200;size.placeholder=t('trackInherit');styleInputs.set('fontSize',size);
  for(const [name,values] of [['fontStyle',['normal','italic','oblique']],['fontWeight',[100,200,300,400,500,600,700,800,900]]]) {
    const input=row(styles,name,'select',[]),inherit=new Option(t('trackInherit'),'');inherit.dataset.i18n='trackInherit';input.add(inherit);
    for(const value of values){const key=name==='fontWeight'?'trackWeight'+value:'trackStyle_'+value,option=new Option(t(key),value);option.dataset.i18n=key;input.add(option);}
    styleInputs.set(name,input);
  }
  for(const name of ['marginTop','marginBottom'])styleInputs.set(name,row(styles,name,'number'));
  for(const name of ['font','fontSize','fontStyle','fontWeight','marginTop','marginBottom'])styleInputs.get(name).addEventListener('input',()=>{
    const input=styleInputs.get(name);if(!input.checkValidity())return;
    const next=structuredClone(read().fieldStyles),style=next[field.value]||{};if(input.value==='')delete style[name];else style[name]=['font','fontStyle'].includes(name)?input.value:Number(input.value);next[field.value]=style;write({...read(),fieldStyles:next});
  });
  const reset=document.createElement('button');reset.type='button';reset.dataset.i18n='trackResetElement';reset.textContent=t('trackResetElement');styles.append(reset);
  reset.addEventListener('click',()=>{const next=structuredClone(read().fieldStyles);delete next[field.value];write({...read(),fieldStyles:next});refreshStyle();});
  field.addEventListener('change',refreshStyle);
  function refreshStyle(){const options=read(),style=options.fieldStyles[field.value]||{};
    for(const name of ['color','background']){const {toggle,input}=styleInputs.get(name);toggle.checked=!!style[name];input.disabled=!toggle.checked;input.dataset.optionDisabled=String(input.disabled);input.value=style[name]||(name==='color'?options.textColor:options.background);}
    font.value=style.font||'';for(const name of ['fontSize','fontStyle','fontWeight'])styleInputs.get(name).value=style[name]??'';for(const name of ['marginTop','marginBottom'])styleInputs.get(name).value=style[name]??0;
  }
  for(const [name,input] of inputs)input.addEventListener('change',()=>{
    if(!input.checkValidity())return;
    write({...read(),...(name==='historySameFields'?{historyFields:input.checked?null:[...read().fields]}:{[name]:input.type==='checkbox'?input.checked:input.type==='number'?Number(input.value):input.value})});refresh();
  });
  function refresh(){const options=read();for(const [name,input] of inputs){if(input.type==='checkbox')input.checked=name==='historySameFields'?options.historyFields===null:options[name];else input.value=options[name];}
    for(const input of historyInputs){input.checked=(options.historyFields??options.fields).includes(input.value);input.disabled=options.historyFields===null;input.dataset.optionDisabled=String(input.disabled);}
    for(const [name,disabled] of [['coverSpin',options.coverShape!=='round'],['coverFit',options.coverPosition==='top']]){inputs.get(name).disabled=disabled;inputs.get(name).dataset.optionDisabled=String(disabled);}refreshStyle();
  }
  refresh();return refresh;
}
