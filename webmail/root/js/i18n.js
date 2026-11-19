// Localized strings.  The server sends them (the boot data, and session.get
// after sign-in or sign-out) as { "section.key": "text" }.
//
// t(key, vars): the text, with each {name} replaced by vars[name]; the key
// itself when there is no such string.  When vars holds a number, the
// first number in vars decides the plural: exactly 1 uses "<key>_one" if
// that string exists.

let strings = Object.create(null);

export function setStrings(s) {
	const next = Object.create(null);
	if (s && typeof s === "object") {
		for (const k of Object.keys(s)) {
			if (typeof s[k] === "string")
				next[k] = s[k];
		}
	}
	strings = next;
}

function firstNumber(vars) {
	for (const k of Object.keys(vars)) {
		if (typeof vars[k] === "number")
			return vars[k];
	}
	return undefined;
}

export function t(key, vars) {
	let s = strings[key];
	if (vars && firstNumber(vars) === 1 && typeof strings[key + "_one"] === "string")
		s = strings[key + "_one"];
	if (typeof s !== "string")
		return key;
	if (!vars)
		return s;
	return s.replace(/\{([a-z0-9_]+)\}/g, (m, name) =>
		Object.prototype.hasOwnProperty.call(vars, name) ? String(vars[name]) : m);
}

// A locale name as a language tag: "en_us" -> "en-US"
export function langTag(locale) {
	const m = /^([a-z]{2,3})(?:_([a-z]{2,4}))?$/.exec(typeof locale === "string" ? locale : "");
	if (!m)
		return "en";
	return m[2] ? m[1] + "-" + m[2].toUpperCase() : m[1];
}
