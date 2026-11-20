// The settings view: the user's web mail settings (settings.get), shown in
// the mail frame in place of the list and the reader (app.js puts it in
// the aux pane).
//
// const view = createSettings({ user, productName, poweredBy, usage, theme, setTheme })
//   user: session.get's user (the name used when no display name is set);
//   productName, poweredBy: session.get's, for the About section;
//   usage(): folder.list's usage ({bytes, quota}) or null;
//   theme(): the theme in use ("system", "light", "dark");
//   setTheme(name): applies a theme at once, keeps it in this browser and
//   saves it on the server; a Promise of the settings.set result.
// view.el          the view's content (below the pane's bar)
// view.focus()     focus on the first section's field
// view.destroy()
//
// Each section is saved on its own (one settings.set call each, which the
// server applies as a whole), so a refusal in one leaves the others as
// they are; each shows "Saved" or the server's text for the refusal
// beside its field.  The choices that are one click (From address,
// theme) are saved when made; the text fields have a Save button.
//   Your name         display_name: used on Internet mail only
//   From address      from_local and from_domain, chosen as one of the
//                     full addresses (from_addresses); hidden with only one
//   Signature         signature: text, "" (no signature on mail) or null
//                     ("Use my BBS signature": the general signature
//                     again); disabled when the system adds none
//                     (signature_enabled false)
//   Theme             theme
//   Images            images_always, by images_add and images_remove
//   Mail forwarding   forward_enabled, forward_address: the BBS account's
//                     own forwarding setting; disabled with the reason
//                     when not allowed
//   About             product name, storage used, the Powered by line
//
// Everything from the server goes into the page as text (textContent,
// value).

import { h, clear } from "./dom.js";
import { icon } from "./icons.js";
import { t } from "./i18n.js";
import { rpc } from "./rpc.js";
import { busy } from "./notice.js";
import { prefs } from "./prefs.js";
import { size, fromChoices, fromIndex } from "./format.js";
import { poweredBy } from "./powered.js";

let seq = 0;

