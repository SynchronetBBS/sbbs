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

const THEME_KEY = "webmail.theme";
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

function setTheme(name) {
	if (name === "light" || name === "dark")
		document.documentElement.setAttribute("data-theme", name);
	else
		document.documentElement.removeAttribute("data-theme");
	store.set(THEME_KEY, name === "system" ? null : name);
}

// ---- Views ----

function showLogin(text, kind) {
	if (frame)
		closeFrame();
	dismissNotice();
	if (login)
		login.el.remove();
	login = createLogin({ insecure: boot.insecure === true, poweredBy: boot.powered_by, onSignedIn: signedIn });
	app.append(login.el);
	view = "login";
	login.say(text, kind);
	login.focus();
}

function signedIn(data) {
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
	frame = buildFrame();
	app.append(frame.el);
	frame.layout.reset();
	view = "mail";
	frame.mail.start();
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
			showMail(true);
			return;
		}
		showLogin(t("errors.expired"), "info");
	}, () => showLogin(t("errors.expired"), "info"));
}

// Sign out and the anonymous session data, in one request.  When the
// server could not be reached the sign-out is not sent again (it may have
// happened): the user is told and stays in the frame.
function signOut() {
	const out = rpc.call("session.logout");
	const get = rpc.call("session.get", { no_webv4: true }).catch(() => null);
	out.catch((e) => {
		if (e.error === "auth")
			return { webv4: false };	// the session had already ended
		throw e;
	}).then((o) => get.then((g) => {
		signedOutHere = true;
		rpc.reset();
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

	const backBtn = (label) => h("button", {
		class: "ib", type: "button", "aria-label": label, onclick: () => layout.back()
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
			backBtn(t("reader.button_back")),
			h("h2", { class: "pane-title", id: "reader-title", tabindex: "-1", "data-pane-title": true,
				text: t("reader.label_message") })),
		h("div", { class: "reader-empty" },
			icon("mailopen"),
			h("p", { text: t("reader.label_choose_message") })));

	const panes = h("div", { class: "panes" }, folders, list, reader);
	const el = h("div", { class: "view", id: "shell", "data-pane": "list" });
	const layout = createLayout(el, { folders, list, reader });
	const mail = createMail({ layout, folderPane: folders, listPane: list, readerPane: reader, listBar, listTitle,
		pollMs: boot.poll_ms, poweredBy: boot.powered_by });

	const acct = accountMenu(who, u);
	const topbar = h("header", { class: "topbar" },
		h("div", { class: "brand" }, template("wm-brand")),
		mail.search.el,
		h("div", { class: "acct" }, acct.button));
	el.append(topbar, panes, acct.scrim, acct.pop);
	layout.onchange = () => acct.close(false);
	return { el, layout, listTitle, acct, mail };
}

function accountMenu(who, u) {
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
			setTheme(name);
			for (const b of seg)
				b.setAttribute("aria-pressed", String(b.getAttribute("data-theme-set") === name));
		}
	}));
	const head = h("div", { class: "pop-head", id: "acct-head" }, h("b", { text: who }));
	if (u.name && u.name !== who)
		head.append(h("small", { text: u.name }));
	const signOutBtn = h("button", { class: "mi", type: "button", onclick: () => {
		close(false);
		signOut();
	} }, icon("logout"), h("span", { text: t("settings.button_sign_out") }));
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

	setTheme(theme());
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
		showLogin(boot.signed_out === true ? t("errors.expired") : "", "info");
	started = true;
} catch (e) {
	failStart();
}
