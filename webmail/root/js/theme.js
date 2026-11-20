// The theme this browser last used (Light or Dark; System needs nothing),
// put on the page before it is first drawn, so a dark page does not
// flash light while the app loads.  A classic script loaded without
// defer from the shell page's head, before the stylesheets apply to any
// content; js/app.js keeps the value ("webmail.theme" in localStorage)
// and reconciles it with the server's setting.  Plain ES5: it runs in
// every browser, including those the app itself does not support.
(function () {
	try {
		var v = window.localStorage.getItem("webmail.theme");
		if (v === "light" || v === "dark")
			document.documentElement.setAttribute("data-theme", v);
	} catch (e) {
		// no storage: the system's theme until the app starts
	}
})();
