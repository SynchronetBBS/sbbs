// Short notices at the bottom of the screen, the "no connection" bar, and
// busy(): the feedback every view uses.
//
// initNotices(app, onRetry)   once, before the others: makes the live
//                             region and the offline bar inside app
// notice(text, {action_label, on_action, ms})
//                             shows text with one optional action button
//                             (Undo); it goes after ms (default 6000) or
//                             when the next notice comes
// dismissNotice()             hides the current notice
// setOffline(on)              shows or hides the offline bar
// busy(el, promise, make)     shows a placeholder in el only if promise has
//                             not settled after 150 ms; returns promise.
//                             make() builds the placeholder (default: the
//                             "Loading ..." text); when it returns null,
//                             nothing is shown

import { h, clear } from "./dom.js";
import { icon } from "./icons.js";
import { t } from "./i18n.js";

const BUSY_DELAY = 150;

let region = null;
let bar = null;
let barText = null;
let barButton = null;
let timer = 0;
let retry = null;

export function initNotices(app, onRetry) {
	retry = onRetry;
	// The region exists from the start, so screen readers announce what is
	// put into it later
	region = h("div", { class: "toast-region", role: "status", "aria-live": "polite" });
	app.append(region);
	// The bar and its live text exist from the start too; only the text
	// changes, so the change is what is announced
	barText = h("span", { class: "grow", role: "status", "aria-live": "polite" });
	barButton = h("button", { class: "btn ghost", type: "button", onclick: () => retry && retry() });
	bar = h("div", { class: "banner", id: "offline", hidden: true }, icon("cloud"), barText, barButton);
	app.prepend(bar);
}

export function dismissNotice() {
	clearTimeout(timer);
	timer = 0;
	if (region)
		clear(region);
}

export function notice(text, opts = {}) {
	dismissNotice();
	if (!region)
		return;
	const box = h("div", { class: "toast" }, h("span", { text }));
	if (opts.action_label && typeof opts.on_action === "function") {
		box.append(h("button", {
			type: "button",
			text: opts.action_label,
			onclick: () => {
				dismissNotice();
				opts.on_action();
			}
		}));
	}
	region.append(box);
	timer = setTimeout(dismissNotice, typeof opts.ms === "number" ? opts.ms : 6000);
}

export function setOffline(on) {
	if (!bar)
		return;
	if (!on) {
		barText.textContent = "";
		bar.hidden = true;
		return;
	}
	if (!bar.hidden)
		return;
	barButton.textContent = t("main.button_retry");
	bar.hidden = false;
	// Set the text once the region is shown, so it is announced
	requestAnimationFrame(() => {
		if (!bar.hidden)
			barText.textContent = t("main.label_retrying");
	});
}

export function busy(el, promise, make) {
	let shown = null;
	const id = setTimeout(() => {
		shown = typeof make === "function" ? make()
			: h("div", { class: "busy", role: "status", text: t("main.label_loading") });
		if (!shown)
			return;
		el.setAttribute("aria-busy", "true");
		el.append(shown);
	}, BUSY_DELAY);
	const done = () => {
		clearTimeout(id);
		el.removeAttribute("aria-busy");
		if (shown && shown.parentNode === el)
			el.removeChild(shown);
	};
	promise.then(done, done);
	return promise;
}
