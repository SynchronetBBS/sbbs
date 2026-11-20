// Line icons, drawn on a 24 x 24 grid with the current text color.  Built
// as SVG elements so no image file or sprite needs to be fetched.

const SVG = "http://www.w3.org/2000/svg";

const p = (d) => ["path", { d }];

const ICONS = {
	alert: [p("M10.29 3.86 1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z"),
		p("M12 9v4M12 17h.01")],
	archive: [p("M21 8v13H3V8"), ["rect", { x: 1, y: 3, width: 22, height: 5, rx: 1 }], p("M10 12h4")],
	back: [p("M19 12H5M12 19l-7-7 7-7")],
	check: [p("M20 6 9 17l-5-5")],
	chev: [p("m6 9 6 6 6-6")],
	up: [p("m6 15 6-6 6 6")],
	down: [p("m6 9 6 6 6-6")],
	clip: [p("m21.44 11.05-9.19 9.19a6 6 0 0 1-8.49-8.49l9.19-9.19a4 4 0 0 1 5.66 5.66l-9.2 9.19a2 2 0 0 1-2.83-2.83l8.49-8.48")],
	code: [p("m16 18 6-6-6-6M8 6l-6 6 6 6")],
	copy: [["rect", { x: 9, y: 9, width: 13, height: 13, rx: 2 }], p("M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1")],
	clock: [["circle", { cx: 12, cy: 12, r: 10 }], p("M12 6v6l4 2")],
	cloud: [p("M22.61 16.95A5 5 0 0 0 18 10h-1.26a8 8 0 0 0-7.05-6M5 5a8 8 0 0 0 4 15h9a5 5 0 0 0 1.7-.3"),
		p("m1 1 22 22")],
	download: [p("M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"), p("m7 10 5 5 5-5M12 15V3")],
	eye: [p("M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"), ["circle", { cx: 12, cy: 12, r: 3 }]],
	forward: [p("m15 14 5-5-5-5"), p("M4 20v-7a4 4 0 0 1 4-4h12")],
	file: [p("M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"), p("M14 2v6h6")],
	image: [["rect", { x: 3, y: 3, width: 18, height: 18, rx: 2 }], ["circle", { cx: 8.5, cy: 8.5, r: 1.5 }],
		p("m21 15-5-5L5 21")],
	inbox: [p("M22 12h-6l-2 3h-4l-2-3H2"),
		p("M5.45 5.11 2 12v6a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2v-6l-3.45-6.89A2 2 0 0 0 16.76 4H7.24a2 2 0 0 0-1.79 1.11z")],
	info: [["circle", { cx: 12, cy: 12, r: 10 }], p("M12 16v-4M12 8h.01")],
	mail: [["rect", { x: 2, y: 4, width: 20, height: 16, rx: 2 }], p("m22 6-10 7L2 6")],
	logout: [p("M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4"), p("m16 17 5-5-5-5M21 12H9")],
	mailopen: [p("M3 9.5 12 3l9 6.5V19a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"), p("m3 9.5 9 6 9-6")],
	folder: [p("M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z")],
	more: [["circle", { cx: 12, cy: 5, r: 1.2 }], ["circle", { cx: 12, cy: 12, r: 1.2 }], ["circle", { cx: 12, cy: 19, r: 1.2 }]],
	move: [p("M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z"), p("M8 13.5h7M12.5 10.5l3 3-3 3")],
	pencil: [p("M12 20h9"), p("M16.5 3.5a2.12 2.12 0 0 1 3 3L7 19l-4 1 1-4z")],
	plus: [p("M12 5v14M5 12h14")],
	menu: [p("M3 6h18M3 12h18M3 18h18")],
	refresh: [p("M23 4v6h-6"), p("M20.49 15a9 9 0 1 1-2.12-9.36L23 10")],
	replyall: [p("M7 14 2 9l5-5"), p("m12 14-5-5 5-5"), p("M22 20v-7a4 4 0 0 0-4-4H7")],
	restore: [p("M1 4v6h6"), p("M3.51 15a9 9 0 1 0 2.13-9.36L1 10")],
	reply: [p("M9 14 4 9l5-5"), p("M20 20v-7a4 4 0 0 0-4-4H4")],
	search: [["circle", { cx: 11, cy: 11, r: 7 }], p("m21 21-4.35-4.35")],
	send: [p("M22 2 11 13"), p("M22 2 15 22l-4-9-9-4 20-7z")],
	spam: [["polygon", { points: "7.86 2 16.14 2 22 7.86 22 16.14 16.14 22 7.86 22 2 16.14 2 7.86 7.86 2" }],
		p("M12 8v4M12 16h.01")],
	star: [["polygon", { points: "12 2 15.09 8.26 22 9.27 17 14.14 18.18 21.02 12 17.77 5.82 21.02 7 14.14 2 9.27 8.91 8.26 12 2" }]],
	sliders: [p("M4 21v-7M4 10V3M12 21v-9M12 8V3M20 21v-5M20 12V3M1 14h6M9 8h6M17 16h6")],
	trash: [p("M3 6h18"), p("m19 6-1 14a2 2 0 0 1-2 2H8a2 2 0 0 1-2-2L5 6"), p("M10 11v6M14 11v6"),
		p("M9 6V4a1 1 0 0 1 1-1h4a1 1 0 0 1 1 1v2")],
	users: [p("M17 21v-2a4 4 0 0 0-4-4H5a4 4 0 0 0-4 4v2"), ["circle", { cx: 9, cy: 7, r: 4 }],
		p("M23 21v-2a4 4 0 0 0-3-3.87M16 3.13a4 4 0 0 1 0 7.75")],
	x: [p("M18 6 6 18M6 6l12 12")]
};

// An icon element, hidden from assistive technology (the control it is in
// carries the accessible name)
export function icon(name, cls) {
	const svg = document.createElementNS(SVG, "svg");
	svg.setAttribute("class", cls ? "i " + cls : "i");
	svg.setAttribute("viewBox", "0 0 24 24");
	svg.setAttribute("aria-hidden", "true");
	svg.setAttribute("focusable", "false");
	for (const [tag, attrs] of ICONS[name] || []) {
		const el = document.createElementNS(SVG, tag);
		for (const [k, v] of Object.entries(attrs))
			el.setAttribute(k, String(v));
		svg.append(el);
	}
	return svg;
}
