// The web mail client: start-up, the mail frame, the account menu, and
// signing in and out.  The shell page (index.xjs) holds the boot data in
// <script type="application/json" id="boot"> (the session.get result), the
// sysop's brand markup in <template> elements, and a block for browsers
// that cannot run this code, which is taken out of the page here first
// (js/check.js, which runs after this module, shows it when this code did
// not run).  Start-up that fails after that (an exception, no strings to
// show anything with) puts the same block back, shown: never a blank or
// half-built page.

const fallback = document.getElementById("wm-unsupported");
fallback?.remove();

import { h, clear, template, store } from "./dom.js";
import { icon } from "./icons.js";
import { t, setStrings, langTag } from "./i18n.js";
import { rpc } from "./rpc.js";
import { initNotices, notice, dismissNotice, setOffline } from "./notice.js";
import { createLayout } from "./layout.js";
import { createLogin } from "./login.js";
import { createMail } from "./mail.js";
import { avatarBadge } from "./avatar.js";
import { prefs } from "./prefs.js";
import { createContacts } from "./contacts.js";
import { createSettings } from "./settings.js";

// The theme is kept in this browser (THEME_KEY, read again by js/theme.js
// before the page is first drawn, so it does not flash) and on the server
// (settings theme, so it follows the user to other browsers; the shell
// page writes a signed-in user's stored Light or Dark on <html> too, for
// a browser that has none of its own).  SYNC_KEY is
// the server's value this browser last saw or set: once there is one the
// server's value wins at start-up; before (a choice made in this browser
// before the server kept one) the browser's choice stays until the server
// holds one other than "system".
const THEME_KEY = "webmail.theme";
const SYNC_KEY = "webmail.theme.server";
const THEMES = ["system", "light", "dark"];

const app = document.getElementById("app");

// Flex layout gap is missing in Safari before 14.1; style.css then uses
// margins (no-flex-gap).  Measured, since CSS cannot test for it.
function flexGap() {
	const d = document.createElement("div");
	d.style.cssText = "display:flex;flex-direction:column;row-gap:1px;position:absolute;visibility:hidden";
	d.append(document.createElement("div"), document.createElement("div"));
	document.body.append(d);
	const ok = d.scrollHeight === 1;
	d.remove();
	if (!ok)
		document.documentElement.classList.add("no-flex-gap");
}

let boot = readBoot();
let view = null;	// "login" or "mail"
let login = null;
let frame = null;
let started = false;
// Signed out in this page: a later session.get asks the server not to
// sign in again through the main site (webv4) while the page stays open
let signedOutHere = false;
// When the session ended and the main site's sign-in brought the user back:
// two of those in LOOP_MS mean the browser does not keep the cookie, so the
// sign-in form is shown instead of signing in again and again
const LOOP_MS = 60000;
let backAgain = [];

// ---- Start-up ----

function readBoot() {
	try {
		const v = JSON.parse(document.getElementById("boot").textContent);
		return v && typeof v === "object" ? v : {};
	} catch (e) {
		return {};
	}
}

// Take a session.get result as the session: it REPLACES the previous one,
// so nothing of a signed-out user (or of an earlier sign-in) is kept
function useSession(data) {
	if (data.strings)
		setStrings(data.strings);
	if (typeof data.locale === "string")
		document.documentElement.lang = langTag(data.locale);
	boot = data;
}

// ---- Theme: System, Light or Dark, remembered by this browser ----

function theme() {
	const v = store.get(THEME_KEY);
	return THEMES.includes(v) ? v : "system";
}

// The theme on the page and in this browser; every theme choice shown
// (the account menu's, the settings view's) follows
function applyTheme(name) {
	if (name === "light" || name === "dark")
		document.documentElement.setAttribute("data-theme", name);
	else
		document.documentElement.removeAttribute("data-theme");
	store.set(THEME_KEY, name === "system" ? null : name);
	for (const b of document.querySelectorAll("[data-theme-set]"))
		b.setAttribute("aria-pressed", String(b.getAttribute("data-theme-set") === name));
}

// The user's choice: applied at once, then saved on the server; a Promise
// of the settings.set result (rejects with the error)
function chooseTheme(name) {
	if (!THEMES.includes(name))
		return Promise.reject({ error: "invalid", message: t("errors.invalid") });
	applyTheme(name);
	return rpc.call("settings.set", { theme: name }).then((r) => {
		prefs.set(r);
		store.set(SYNC_KEY, typeof r.theme === "string" ? r.theme : name);
		return r;
	});
}

