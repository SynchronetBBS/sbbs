// Shows the unsupported-browser block at once when the app did not start.
//
// The shell page loads this classic script with "defer" right after the
// js/app.js module.  Deferred and module scripts run in document order, so
// this runs after app.js has run, or failed to load, parse or start (no
// module support, a syntax this browser cannot read, a failed download).
// app.js removes the block as its first act; if the block is still here,
// the app did not start.  Until then style.css keeps the block invisible,
// so it never flashes while the modules load.  Written for any browser
// (no modern syntax).
(function () {
	var b = document.getElementById("wm-unsupported");
	if (b)
		b.className += " wm-failed";
})();
