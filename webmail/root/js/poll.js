// Checking for changes in the background.
//
// const poller = createPoller(ms, check)
//   calls check() (which returns a Promise) every ms milliseconds while the
//   page is visible; never two at a time.  While the page is hidden nothing
//   runs; when it becomes visible again, or the window gains focus, a check
//   runs at once (at most one every MIN_GAP ms for those).
// poller.now()     a check at once (unless one is running)
// poller.stop()    stops for good and removes the listeners

const MIN_GAP = 2000;

export function createPoller(ms, check) {
	let timer = 0;
	let running = false;
	let stopped = false;
	let last = 0;

	function schedule() {
		clearTimeout(timer);
		timer = 0;
		if (!stopped && !document.hidden)
			timer = setTimeout(now, ms);
	}

	async function now() {
		clearTimeout(timer);
		timer = 0;
		if (stopped || running || document.hidden)
			return;
		running = true;
		try {
			await check();
		} catch (e) {
			// the error was shown where it applies; keep checking
		} finally {
			running = false;
			last = Date.now();
			schedule();
		}
	}

	function wake() {
		if (document.hidden) {
			clearTimeout(timer);
			timer = 0;
			return;
		}
		if (Date.now() - last >= MIN_GAP)
			now();
		else if (!timer && !running)
			schedule();
	}

	document.addEventListener("visibilitychange", wake);
	window.addEventListener("focus", wake);
	schedule();

	return {
		now,
		stop() {
			stopped = true;
			clearTimeout(timer);
			timer = 0;
			document.removeEventListener("visibilitychange", wake);
			window.removeEventListener("focus", wake);
		}
	};
}
