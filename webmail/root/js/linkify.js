// Web addresses in plain text as links.
//
// linkify(text)          the text in pieces: strings, and { text, href }
//                        for each address found.  Only http:// and https://
//                        addresses become links, and bare "www." host names
//                        (as https://).  No other scheme (javascript:,
//                        data:, vbscript:, file:, mailto: ...) ever does.
//                        Punctuation that ends a sentence is left out of
//                        the link, and so is a closing bracket without its
//                        opening one ("(see https://x.example/a)").  An
//                        address longer than MAX_LINK stays text.
// appendLinks(el, text)  appends the text to el as text nodes and <a>
//                        elements (built with DOM calls; the link's text is
//                        the address as written), opening in a new tab
//                        with no opener and no referrer

const FIND = /(?:https?:\/\/|www\.)[^\s<>"\u0000-\u001f\u007f\u00a0\u2000-\u200b\u2028\u2029\u3000]+/gi;
const TRAIL = ".,;:!?'\"*_~";
const PAIRS = { ")": "(", "]": "[", "}": "{" };
const MAX_LINK = 2048;		// a longer run is left as text

// The address without what ends the sentence around it.  The brackets are
// counted once, then kept up to date as characters come off the end, so
// a long run of them costs no more than its length.
function trim(s) {
	const n = { "(": 0, ")": 0, "[": 0, "]": 0, "{": 0, "}": 0 };
	for (let i = 0; i < s.length; i++) {
		const c = s.charAt(i);
		if (Object.prototype.hasOwnProperty.call(n, c))
			n[c]++;
	}
	let end = s.length;
	while (end > 0) {
		const last = s.charAt(end - 1);
		if (TRAIL.includes(last))
			end--;
		else if (PAIRS[last] && n[last] > n[PAIRS[last]]) {
			n[last]--;
			end--;
		} else
			break;
	}
	return s.slice(0, end);
}

// The link for an address as written, or null
function target(text) {
	const href = /^www\./i.test(text) ? "https://" + text : text;
	let u;
	try {
		u = new URL(href);
	} catch (e) {
		return null;
	}
	if ((u.protocol !== "http:" && u.protocol !== "https:") || u.hostname === "" || !/[a-z0-9]/i.test(u.hostname))
		return null;
	return u.href;
}

export function linkify(text) {
	const out = [];
	let at = 0;
	FIND.lastIndex = 0;
	let m;
	while ((m = FIND.exec(text)) !== null) {
		const start = m.index;
		const prev = start > 0 ? text.charAt(start - 1) : "";
		const www = m[0].charAt(0) === "w" || m[0].charAt(0) === "W";
		// part of a longer word ("xhttp://", "user@www.", "a.www.")
		if (/[a-z0-9]/i.test(prev) || (www && /[.@\/_\-]/.test(prev)))
			continue;
		const s = trim(m[0]);
		FIND.lastIndex = start + Math.max(1, s.length);
		if (s.length > MAX_LINK) {
			FIND.lastIndex = start + m[0].length;
			continue;
		}
		const href = s.length > (www ? 4 : 0) ? target(s) : null;
		if (href === null)
			continue;
		if (start > at)
			out.push(text.slice(at, start));
		out.push({ text: s, href });
		at = start + s.length;
	}
	if (at < text.length)
		out.push(text.slice(at));
	return out;
}

export function appendLinks(el, text) {
	for (const piece of linkify(text)) {
		if (typeof piece === "string") {
			el.append(document.createTextNode(piece));
			continue;
		}
		const a = document.createElement("a");
		a.href = piece.href;
		a.rel = "noopener noreferrer";
		a.target = "_blank";
		a.textContent = piece.text;
		el.append(a);
	}
}
