import {api} from './auth.js';
import {t,translate,locale} from './i18n.js';

export const componentLabel = type => t({deck:'deckSettings',master:'masterSettings',waveform:'waveNav',text:'creativeText',image:'creativeImage',fx:'creativeFx'}[type]);
export async function listPresets() {
  const result = await api('/api/presets');
  if (!Array.isArray(result.presets)) throw Error(t('presetUnavailable'));
  return result.presets.sort((a,b) => a.name.localeCompare(b.name,locale()));
}

// Each settings page uses the same server library, with its own component type.
export function setupPresets({type,read,apply}) {
  const root = document.getElementById('component-presets');
  if (!root) return;
  root.classList.add('component-presets');
  root.innerHTML = '<h2 data-i18n="presetLibrary"></h2><p class="preset-help" data-i18n="presetLibraryHelp"></p><label for="saved-preset" data-i18n="presetChoose"></label><div class="preset-row"><select id="saved-preset"></select><button id="preset-load" type="button" data-i18n="presetLoad"></button></div><label for="preset-name" data-i18n="presetName"></label><input id="preset-name" type="text" maxlength="80" autocomplete="off"><div class="preset-actions"><button id="preset-create" type="button" data-i18n="presetCreate"></button><button id="preset-update" type="button" data-i18n="presetUpdate"></button><button id="preset-delete" type="button" data-i18n="portalDelete"></button><button id="presets-refresh" type="button" data-i18n="sceneReload"></button></div><p id="preset-message" role="status" aria-live="polite"></p>';
  const $ = id => root.querySelector('#'+id);
  let presets = [], busy = false, message = '', error = '';
  const selected = () => presets.find(p => p.id === $('saved-preset').value);
  function state() {
    for (const input of root.querySelectorAll('input,select,button')) input.disabled = busy;
    for (const name of ['load','update','delete']) $('preset-'+name).disabled = busy || !selected();
    for (const name of ['create','update']) $('preset-'+name).disabled ||= !$('preset-name').value.trim();
    $('preset-message').textContent = error || (message ? t(message) : '');
    root.setAttribute('aria-busy',String(busy));
  }
  function list(id = $('saved-preset').value) {
    $('saved-preset').replaceChildren(new Option(t(presets.length?'presetChoose':'presetEmpty'),''));
    for (const p of presets) $('saved-preset').add(new Option(p.name,p.id));
    $('saved-preset').value = presets.some(p => p.id === id) ? id : '';
    state();
  }
  async function run(action) {
    if (busy) return;
    busy = true; error = ''; message = ''; state();
    try { await action(); } catch (e) { error = e.message; }
    finally { busy = false; state(); }
  }
  const reload = async () => { presets = (await listPresets()).filter(p => p.type === type); list(); };
  $('saved-preset').addEventListener('change',() => { $('preset-name').value = selected()?.name || ''; message = ''; error = ''; state(); });
  $('preset-name').addEventListener('input',state);
  $('preset-load').addEventListener('click',() => {
    const preset = selected(); if (!preset || busy) return;
    apply(JSON.parse(JSON.stringify(preset.options)));
    message = 'presetLoaded'; error = ''; state();
  });
  for (const name of ['create','update']) $('preset-'+name).addEventListener('click',() => run(async () => {
    const previous = name === 'update' ? selected() : null;
    if (name === 'update' && !previous) return;
    const preset = await api('/api/presets',{action:'save',id:previous?.id||'',revision:previous?.revision||0,preset:{name:$('preset-name').value.trim(),type,options:read()}});
    presets = presets.filter(p => p.id !== preset.id); presets.push(preset);
    presets.sort((a,b) => a.name.localeCompare(b.name,locale()));
    list(preset.id); $('preset-name').value = preset.name; message = 'presetSaved';
  }));
  $('preset-delete').addEventListener('click',() => {
    const preset = selected(); if (!preset || busy || !confirm(t('presetDeleteConfirm',{name:preset.name}))) return;
    run(async () => { await api('/api/presets',{action:'delete',id:preset.id,revision:preset.revision}); presets = presets.filter(p => p.id !== preset.id); list(''); $('preset-name').value = ''; message = 'presetDeleted'; });
  });
  $('presets-refresh').addEventListener('click',() => run(reload));
  window.addEventListener('languagechange',() => { translate(root); list(); });
  translate(root); run(reload);
}
