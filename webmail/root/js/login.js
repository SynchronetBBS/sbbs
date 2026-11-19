// The sign-in view.
//
// const view = createLogin({ insecure, poweredBy, onSignedIn })
//   poweredBy: the system's version notice for the footer line ("" none)
//   view.el                the view's element (to be placed in the page)
//   view.say(text, kind)   a message above the form: "error" (an alert),
//                          or "info" (signed out, session ended)
//   view.focus()           focus the first field that needs input
// onSignedIn(data) gets the session.get result for the user once the
// server accepts the name and password.

import { h, template } from "./dom.js";
import { icon } from "./icons.js";
import { t } from "./i18n.js";
import { rpc } from "./rpc.js";
import { poweredBy as poweredLine } from "./powered.js";

export function createLogin({ insecure, poweredBy, onSignedIn }) {
	let pending = false;

	const errText = h("p");
	const errBox = h("div", { class: "msgbox err", role: "alert", hidden: true }, icon("alert"), errText);
	const infoText = h("p");
	const infoBox = h("div", { class: "msgbox info", role: "status", hidden: true }, icon("info"), infoText);
	const user = h("input", {
		id: "login-user", name: "user", autocomplete: "username", autocapitalize: "none",
		autocorrect: "off", spellcheck: "false", required: true, enterkeyhint: "next"
	});
	const pass = h("input", {
		id: "login-pass", name: "password", type: "password", autocomplete: "current-password",
		required: true, enterkeyhint: "go"
	});
	const keep = h("input", { id: "login-keep", type: "checkbox", "aria-describedby": "login-keep-help" });
	const submit = h("button", { class: "btn primary", type: "submit", text: t("main.button_sign_in") });

	const form = h("form", { class: "card", novalidate: true, "aria-labelledby": "login-title", onsubmit: send },
		h("div", { class: "brand" }, template("wm-brand")),
		h("h1", { id: "login-title", tabindex: "-1", text: t("main.label_sign_in_title") }),
		insecure ? h("div", { class: "msgbox warn" }, icon("alert"),
			h("p", { text: t("main.label_not_encrypted") })) : null,
		infoBox,
		errBox,
		h("div", { class: "field" }, h("label", { for: "login-user", text: t("main.label_user_name") }), user),
		h("div", { class: "field" }, h("label", { for: "login-pass", text: t("main.label_password") }), pass),
		h("label", { class: "check", for: "login-keep" }, keep, h("span", { text: t("main.label_keep_signed_in") })),
		h("p", { class: "hint", id: "login-keep-help", text: t("main.label_keep_signed_in_help") }),
		submit,
		h("div", { class: "sysop" }, template("wm-login-text")),
		h("div", { class: "sysop" }, template("wm-footer")),
		poweredLine(poweredBy)
	);
	const el = h("main", { id: "login", class: "view" }, form);

	function say(text, kind) {
		errBox.hidden = true;
		infoBox.hidden = true;
		if (!text)
			return;
		if (kind === "error") {
			errText.textContent = text;
			errBox.hidden = false;
		} else {
			infoText.textContent = text;
			infoBox.hidden = false;
		}
	}

	function focus() {
		(user.value === "" ? user : pass).focus();
	}

	function busy(on) {
		pending = on;
		submit.setAttribute("aria-disabled", on ? "true" : "false");
		submit.textContent = t(on ? "main.label_signing_in" : "main.button_sign_in");
	}

	function send(e) {
		e.preventDefault();
		if (pending)
			return;
		const name = user.value.trim();
		if (name === "" || pass.value === "") {
			say(t("main.label_enter_credentials"), "error");
			(name === "" ? user : pass).focus();
			return;
		}
		busy(true);
		// One request: sign in, then the session data as the new user
		const login = rpc.call("session.login", { user: name, password: pass.value, keep: keep.checked });
		const get = rpc.call("session.get");
		Promise.allSettled([login, get]).then(([l, g]) => {
			busy(false);
			if (l.status === "fulfilled" && g.status === "fulfilled" && g.value.user) {
				pass.value = "";
				onSignedIn(g.value);
				return;
			}
			const err = l.status === "rejected" ? l.reason : (g.status === "rejected" ? g.reason : null);
			say(err && err.message ? err.message : t("errors.internal"), "error");
			pass.value = "";
			pass.focus();
		});
	}

	return { el, say, focus };
}