// The first theme: this browser's; when it has none of its own, the one
// the shell page wrote from the user's stored settings (the server's)
function startTheme() {
	const written = document.documentElement.getAttribute("data-theme");
	if (store.get(THEME_KEY) === null && (written === "light" || written === "dark")) {
		applyTheme(written);
		store.set(SYNC_KEY, written);
		return;
	}
	applyTheme(theme());
}

// The server's theme at start-up (see SYNC_KEY)
function reconcileTheme(r) {
	const server = r && THEMES.includes(r.theme) ? r.theme : null;
	if (server === null)
		return;
	if (store.get(SYNC_KEY) === null && server === "system" && theme() !== "system")
		return;	// this browser's earlier choice stays until one is saved
	store.set(SYNC_KEY, server);
	if (server !== theme())
		applyTheme(server);
}

// ---- Views ----

function showLogin(text, kind) {
	if (frame)
		closeFrame();
	dismissNotice();
	if (login)
		login.el.remove();
	login = createLogin({ insecure: boot.insecure === true, poweredBy: boot.powered_by, onSignedIn: signedIn,
		webv4Url: boot.webv4_only === true && typeof boot.webv4_url === "string" ? boot.webv4_url : undefined });
	app.append(login.el);
	view = "login";
	login.say(text, kind);
	login.focus();
}

function signedIn(data) {
	signedOutHere = false;	// signed in with the password since
	useSession(data);
	showMail(true);
}

function showMail(focus) {
	if (login) {
		login.el.remove();
		login = null;
	}
	if (frame)
		closeFrame();
	rpc.user = boot.user && typeof boot.user.number === "number" ? boot.user.number : 0;
	prefs.reset();
	frame = buildFrame();
	app.append(frame.el);
	frame.layout.reset();
	view = "mail";
	frame.mail.start();
	// the user's settings (theme, the senders whose images are shown), for
	// the views that use them
	prefs.load({ background: true }).then(reconcileTheme, () => {});
	if (focus)
		frame.listTitle.focus();
}

function sessionArgs() {
	return signedOutHere ? { no_webv4: true } : undefined;
}

// Session gone (expired, signed out elsewhere, password changed).  With
// the main site's sign-in allowed, the same request may have signed the
// user in again: then the mail is shown again, not the sign-in form.
function sessionEnded() {
	if (view !== "mail")
		return;
	view = "ending";
	rpc.reset();
	rpc.call("session.get", sessionArgs()).then((g) => {
		useSession(g);
		if (g.user) {
			const now = Date.now();
			backAgain = backAgain.filter((at) => now - at < LOOP_MS);
			backAgain.push(now);
			if (backAgain.length < 2) {
				showMail(true);
				return;
			}
			backAgain = [];
		}
		showLogin(t("errors.signed_out"), "info");
	}, () => showLogin(t("errors.signed_out"), "info"));
}

// Sign out and the anonymous session data, in one request.  When the
// server could not be reached the sign-out is not sent again (it may have
// happened): the user is told and stays in the frame.
async function signOut() {
	// what compose has not saved is saved first (or kept in this tab)
	if (frame && frame.mail.compose)
		await frame.mail.compose.flush();
	const out = rpc.call("session.logout");
	const get = rpc.call("session.get", { no_webv4: true }).catch(() => null);
	out.catch((e) => {
		if (e.error === "auth")
			return { webv4: false };	// the session had already ended
		throw e;
	}).then((o) => get.then((g) => {
		signedOutHere = true;
		rpc.reset();
		prefs.reset();
		if (g)
			useSession(g);
		let text = t("settings.label_signed_out");
		if (o.webv4 === true)
			text += " " + t("main.label_still_signed_in");
		showLogin(text, "info");
	}), (e) => notice(e.message));
}

// ---- The mail frame: top bar with search, three panes (filled by
// mail.js), account menu ----

function initial(name) {
	const c = Array.from(String(name || "").trim())[0];
	return c ? c.toUpperCase() : "?";
}

