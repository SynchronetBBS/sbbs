// A message's attachments: the list of chips (download, and an in-app
// preview for images), and fetching the images an HTML body shows inline.
//
// partUrl(id, pid, dl)    the relative part.ssjs URL of a part, or null when
//                         id or pid is not a valid id (only the message id
//                         and the part id ever go in the URL, never a name)
// isImage(type)           one of the four types part.ssjs shows inline
// attachmentList(id, parts, opener)
//                         the chips for parts ({pid, name, type, size}).
//                         A part of size -1 is gone: its chip says so and
//                         has no links.  opener(part, button) opens the
//                         preview of an image.
// openPreview(id, part, returnTo)
//                         a dialog showing the image; Escape or Close
//                         close it and focus goes back to returnTo
// inlineImages(id, parts, wanted, signal)
//                         a Promise of { "<lower-cased content id>": data
//                         URL } for the parts with a Content-ID in wanted
//                         (a Set) whose type is an image type above:
//                         each fetched from part.ssjs with the session's
//                         cookie, one at a time, given up after
//                         FETCH_TIMEOUT, checked for its type and size (at
//                         most MAX_IMAGE bytes each; no more downloads once
//                         MAX_IMAGES have arrived for the message) and made
//                         a data: URL.  One that cannot be fetched is left
//                         out (a broken image), never an error.  signal (an
//                         AbortSignal) stops the fetching: the images
//                         that arrived until then are returned.  (The
//                         size a part is listed with is that of its
//                         encoded form, so the list is filtered loosely
//                         and the bytes received decide.)
// contentId(part)         a part's Content-ID without <>, lower-cased

import { h } from "./dom.js";
import { icon } from "./icons.js";
import { t } from "./i18n.js";
import { size } from "./format.js";

const IMAGE_TYPES = ["image/png", "image/jpeg", "image/gif", "image/webp"];
const MAX_IMAGE = 5 * 1048576;
const MAX_IMAGES = 20 * 1048576;
const FETCH_TIMEOUT = 10000;
const MSG_ID = /^[sp][1-9][0-9]{0,9}$/;
const PART_ID = /^(?:f[1-9][0-9]{0,5}|[0-9]{1,6}(?:\.[0-9]{1,6}){0,19})$/;

export function partUrl(id, pid, dl) {
	if (!MSG_ID.test(String(id)) || !PART_ID.test(String(pid)))
		return null;
	return "part.ssjs?id=" + encodeURIComponent(id) + "&pid=" + encodeURIComponent(pid) + "&dl=" + (dl ? "1" : "0");
}

export function isImage(type) {
	return IMAGE_TYPES.includes(String(type || "").toLowerCase());
}

function partName(p) {
	return typeof p.name === "string" && p.name !== "" ? p.name : t("reader.label_attachment");
}

export function attachmentList(id, parts, opener) {
	const box = h("ul", { class: "atts", "aria-label": t("main.label_attachments") });
	for (const p of parts) {
		const name = partName(p);
		const gone = p.size === -1;
		const dl = gone ? null : partUrl(id, p.pid, true);
		const img = isImage(p.type);
		const meta = h("span", { class: "meta" },
			h("span", { class: "ell", text: name }),
			h("small", { text: gone || !dl ? t("reader.label_attachment_gone") : size(p.size) }));
		const chip = h("li", { class: "att" + (gone || !dl ? " gone" : "") }, icon(img ? "image" : "file", "lead"), meta);
		if (dl && img) {
			const view = h("button", { class: "ib", type: "button", title: t("reader.button_view"),
				"aria-label": t("reader.label_view_name", { name }) }, icon("eye"));
			view.addEventListener("click", () => opener(p, view));
			chip.append(view);
		}
		if (dl) {
			chip.append(h("a", { class: "ib", href: dl, title: t("reader.button_download"),
				"aria-label": t("reader.label_download_name", { name }) }, icon("download")));
		}
		box.append(chip);
	}
	return box;
}

// ---- Preview ----

let open = null;

function focusables(el) {
	return Array.from(el.querySelectorAll("a[href], button:not([disabled]), [tabindex]:not([tabindex='-1'])"))
		.filter((e) => !e.hidden && e.getClientRects().length > 0);
}

export function closePreview(restore) {
	if (!open)
		return;
	const o = open;
	open = null;
	document.removeEventListener("focusin", o.onFocus, true);
	o.el.remove();
	if (restore !== false && o.returnTo && o.returnTo.isConnected)
		o.returnTo.focus({ preventScroll: true });
}