export function createSettings(o) {
	const user = o.user || {};
	const userName = user.name || user.alias || "";
	let dead = false;
	const uid = "wm-set-" + (++seq);

	const body = h("div", { class: "form settings" });
	const stateEl = h("div", { class: "aux-state" });
	const el = h("div", { class: "aux-body" }, h("div", { class: "aux-scroll" }, stateEl, body));

	// ---- Helpers ----

	// A section: its heading, its content, and its status line (Saved, or
	// the server's text for a refusal)
	function section(key, title) {
		const head = h("h3", { class: "set-head", id: uid + "-" + key, text: title });
		const status = h("p", { class: "set-status", id: uid + "-" + key + "-status", role: "status", "aria-live": "polite" });
		const box = h("div", { class: "set-body" });
		const el2 = h("section", { class: "set-sec", "aria-labelledby": head.id, "data-sec": key }, head, box, status);
		return { el: el2, box, status, statusId: status.id };
	}

	function hint(text, id) {
		return h("p", { class: "field-hint", id: id || null, text });
	}

	// The outcome of a save in a section's status line; on a refusal the
	// field it is about is marked
	function saved(sec, field, text) {
		clear(sec.status);
		sec.status.className = "set-status ok";
		sec.status.append(icon("check", "sm"), h("span", { text: text || t("settings.label_saved_short") }));
		if (field)
			field.removeAttribute("aria-invalid");
	}

	function refused(sec, field, e) {
		clear(sec.status);
		sec.status.className = "set-status err";
		sec.status.append(icon("alert", "sm"), h("span", { text: e && e.message ? e.message : t("errors.failed") }));
		if (field)
			field.setAttribute("aria-invalid", "true");
	}

	function saving(sec) {
		clear(sec.status);
		sec.status.className = "set-status";
		sec.status.textContent = t("settings.label_saving");
	}

	// settings.set with args: the result is the new settings for every view
	function save(args) {
		return rpc.call("settings.set", args).then((r) => {
			prefs.set(r);
			return r;
		});
	}

	// A button that runs f() once at a time: while it runs the button is
	// marked aria-disabled and a repeat press is ignored (it is not
	// disabled, so focus stays on it)
	function busyButton(b, f) {
		b.addEventListener("click", () => {
			if (b.disabled || b.getAttribute("aria-disabled") === "true")
				return;
			b.setAttribute("aria-disabled", "true");
			Promise.resolve(f()).finally(() => b.removeAttribute("aria-disabled"));
		});
		return b;
	}

	function saveButton(label, f) {
		return busyButton(h("button", { class: "btn", type: "button", text: label }), f);
	}

	// ---- Sections ----

	function nameSection(r) {
		const sec = section("name", t("settings.label_name_heading"));
		const id = uid + "-name";
		// maxlength: the server keeps at most 64 bytes (and the page says
		// "Saved as" when it cut), so a long paste never reaches its length
		// refusal
		const input = h("input", { id, type: "text", autocomplete: "name", spellcheck: "false", maxlength: "64",
			placeholder: userName, "aria-describedby": id + "-hint " + sec.statusId });
		input.value = r.display_name || "";
		const btn = saveButton(t("main.button_save"), () => {
			const typed = input.value.trim();
			saving(sec);
			return save({ display_name: typed }).then((r2) => {
				input.value = r2.display_name || "";
				saved(sec, input, r2.display_name !== typed ? t("settings.label_name_saved_as", { name: r2.display_name }) : "");
			}, (e) => refused(sec, input, e));
		});
		input.addEventListener("keydown", (e) => {
			if (e.key === "Enter") {
				e.preventDefault();
				btn.click();
			}
		});
		sec.box.append(h("div", { class: "field" },
			h("label", { for: id, text: t("settings.label_display_name") }), input,
			hint(t("settings.label_display_name_help", { name: userName }), id + "-hint")),
			h("div", { class: "set-actions" }, btn));
		return sec.el;
	}

	function fromSection(r) {
		const choices = fromChoices(r);
		if (choices.length < 2)
			return null;
		const sec = section("from", t("settings.label_default_from"));
		const id = uid + "-from";
		const sel = h("select", { id, "aria-describedby": id + "-hint " + sec.statusId });
		choices.forEach((c, i) => {
			sel.append(h("option", { value: String(i), text: c.addr }));
		});
		const show = (r1) => {
			sel.selectedIndex = fromIndex(choices, r1.from_local, r1.from_domain);
		};
		show(r);
		// Saved when a choice is made: with the pointer at once; with the
		// keys (arrows step through a closed select, each a "change") on
		// Enter or when focus leaves.  A reply to an earlier choice than
		// the last one sent is ignored.
		let last = sel.value;	// the choice last sent (or stored)
		let stored = sel.value;	// the server's
		let keyed = false;
		let seqF = 0;
		function commit() {
			keyed = false;
			const v = sel.value;
			if (v === last)
				return;
			const my = ++seqF;
			last = v;
			saving(sec);
			const c = choices[sel.selectedIndex] || choices[0];
			save({ from_domain: c.domain, from_local: c.local }).then((r2) => {
				if (my !== seqF)
					return;
				if (sel.value === v)
					show(r2);
				stored = last = sel.value === v ? sel.value : v;
				saved(sec, sel);
			}, (e) => {
				if (my !== seqF)
					return;
				if (sel.value === v)
					sel.value = stored;
				last = stored;
				refused(sec, sel, e);
			});
		}
		sel.addEventListener("keydown", (e) => {
			if (e.key === "Enter") {
				e.preventDefault();
				commit();
			} else if (e.key !== "Tab")
				keyed = true;
		});
		sel.addEventListener("pointerdown", () => {
			keyed = false;
		});
		sel.addEventListener("change", () => {
			if (!keyed)
				commit();
		});
		sel.addEventListener("blur", commit);
		sec.box.append(h("div", { class: "field" },
			h("label", { for: id, text: t("settings.label_from_address") }), sel,
			hint(t("settings.label_default_from_help"), id + "-hint")));
		return sec.el;
	}

	function signatureSection(r) {
		const sec = section("signature", t("settings.label_signature"));
		const id = uid + "-sig";
		const area = h("textarea", { id, rows: "5", spellcheck: "true", "aria-describedby": id + "-hint " + sec.statusId });
		const note = hint("", id + "-hint");
		const useBbs = busyButton(h("button", { class: "btn ghost", type: "button", text: t("settings.button_use_bbs_signature") }),
			() => {
				saving(sec);
				return save({ signature: null }).then((r2) => {
					fill(r2);
					saved(sec, area, t("settings.label_signature_bbs_again"));
					area.focus();
				}, (e) => refused(sec, area, e));
			});
		const btn = saveButton(t("main.button_save"), () => {
			saving(sec);
			return save({ signature: area.value }).then((r2) => {
				fill(r2);
				saved(sec, area);
			}, (e) => refused(sec, area, e));
		});
		function fill(x) {
			const on = x.signature_enabled === true;
			const src = x.signature_source;
			area.value = typeof x.signature === "string" ? x.signature : "";
			area.disabled = !on;
			btn.disabled = !on;
			useBbs.hidden = !on || src !== "mail";
			let text;
			if (!on)
				text = t("settings.label_signature_off");
			else if (src === "general")
				text = t("settings.label_signature_general");
			else if (src === "mail")
				text = t(area.value.trim() === "" ? "settings.label_signature_none_mail" : "settings.label_signature_own");
			else
				text = t("settings.label_signature_help");
			note.textContent = text;
		}
		fill(r);
		sec.box.append(h("div", { class: "field" }, h("label", { for: id, text: t("settings.label_signature_text") }),
			area, note), h("div", { class: "set-actions" }, btn, useBbs));
		return sec.el;
	}

	function themeSection() {
		const sec = section("theme", t("settings.label_theme"));
		const labels = [
			["system", t("settings.label_theme_system")],
			["light", t("settings.label_theme_light")],
			["dark", t("settings.label_theme_dark")]
		];
		const now = o.theme();
		const seg = h("div", { class: "seg set-seg", role: "group", "aria-labelledby": uid + "-theme",
			"aria-describedby": sec.statusId },
			labels.map(([name, text]) => h("button", { type: "button", "data-theme-set": name,
				"aria-pressed": String(now === name), text, onclick: () => {
					saving(sec);
					// the theme is applied either way: a failed save says it
					// holds in this browser only
					Promise.resolve(o.setTheme(name)).then(() => saved(sec, null),
						() => refused(sec, null, { message: t("settings.label_theme_local_only") }));
				} })));
		sec.box.append(seg, hint(t("settings.label_theme_help")));
		return sec.el;
	}

	function imagesSection(r) {
		const sec = section("images", t("settings.label_images"));
		const id = uid + "-img";
		const list = h("ul", { class: "set-list", "aria-labelledby": sec.el.getAttribute("aria-labelledby") });
		const none = h("p", { class: "field-hint", text: t("settings.label_images_none") });
		const input = h("input", { id, type: "email", inputmode: "email", autocomplete: "off", spellcheck: "false",
			autocapitalize: "none", placeholder: t("settings.placeholder_address"), "aria-describedby": sec.statusId });
		function fill(x) {
			clear(list);
			const senders = Array.isArray(x.images_always) ? x.images_always.filter((a) => typeof a === "string") : [];
			for (const a of senders) {
				list.append(h("li", { class: "set-item" }, h("span", { class: "ell", text: a }),
					h("button", { class: "linkbtn", type: "button", text: t("settings.button_remove"),
						"aria-label": t("settings.button_remove_sender", { address: a }), onclick: (e) => removeOne(a, e.currentTarget) })));
			}
			list.hidden = senders.length === 0;
			none.hidden = senders.length > 0;
		}
		function removeOne(a, b) {
			if (b.getAttribute("aria-disabled") === "true")
				return;
			const li = b.closest("li");
			const next = li && (li.nextElementSibling || li.previousElementSibling);
			const nextAddr = next ? next.querySelector("span").textContent : null;
			b.setAttribute("aria-disabled", "true");
			saving(sec);
			save({ images_remove: a }).then((r2) => {
				fill(r2);
				saved(sec, null, t("settings.label_images_removed", { address: a }));
				const again = nextAddr !== null ? Array.from(list.querySelectorAll("li")).find((x) => x.firstChild.textContent === nextAddr) : null;
				(again ? again.querySelector("button") : input).focus();
			}, (e) => {
				b.removeAttribute("aria-disabled");
				refused(sec, null, e);
			});
		}
		const add = saveButton(t("settings.button_add"), () => {
			const a = input.value.trim();
			if (a === "") {
				input.focus();
				return null;
			}
			saving(sec);
			return save({ images_add: a }).then((r2) => {
				fill(r2);
				input.value = "";
				saved(sec, input, t("settings.label_images_added", { address: a }));
			}, (e) => refused(sec, input, e));
		});
		input.addEventListener("keydown", (e) => {
			if (e.key === "Enter") {
				e.preventDefault();
				add.click();
			}
		});
		fill(r);
		sec.box.append(hint(t("settings.label_images_help")), hint(t("settings.label_images_warning")), list, none,
			h("div", { class: "field" }, h("label", { for: id, text: t("settings.label_images_add") }),
				h("div", { class: "set-row" }, input, add)));
		return sec.el;
	}

	function forwardSection(r) {
		const sec = section("forward", t("settings.label_forwarding"));
		const id = uid + "-fwd";
		const f = r.forward && typeof r.forward === "object" ? r.forward : {};
		const allowed = f.allowed === true;
		const box = h("input", { id: id + "-on", type: "checkbox" });
		const input = h("input", { id, type: "email", inputmode: "email", autocomplete: "email", spellcheck: "false",
			autocapitalize: "none", placeholder: t("settings.placeholder_address"),
			"aria-describedby": id + "-keep " + id + "-use " + sec.statusId });
		const keep = hint("", id + "-keep");
		let stored = "";
		function fill(x) {
			stored = typeof x.address === "string" ? x.address : "";
			box.checked = x.enabled === true;
			input.value = stored;
			keep.textContent = t(x.keeps_copy === true ? "settings.label_forward_kept" : "settings.label_forward_not_kept");
		}
		fill(f);
		const btn = saveButton(t("main.button_save"), () => {
			const a = input.value.trim();
			const args = { forward_enabled: box.checked };
			if (a !== stored && !(a === "" && !box.checked))
				args.forward_address = a;
			saving(sec);
			return save(args).then((r2) => {
				fill(r2.forward || {});
				saved(sec, input, t(box.checked ? "settings.label_forward_on" : "settings.label_forward_off_now"));
			}, (e) => refused(sec, input, e));
		});
		input.addEventListener("keydown", (e) => {
			if (e.key === "Enter") {
				e.preventDefault();
				btn.click();
			}
		});
		sec.box.append(hint(t("settings.label_forward_same"), id + "-same"));
		let describe = id + "-same";
		if (!allowed) {
			sec.box.append(h("div", { class: "msgbox info", role: "note", id: id + "-why" }, icon("info"),
				h("p", { text: t(f.reason === "restricted" ? "settings.label_forward_restricted" : "settings.label_forward_off") })));
			describe = id + "-why " + describe;
			box.disabled = true;
			input.disabled = true;
			btn.disabled = true;
		}
		box.setAttribute("aria-describedby", describe + " " + sec.statusId);
		sec.box.append(h("label", { class: "check", for: box.id }, box, h("span", { text: t("settings.label_forward_enable") })),
			h("div", { class: "field" }, h("label", { for: id, text: t("settings.label_forward_to") }), input, keep,
				hint(t("settings.label_forward_address_use"), id + "-use")),
			h("div", { class: "set-actions" }, btn));
		return sec.el;
	}

	function aboutSection() {
		const sec = section("about", t("settings.label_about"));
		const u = typeof o.usage === "function" ? o.usage() : null;
		const items = [];
		if (typeof o.productName === "string" && o.productName !== "")
			items.push(h("p", { class: "about-name", text: o.productName }));
		if (u && typeof u.bytes === "number") {
			const quota = typeof u.quota === "number" && u.quota > 0 ? u.quota : 0;
			items.push(h("p", { text: t("main.label_storage_heading") + ": " + (quota
				? t("main.label_storage", { used: size(u.bytes), quota: size(quota) })
				: t("main.label_storage_used", { used: size(u.bytes) })) }));
		}
		const p = poweredBy(o.poweredBy);
		if (p)
			items.push(p);
		sec.box.append(...items);
		sec.status.remove();
		return sec.el;
	}

	// ---- Loading ----

	function render(r) {
		clear(body);
		clear(stateEl);
		body.append(...[nameSection(r), fromSection(r), signatureSection(r), themeSection(), imagesSection(r),
			forwardSection(r), aboutSection()].filter((x) => x !== null));
	}

	function load() {
		clear(stateEl);
		const p = prefs.load().then((r) => {
			if (!dead)
				render(r);
		}, (e) => {
			if (dead)
				return;
			clear(stateEl);
			stateEl.append(h("div", { class: "empty failed", role: "alert" },
				h("span", { class: "badge" }, icon("alert")),
				h("p", { class: "lead", text: t("settings.label_load_failed") }),
				e && e.message && e.error !== "failed" ? h("p", { text: e.message }) : null,
				h("button", { class: "btn", type: "button", text: t("list.button_try_again"), onclick: () => load() })));
			const b = stateEl.querySelector("button");
			if (b)
				b.focus();
		});
		busy(stateEl, p);
	}

	load();

	return {
		el,
		focus: () => {
			const f = body.querySelector("input, select, textarea, button");
			if (f)
				f.focus();
		},
		destroy: () => {
			dead = true;
		}
	};
}