function buildFrame() {
	const u = boot.user || {};
	const who = u.alias || u.name || "";

	const backBtn = (label, cls) => h("button", {
		class: cls ? "ib " + cls : "ib", type: "button", "aria-label": label, onclick: () => layout.back()
	}, icon("back"));

	const folders = h("nav", { class: "pane", id: "folders", "aria-labelledby": "folders-title" },
		h("div", { class: "pane-bar" },
			backBtn(t("main.button_back")),
			h("h2", { class: "pane-title", id: "folders-title", tabindex: "-1", "data-pane-title": true,
				text: t("folders.label_folders") })));

	const menuBtn = h("button", {
		class: "ib ph-only", type: "button", "aria-label": t("folders.label_folders"),
		onclick: (e) => layout.show("folders", { push: true, opener: e.currentTarget })
	}, icon("menu"));
	const listTitle = h("h2", { class: "pane-title", id: "list-title", tabindex: "-1", "data-pane-title": true,
		text: t("list.label_messages") });
	const listBar = h("div", { class: "pane-bar" }, menuBtn, listTitle);
	const list = h("section", { class: "pane", id: "list-pane", "aria-labelledby": "list-title" }, listBar);

	const reader = h("section", { class: "pane", id: "reader", "aria-labelledby": "reader-title" },
		h("div", { class: "pane-bar" },
			backBtn(t("reader.button_back"), "ph-only"),
			h("h2", { class: "pane-title vh", id: "reader-title", text: t("reader.label_message") })),
		h("div", { class: "reader-empty" },
			icon("mailopen"),
			h("p", { text: t("reader.label_choose_message") })));

	// Contacts and Settings: one view at a time in place of the list and
	// the reader, with its own history entry (Back returns to the mail)
	const auxTitle = h("h2", { class: "pane-title", id: "aux-title", tabindex: "-1", "data-pane-title": true });
	const auxBody = h("div", { class: "aux-host" });
	const aux = h("section", { class: "pane", id: "aux", "aria-labelledby": "aux-title" },
		h("div", { class: "pane-bar" }, backBtn(t("main.button_back_to_mail")), auxTitle), auxBody);
	let auxView = null;	// { name, view }

	const panes = h("div", { class: "panes" }, folders, list, reader, aux);
	const el = h("div", { class: "view", id: "shell", "data-pane": "list" });
	const layout = createLayout(el, { folders, list, reader, aux });
	const mail = createMail({ layout, folderPane: folders, listPane: list, readerPane: reader, listBar, listTitle,
		readerBar: reader.querySelector(".pane-bar"),
		pollMs: boot.poll_ms, poweredBy: boot.powered_by, limits: boot.limits, user: u, host: () => el,
		onContacts: (b) => openAux("contacts", b), onSettings: (b) => openAux("settings", b),
		onNavigate: () => {
			if (layout.current() === "aux")
				layout.back();
		} });

	function makeAux(name) {
		if (name === "contacts")
			return createContacts({ write: (to, opener) => mail.compose.open({ mode: "new", to, opener }) });
		return createSettings({ user: u, productName: boot.product_name, poweredBy: boot.powered_by,
			usage: () => mail.usage(), theme, setTheme: chooseTheme });
	}

	function closeAux() {
		if (!auxView)
			return;
		auxView.view.destroy();
		auxView = null;
		clear(auxBody);
		mail.setAux(null);
	}

	// Show the contacts or settings view; opener gets focus back when it
	// closes
	function openAux(name, opener) {
		acct.close(false);
		if (auxView && auxView.name === name && layout.current() === "aux") {
			auxTitle.focus();
			return;
		}
		if (auxView) {
			auxView.view.destroy();
			clear(auxBody);
		}
		auxView = { name, view: makeAux(name) };
		auxTitle.textContent = t(name === "contacts" ? "contacts.label_contacts" : "settings.label_settings");
		auxBody.append(auxView.view.el);
		mail.setAux(name);
		if (layout.current() === "aux")
			auxTitle.focus();	// one view in place of the other: no new history entry
		else
			layout.show("aux", { push: true, opener: opener || document.activeElement });
	}

	const acct = accountMenu(who, u, openAux);
	const topbar = h("header", { class: "topbar" },
		h("div", { class: "brand" }, template("wm-brand")),
		mail.search.el,
		h("div", { class: "acct" }, acct.button));
	el.append(topbar, panes, acct.scrim, acct.pop);
	layout.onchange = (name) => {
		acct.close(false);
		if (name !== "aux")
			closeAux();
	};
	return { el, layout, listTitle, acct, mail, closeAux };
}

