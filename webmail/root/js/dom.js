// Small DOM helpers.  Text is always set as text (textContent or an
// attribute), never parsed as markup.

// h(tag, props, ...children): an element.  props: "class", "text"
// (textContent), "on<event>" (a listener), anything else an attribute
// (true gives an empty attribute; null, undefined and false are skipped).
// Children are nodes or strings (as text); arrays are flattened.
export function h(tag, props, ...children) {
	const el = document.createElement(tag);
	if (props) {
		for (const [k, v] of Object.entries(props)) {
			if (v === null || v === undefined || v === false)
				continue;
			if (k === "class")
				el.className = v;
			else if (k === "text")
				el.textContent = v;
			else if (k.startsWith("on") && typeof v === "function")
				el.addEventListener(k.slice(2), v);
			else
				el.setAttribute(k, v === true ? "" : String(v));
		}
	}
	for (const c of children.flat()) {
		if (c === null || c === undefined || c === false)
			continue;
		el.append(typeof c === "string" ? document.createTextNode(c) : c);
	}
	return el;
}

// The content of a <template> written by the shell page (the sysop's brand,
// sign-in text and footer markup), or an empty fragment
export function template(id) {
	const t = document.getElementById(id);
	return t && t.content ? t.content.cloneNode(true) : document.createDocumentFragment();
}

// Remove every child of el
export function clear(el) {
	while (el.firstChild)
		el.removeChild(el.firstChild);
}

// localStorage, which may be missing or refuse (private windows, blocked
// site data): failures read as null and writes are dropped
export const store = {
	get(key) {
		try {
			return window.localStorage.getItem(key);
		} catch (e) {
			return null;
		}
	},
	set(key, value) {
		try {
			if (value === null)
				window.localStorage.removeItem(key);
			else
				window.localStorage.setItem(key, value);
		} catch (e) {
			// storage unavailable: the setting lasts for this page only
		}
	}
};