export function openPreview(id, part, returnTo) {
	closePreview(false);
	const src = partUrl(id, part.pid, false);
	const dl = partUrl(id, part.pid, true);
	if (!src)
		return;
	const name = partName(part);
	const titleId = "wm-preview-title";
	const close = h("button", { class: "ib", type: "button", title: t("main.button_close"),
		"aria-label": t("main.button_close"), onclick: () => closePreview(true) }, icon("x"));
	const failed = h("p", { class: "preview-failed", text: t("reader.label_preview_failed"), hidden: true });
	const img = h("img", { alt: name, src });
	img.addEventListener("error", () => {
		img.hidden = true;
		failed.hidden = false;
	});
	const dialog = h("div", { class: "preview", role: "dialog", "aria-modal": "true", "aria-labelledby": titleId },
		h("div", { class: "preview-bar" },
			h("h2", { class: "ell", id: titleId, text: name }),
			h("a", { class: "ib", href: dl, title: t("reader.button_download"),
				"aria-label": t("reader.label_download_name", { name }) }, icon("download")),
			close),
		h("div", { class: "preview-body", onclick: (e) => {
			if (e.target === e.currentTarget)
				closePreview(true);
		} }, img, failed));
	const el = h("div", { class: "preview-wrap" }, dialog);
	dialog.addEventListener("keydown", (e) => {
		if (e.key === "Escape") {
			e.preventDefault();
			e.stopPropagation();
			closePreview(true);
			return;
		}
		if (e.key !== "Tab")
			return;
		const f = focusables(dialog);
		if (f.length === 0)
			return;
		const first = f[0];
		const last = f[f.length - 1];
		if (e.shiftKey && document.activeElement === first) {
			e.preventDefault();
			last.focus();
		} else if (!e.shiftKey && document.activeElement === last) {
			e.preventDefault();
			first.focus();
		}
	});
	// Focus that gets outside (a click on the page behind, a screen
	// reader's own movement) is brought back in
	const onFocus = (e) => {
		if (open && !dialog.contains(e.target))
			close.focus({ preventScroll: true });
	};
	document.getElementById("app").append(el);
	open = { el, returnTo, onFocus };
	document.addEventListener("focusin", onFocus, true);
	close.focus({ preventScroll: true });
}

// ---- Inline images ----

function dataUrl(blob, type) {
	return new Promise((resolve) => {
		const r = new FileReader();
		r.onload = () => resolve(typeof r.result === "string" && r.result.startsWith("data:" + type + ";base64,")
			? r.result : null);
		r.onerror = () => resolve(null);
		r.readAsDataURL(new Blob([blob], { type }));
	});
}

// One image, or null; given up after FETCH_TIMEOUT
async function fetchImage(url, room, signal) {
	const ctl = typeof AbortController === "function" ? new AbortController() : null;
	const timer = ctl ? setTimeout(() => ctl.abort(), FETCH_TIMEOUT) : 0;
	const stop = () => {
		if (ctl)
			ctl.abort();
	};
	if (signal)
		signal.addEventListener("abort", stop);
	try {
		const resp = await fetch(url, { credentials: "same-origin", cache: "no-store", signal: ctl ? ctl.signal : undefined });
		if (!resp.ok)
			return null;
		const type = String(resp.headers.get("Content-Type") || "").split(";")[0].trim().toLowerCase();
		const len = Number(resp.headers.get("Content-Length"));
		if (!isImage(type) || len > MAX_IMAGE || len > room) {
			if (ctl)
				ctl.abort();
			return null;
		}
		const blob = await resp.blob();
		if (blob.size === 0 || blob.size > MAX_IMAGE || blob.size > room)
			return null;
		return { type, blob };
	} catch (e) {
		return null;	// failed, refused or too slow
	} finally {
		clearTimeout(timer);
		if (signal)
			signal.removeEventListener("abort", stop);
	}
}

// A part's Content-ID without its angle brackets, lower-cased ("" for none)
export function contentId(p) {
	return typeof p.cid === "string" ? p.cid.trim().replace(/^<|>$/g, "").toLowerCase() : "";
}

export async function inlineImages(id, parts, wanted, signal) {
	const map = {};
	let total = 0;
	const list = parts.filter((p) => {
		const c = contentId(p);
		return c !== "" && wanted.has(c) && isImage(p.type) && typeof p.size === "number"
			&& p.size >= 0 && p.size <= MAX_IMAGE * 1.4 && partUrl(id, p.pid, false) !== null;
	});
	// one at a time, so the total stops further downloads
	for (const p of list) {
		if (total >= MAX_IMAGES || (signal && signal.aborted))
			break;
		const g = await fetchImage(partUrl(id, p.pid, false), MAX_IMAGES - total, signal);
		if (!g)
			continue;
		const url = await dataUrl(g.blob, g.type);
		if (url) {
			total += g.blob.size;
			map[contentId(p)] = url;
		}
	}
	return map;
}