function accountMenu(who, u, openAux) {
	const button = h("button", {
		class: "avatar-btn", type: "button", "aria-haspopup": "dialog", "aria-expanded": "false",
		"aria-controls": "acct-pop", "aria-label": t("main.label_account", { name: who }),
		onclick: () => (pop.hidden ? open() : close(true))
	}, avatarBadge(u.avatar, initial(who), "face"));
	const labels = [
		["system", t("settings.label_theme_system")],
		["light", t("settings.label_theme_light")],
		["dark", t("settings.label_theme_dark")]
	];
	const seg = labels.map(([name, text]) => h("button", {
		type: "button", "data-theme-set": name, "aria-pressed": String(theme() === name), text,
		onclick: () => {
			// applied either way; a failed save holds in this browser only
			chooseTheme(name).catch(() => notice(t("settings.label_theme_local_only")));
		}
	}));
	const head = h("div", { class: "pop-head", id: "acct-head" }, h("b", { text: who }));
	if (u.name && u.name !== who)
		head.append(h("small", { text: u.name }));
	const signOutBtn = h("button", { class: "mi", type: "button", onclick: () => {
		close(false);
		signOut();
	} }, icon("logout"), h("span", { text: t("settings.button_sign_out") }));
	// the views open with focus to come back to the account button
	const viewBtn = (name, ic, label) => h("button", { class: "mi", type: "button", "data-aux": name,
		onclick: () => openAux(name, button) }, icon(ic), h("span", { text: label }));
	const pop = h("div", { class: "pop", id: "acct-pop", role: "dialog", "aria-labelledby": "acct-head", hidden: true,
		onkeydown: (e) => {
			if (e.key === "Escape") {
				e.preventDefault();
				close(true);
			}
		}
	},
		head,
		h("hr"),
		h("div", { class: "pop-label", id: "theme-label", text: t("settings.label_theme") }),
		h("div", { class: "seg", role: "group", "aria-labelledby": "theme-label" }, seg),
		h("hr"),
		viewBtn("contacts", "users", t("contacts.label_contacts")),
		viewBtn("settings", "sliders", t("settings.label_settings")),
		h("hr"),
		signOutBtn);
	const scrim = h("div", { class: "scrim", hidden: true, onclick: () => close(true) });

	function place() {
		pop.style.left = "";
		pop.style.top = "";
		if (window.matchMedia("(max-width: 899.98px)").matches)
			return;
		const r = button.getBoundingClientRect();
		pop.style.left = Math.max(8, Math.min(r.right - pop.offsetWidth, window.innerWidth - pop.offsetWidth - 8)) + "px";
		pop.style.top = (r.bottom + 6) + "px";
	}

	function open() {
		pop.hidden = false;
		scrim.hidden = false;
		button.setAttribute("aria-expanded", "true");
		place();
		(seg.find((b) => b.getAttribute("aria-pressed") === "true") || seg[0]).focus();
	}

	function close(restore) {
		if (pop.hidden)
			return;
		pop.hidden = true;
		scrim.hidden = true;
		button.setAttribute("aria-expanded", "false");
		if (restore)
			button.focus();
	}

	// Focus leaving the menu closes it
	pop.addEventListener("focusout", (e) => {
		if (!pop.hidden && e.relatedTarget && !pop.contains(e.relatedTarget) && e.relatedTarget !== button)
			close(false);
	});
	const onResize = () => close(false);
	window.addEventListener("resize", onResize);
	const destroy = () => window.removeEventListener("resize", onResize);

	return { button, pop, scrim, close, destroy };
}

function closeFrame() {
	frame.closeAux();
	frame.mail.destroy();
	frame.acct.destroy();
	frame.layout.destroy();
	frame.el.remove();
	frame = null;
}

// ---- Start-up failure: the page's own block (its text written by the
// server in the user's language, or in English), with its reload link ----

function failStart() {
	app.hidden = true;
	if (fallback && !fallback.isConnected) {
		fallback.classList.add("wm-failed");
		document.body.prepend(fallback);
	}
}

function onStartError() {
	if (!started)
		failStart();
}

// ---- Wiring ----

window.addEventListener("error", onStartError);
window.addEventListener("unhandledrejection", onStartError);

try {
	flexGap();
	rpc.hooks.auth = sessionEnded;
	rpc.hooks.signedOut = sessionEnded;
	rpc.hooks.offline = setOffline;
	// Someone else signed in in this browser: this page must not act for them
	rpc.hooks.user = () => window.location.reload();

	startTheme();
	useSession(boot);
	// Without strings (the boot data could not be read) nothing can be
	// shown but the page's own block
	if (!boot.strings || typeof boot.strings !== "object")
		throw new Error("no strings");
	initNotices(app, () => rpc.retryNow());
	app.hidden = false;

	if (boot.ok === true && boot.user)
		showMail(false);
	else if (boot.ok === false)
		showLogin(boot.message || t("errors.internal"), "error");
	else
		showLogin(boot.signed_out === true ? t("errors.signed_out") : "", "info");
	started = true;
} catch (e) {
	failStart();
}
