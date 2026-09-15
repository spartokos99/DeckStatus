// Preserve saved designs and language when upgrading from the earlier branding.
export function readSetting(key) {
  try {
    const current = localStorage.getItem(key);
    if (current !== null || !key.startsWith('deckstatus.')) return current;
    const legacy = localStorage.getItem('rb.' + key.slice('deckstatus.'.length));
    if (legacy !== null) localStorage.setItem(key, legacy);
    return legacy;
  } catch (_) { return null; }
}
export function writeSetting(key, value) {
  try { localStorage.setItem(key, value); } catch (_) {}
}
