// The search field in the top bar.
//
// const search = createSearch(onSearch)
//   onSearch(query) is called when the form is submitted (Enter or the
//   Search button) with the query, white space trimmed, at most MAX_QUERY
//   characters; "" means clear the search.  The clear button calls it with
//   "" and puts focus back in the field.
// search.el            the form
// search.set(query)    shows query in the field (when the search changes
//                      from elsewhere, e.g. "Clear search" in the list)
// search.focus()

import { h } from "./dom.js";
import { icon } from "./icons.js";
import { t } from "./i18n.js";

export const MAX_QUERY = 200;

export function createSearch(onSearch) {
	const input = h("input", {
		id: "wm-q", type: "search", autocomplete: "off", enterkeyhint: "search", spellcheck: "false",
		maxlength: String(MAX_QUERY), placeholder: t("list.placeholder_search")
	});
	const clearBtn = h("button", {
		class: "ib", type: "button", "aria-label": t("list.button_clear_search"), hidden: true,
		onclick: () => {
			input.value = "";
			clearBtn.hidden = true;
			onSearch("");
			input.focus();
		}
	}, icon("x"));
	const go = h("button", { class: "ib", type: "submit", "aria-label": t("main.button_search") }, icon("search"));
	const el = h("form", {
		class: "search", role: "search",
		onsubmit: (e) => {
			e.preventDefault();
			const q = input.value.trim().slice(0, MAX_QUERY);
			input.value = q;
			clearBtn.hidden = q === "";
			onSearch(q);
		}
	},
		h("label", { class: "vh", for: "wm-q", text: t("list.placeholder_search") }),
		input,
		h("span", { class: "search-btns" }, clearBtn, go));
	input.addEventListener("input", () => {
		clearBtn.hidden = input.value === "";
	});

	return {
		el,
		set(q) {
			input.value = q;
			clearBtn.hidden = q === "";
		},
		focus() {
			input.focus();
		}
	};
}
