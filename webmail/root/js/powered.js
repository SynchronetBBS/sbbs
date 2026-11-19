// The "Powered by Synchronet ..." line (the sign-in page's footer and the
// bottom of the folder pane).
//
// poweredBy(version)  the line as an element, or null for no version
//                     text: the localized main.label_powered_by with
//                     {version} replaced by the system's version notice,
//                     whose word "Synchronet" links to the project's site.
//                     Built from text nodes and one <a>; nothing is parsed
//                     as markup.

import { h } from "./dom.js";
import { t } from "./i18n.js";

const PRODUCT = "Synchronet";
const SITE = "https://www.synchro.net/";

function versionNodes(version) {
	const i = version.indexOf(PRODUCT);
	if (i < 0)
		return [version];
	return [version.slice(0, i),
		h("a", { href: SITE, rel: "noopener noreferrer", target: "_blank", text: PRODUCT }),
		version.slice(i + PRODUCT.length)];
}

export function poweredBy(version) {
	if (typeof version !== "string" || version === "")
		return null;
	const pattern = t("main.label_powered_by");
	const at = pattern.indexOf("{version}");
	const el = h("p", { class: "powered" });
	if (at < 0) {
		el.append(...versionNodes(version));
		return el;
	}
	el.append(pattern.slice(0, at), ...versionNodes(version), pattern.slice(at + "{version}".length));
	return el;
}
