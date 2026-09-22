// Shared polling helper for the application pages.
//
// A page that refreshes a status line does not need to keep asking while nobody is
// looking at it, and a server that is briefly unreachable should not be hammered at
// the same rate. Every job here pauses while the tab is hidden, resumes immediately
// when it comes back, backs off after consecutive failures and never overlaps runs.
// Renderer documents deliberately do not use this: an OBS browser source is never
// "hidden" in a useful sense and its cadence is part of the on-air latency.
export function poll(task, options = {}) {
  const { interval = 1000, hiddenInterval = 0, timeout = 2500, maxDelay = 5000, immediate = true } = options;
  let timer, running = false, stopped = false, failures = 0;

  const delay = () => failures
    ? Math.min(maxDelay, interval * 2 ** Math.min(failures, 4))
    : document.hidden ? hiddenInterval : interval;

  const schedule = wait => { clearTimeout(timer); if (!stopped) timer = setTimeout(run, wait); };

  async function run() {
    if (stopped || running) return;
    // Paused: visibilitychange restarts the job instead of a wake-up timer.
    if (document.hidden && !hiddenInterval) return;
    running = true;
    const controller = new AbortController();
    const expiry = timeout ? setTimeout(() => controller.abort(), timeout) : 0;
    try { await task(controller.signal); failures = 0; }
    catch (_) { failures = Math.min(failures + 1, 6); }
    finally { clearTimeout(expiry); running = false; schedule(delay()); }
  }

  const onVisibility = () => { if (!document.hidden) schedule(0); };
  const stop = () => { stopped = true; clearTimeout(timer); document.removeEventListener('visibilitychange', onVisibility); };
  document.addEventListener('visibilitychange', onVisibility);
  window.addEventListener('pagehide', stop, { once: true });

  schedule(immediate ? 0 : interval);
  return { stop, refresh: () => { failures = 0; schedule(0); } };
}
