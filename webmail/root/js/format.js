// Dates and sizes for display, in the page's language (the <html lang> the
// app sets from the user's locale) and the browser's time zone.
//
// listDate(seconds, now)  the short form for a list row: the time for today,
//                         month and day for this year, else the full date
// fullDate(seconds)       weekday, date and time (for titles)
// zonedDate(seconds, minutes)
//                         the same at a UTC offset of minutes (the sender's
//                         time), followed by "UTC+hh:mm"; "" for an offset
//                         that is not a whole number within a day
// size(bytes)             "312 KB", "1.2 MB" and so on
// number(n)               a count, grouped as the language writes it

import { t } from "./i18n.js";

let lang = null;
let fmt = null;

// Formatters, made again when the page's language changes
function formats() {
	const l = document.documentElement.lang || undefined;
	if (fmt && l === lang)
		return fmt;
	const make = (o) => {
		try {
			return new Intl.DateTimeFormat(l, o);
		} catch (e) {
			return new Intl.DateTimeFormat(undefined, o);	// a tag this browser does not know
		}
	};
	const num = (digits) => {
		try {
			return new Intl.NumberFormat(l, { maximumFractionDigits: digits });
		} catch (e) {
			return new Intl.NumberFormat(undefined, { maximumFractionDigits: digits });
		}
	};
	lang = l;
	fmt = {
		time: make({ hour: "numeric", minute: "2-digit" }),
		day: make({ month: "short", day: "numeric" }),
		date: make({ year: "numeric", month: "short", day: "numeric" }),
		full: make({ weekday: "short", year: "numeric", month: "short", day: "numeric",
			hour: "numeric", minute: "2-digit" }),
		utc: make({ weekday: "short", year: "numeric", month: "short", day: "numeric",
			hour: "numeric", minute: "2-digit", timeZone: "UTC" }),
		n0: num(0),
		n1: num(1)
	};
	return fmt;
}

function valid(seconds) {
	return typeof seconds === "number" && isFinite(seconds) && seconds > 0;
}

export function listDate(seconds, now = new Date()) {
	if (!valid(seconds))
		return "";
	const d = new Date(seconds * 1000);
	const f = formats();
	if (d.getFullYear() === now.getFullYear() && d.getMonth() === now.getMonth() && d.getDate() === now.getDate())
		return f.time.format(d);
	if (d.getFullYear() === now.getFullYear())
		return f.day.format(d);
	return f.date.format(d);
}

export function fullDate(seconds) {
	return valid(seconds) ? formats().full.format(new Date(seconds * 1000)) : "";
}

export function zonedDate(seconds, minutes) {
	if (!valid(seconds) || typeof minutes !== "number" || !Number.isInteger(minutes) || Math.abs(minutes) >= 1440)
		return "";
	const a = Math.abs(minutes);
	const zone = "UTC" + (minutes < 0 ? "-" : "+") + String(Math.floor(a / 60)).padStart(2, "0") + ":"
		+ String(a % 60).padStart(2, "0");
	return formats().utc.format(new Date((seconds + minutes * 60) * 1000)) + " " + zone;
}

export function number(n) {
	return formats().n0.format(typeof n === "number" && isFinite(n) ? n : 0);
}

export function size(bytes) {
	const f = formats();
	const n = typeof bytes === "number" && isFinite(bytes) && bytes > 0 ? bytes : 0;
	const units = [["main.label_size_kb", 1024], ["main.label_size_mb", 1048576], ["main.label_size_gb", 1073741824]];
	if (n < 1024)
		return t("main.label_size_bytes", { count: n, n: f.n0.format(n) });
	let u = units[0];
	for (const x of units) {
		if (n >= x[1])
			u = x;
	}
	const v = n / u[1];
	return t(u[0], { n: (v < 10 ? f.n1 : f.n0).format(v) });
}
