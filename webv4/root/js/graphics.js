const CP437_UTF8 = {
    '\x80': '\u00c7', // LATIN CAPITAL LETTER C WITH CEDILLA
    '\x81': '\u00fc', // LATIN SMALL LETTER U WITH DIAERESIS
    '\x82': '\u00e9', // LATIN SMALL LETTER E WITH ACUTE
    '\x83': '\u00e2', // LATIN SMALL LETTER A WITH CIRCUMFLEX
    '\x84': '\u00e4', // LATIN SMALL LETTER A WITH DIAERESIS
    '\x85': '\u00e0', // LATIN SMALL LETTER A WITH GRAVE
    '\x86': '\u00e5', // LATIN SMALL LETTER A WITH RING ABOVE
    '\x87': '\u00e7', // LATIN SMALL LETTER C WITH CEDILLA
    '\x88': '\u00ea', // LATIN SMALL LETTER E WITH CIRCUMFLEX
    '\x89': '\u00eb', // LATIN SMALL LETTER E WITH DIAERESIS
    '\x8a': '\u00e8', // LATIN SMALL LETTER E WITH GRAVE
    '\x8b': '\u00ef', // LATIN SMALL LETTER I WITH DIAERESIS
    '\x8c': '\u00ee', // LATIN SMALL LETTER I WITH CIRCUMFLEX
    '\x8d': '\u00ec', // LATIN SMALL LETTER I WITH GRAVE
    '\x8e': '\u00c4', // LATIN CAPITAL LETTER A WITH DIAERESIS
    '\x8f': '\u00c5', // LATIN CAPITAL LETTER A WITH RING ABOVE
    '\x90': '\u00c9', // LATIN CAPITAL LETTER E WITH ACUTE
    '\x91': '\u00e6', // LATIN SMALL LIGATURE AE
    '\x92': '\u00c6', // LATIN CAPITAL LIGATURE AE
    '\x93': '\u00f4', // LATIN SMALL LETTER O WITH CIRCUMFLEX
    '\x94': '\u00f6', // LATIN SMALL LETTER O WITH DIAERESIS
    '\x95': '\u00f2', // LATIN SMALL LETTER O WITH GRAVE
    '\x96': '\u00fb', // LATIN SMALL LETTER U WITH CIRCUMFLEX
    '\x97': '\u00f9', // LATIN SMALL LETTER U WITH GRAVE
    '\x98': '\u00ff', // LATIN SMALL LETTER Y WITH DIAERESIS
    '\x99': '\u00d6', // LATIN CAPITAL LETTER O WITH DIAERESIS
    '\x9a': '\u00dc', // LATIN CAPITAL LETTER U WITH DIAERESIS
    '\x9b': '\u00a2', // CENT SIGN
    '\x9c': '\u00a3', // POUND SIGN
    '\x9d': '\u00a5', // YEN SIGN
    '\x9e': '\u20a7', // PESETA SIGN
    '\x9f': '\u0192', // LATIN SMALL LETTER F WITH HOOK
    '\xa0': '\u00e1', // LATIN SMALL LETTER A WITH ACUTE
    '\xa1': '\u00ed', // LATIN SMALL LETTER I WITH ACUTE
    '\xa2': '\u00f3', // LATIN SMALL LETTER O WITH ACUTE
    '\xa3': '\u00fa', // LATIN SMALL LETTER U WITH ACUTE
    '\xa4': '\u00f1', // LATIN SMALL LETTER N WITH TILDE
    '\xa5': '\u00d1', // LATIN CAPITAL LETTER N WITH TILDE
    '\xa6': '\u00aa', // FEMININE ORDINAL INDICATOR
    '\xa7': '\u00ba', // MASCULINE ORDINAL INDICATOR
    '\xa8': '\u00bf', // INVERTED QUESTION MARK
    '\xa9': '\u2310', // REVERSED NOT SIGN
    '\xaa': '\u00ac', // NOT SIGN
    '\xab': '\u00bd', // VULGAR FRACTION ONE HALF
    '\xac': '\u00bc', // VULGAR FRACTION ONE QUARTER
    '\xad': '\u00a1', // INVERTED EXCLAMATION MARK
    '\xae': '\u00ab', // LEFT-POINTING DOUBLE ANGLE QUOTATION MARK
    '\xaf': '\u00bb', // RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK
    '\xb0': '\u2591', // LIGHT SHADE
    '\xb1': '\u2592', // MEDIUM SHADE
    '\xb2': '\u2593', // DARK SHADE
    '\xb3': '\u2502', // BOX DRAWINGS LIGHT VERTICAL
    '\xb4': '\u2524', // BOX DRAWINGS LIGHT VERTICAL AND LEFT
    '\xb5': '\u2561', // BOX DRAWINGS VERTICAL SINGLE AND LEFT DOUBLE
    '\xb6': '\u2562', // BOX DRAWINGS VERTICAL DOUBLE AND LEFT SINGLE
    '\xb7': '\u2556', // BOX DRAWINGS DOWN DOUBLE AND LEFT SINGLE
    '\xb8': '\u2555', // BOX DRAWINGS DOWN SINGLE AND LEFT DOUBLE
    '\xb9': '\u2563', // BOX DRAWINGS DOUBLE VERTICAL AND LEFT
    '\xba': '\u2551', // BOX DRAWINGS DOUBLE VERTICAL
    '\xbb': '\u2557', // BOX DRAWINGS DOUBLE DOWN AND LEFT
    '\xbc': '\u255d', // BOX DRAWINGS DOUBLE UP AND LEFT
    '\xbd': '\u255c', // BOX DRAWINGS UP DOUBLE AND LEFT SINGLE
    '\xbe': '\u255b', // BOX DRAWINGS UP SINGLE AND LEFT DOUBLE
    '\xbf': '\u2510', // BOX DRAWINGS LIGHT DOWN AND LEFT
    '\xc0': '\u2514', // BOX DRAWINGS LIGHT UP AND RIGHT
    '\xc1': '\u2534', // BOX DRAWINGS LIGHT UP AND HORIZONTAL
    '\xc2': '\u252c', // BOX DRAWINGS LIGHT DOWN AND HORIZONTAL
    '\xc3': '\u251c', // BOX DRAWINGS LIGHT VERTICAL AND RIGHT
    '\xc4': '\u2500', // BOX DRAWINGS LIGHT HORIZONTAL
    '\xc5': '\u253c', // BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL
    '\xc6': '\u255e', // BOX DRAWINGS VERTICAL SINGLE AND RIGHT DOUBLE
    '\xc7': '\u255f', // BOX DRAWINGS VERTICAL DOUBLE AND RIGHT SINGLE
    '\xc8': '\u255a', // BOX DRAWINGS DOUBLE UP AND RIGHT
    '\xc9': '\u2554', // BOX DRAWINGS DOUBLE DOWN AND RIGHT
    '\xca': '\u2569', // BOX DRAWINGS DOUBLE UP AND HORIZONTAL
    '\xcb': '\u2566', // BOX DRAWINGS DOUBLE DOWN AND HORIZONTAL
    '\xcc': '\u2560', // BOX DRAWINGS DOUBLE VERTICAL AND RIGHT
    '\xcd': '\u2550', // BOX DRAWINGS DOUBLE HORIZONTAL
    '\xce': '\u256c', // BOX DRAWINGS DOUBLE VERTICAL AND HORIZONTAL
    '\xcf': '\u2567', // BOX DRAWINGS UP SINGLE AND HORIZONTAL DOUBLE
    '\xd0': '\u2568', // BOX DRAWINGS UP DOUBLE AND HORIZONTAL SINGLE
    '\xd1': '\u2564', // BOX DRAWINGS DOWN SINGLE AND HORIZONTAL DOUBLE
    '\xd2': '\u2565', // BOX DRAWINGS DOWN DOUBLE AND HORIZONTAL SINGLE
    '\xd3': '\u2559', // BOX DRAWINGS UP DOUBLE AND RIGHT SINGLE
    '\xd4': '\u2558', // BOX DRAWINGS UP SINGLE AND RIGHT DOUBLE
    '\xd5': '\u2552', // BOX DRAWINGS DOWN SINGLE AND RIGHT DOUBLE
    '\xd6': '\u2553', // BOX DRAWINGS DOWN DOUBLE AND RIGHT SINGLE
    '\xd7': '\u256b', // BOX DRAWINGS VERTICAL DOUBLE AND HORIZONTAL SINGLE
    '\xd8': '\u256a', // BOX DRAWINGS VERTICAL SINGLE AND HORIZONTAL DOUBLE
    '\xd9': '\u2518', // BOX DRAWINGS LIGHT UP AND LEFT
    '\xda': '\u250c', // BOX DRAWINGS LIGHT DOWN AND RIGHT
    '\xdb': '\u2588', // FULL BLOCK
    '\xdc': '\u2584', // LOWER HALF BLOCK
    '\xdd': '\u258c', // LEFT HALF BLOCK
    '\xde': '\u2590', // RIGHT HALF BLOCK
    '\xdf': '\u2580', // UPPER HALF BLOCK
    '\xe0': '\u03b1', // GREEK SMALL LETTER ALPHA
    '\xe1': '\u00df', // LATIN SMALL LETTER SHARP S
    '\xe2': '\u0393', // GREEK CAPITAL LETTER GAMMA
    '\xe3': '\u03c0', // GREEK SMALL LETTER PI
    '\xe4': '\u03a3', // GREEK CAPITAL LETTER SIGMA
    '\xe5': '\u03c3', // GREEK SMALL LETTER SIGMA
    '\xe6': '\u00b5', // MICRO SIGN
    '\xe7': '\u03c4', // GREEK SMALL LETTER TAU
    '\xe8': '\u03a6', // GREEK CAPITAL LETTER PHI
    '\xe9': '\u0398', // GREEK CAPITAL LETTER THETA
    '\xea': '\u03a9', // GREEK CAPITAL LETTER OMEGA
    '\xeb': '\u03b4', // GREEK SMALL LETTER DELTA
    '\xec': '\u221e', // INFINITY
    '\xed': '\u03c6', // GREEK SMALL LETTER PHI
    '\xee': '\u03b5', // GREEK SMALL LETTER EPSILON
    '\xef': '\u2229', // INTERSECTION
    '\xf0': '\u2261', // IDENTICAL TO
    '\xf1': '\u00b1', // PLUS-MINUS SIGN
    '\xf2': '\u2265', // GREATER-THAN OR EQUAL TO
    '\xf3': '\u2264', // LESS-THAN OR EQUAL TO
    '\xf4': '\u2320', // TOP HALF INTEGRAL
    '\xf5': '\u2321', // BOTTOM HALF INTEGRAL
    '\xf6': '\u00f7', // DIVISION SIGN
    '\xf7': '\u2248', // ALMOST EQUAL TO
    '\xf8': '\u00b0', // DEGREE SIGN
    '\xf9': '\u2219', // BULLET OPERATOR
    '\xfa': '\u00b7', // MIDDLE DOT
    '\xfb': '\u221a', // SQUARE ROOT
    '\xfc': '\u207f', // SUPERSCRIPT LATIN SMALL LETTER N
    '\xfd': '\u00b2', // SUPERSCRIPT TWO
    '\xfe': '\u25a0', // BLACK SQUARE
    '\xff': '\u00a0', // NO-BREAK SPACE
};

const ANSI_Colors = [
    "#000000", // Black
    "#A80000", // Red
    "#00A800", // Green
    "#A85400", // Brown
    "#0000A8", // Blue
    "#A800A8", // Magenta
    "#00A8A8", // Cyan
    "#A8A8A8", // Light Grey
    "#545454", // Dark Grey (High Black)
    "#FC5454", // Light Red
    "#54FC54", // Light Green
    "#FCFC54", // Yellow (High Brown)
    "#5454FC", // Light Blue
    "#FC54FC", // Light Magenta
    "#54FCFC", // Light Cyan
    "#FFFFFF"  // White
];
// Index into ANSI_Colors
const CGA_Colors = [
    0,
    4,
    2,
    6,
    1,
    5,
    3,
    7,
    8,
    12,
    10,
    14,
    9,
    13,
    11,
    15
];

function parse(body) {

    let i;
    let x = 0;
    let y = 0;
    let _x = 0;
    let _y = 0;
    let fg = 7;
    let bg = 0;
    let high = 0;
    let move;
    let match;
    let opts;
    let data = [[]];
    const lifo = [];
    const re = /^((?<ansi>\u001b\[((?:[\x30-\x3f]{0,2};?)*)([\x20-\x2f]*)([\x40-\x7c]))|(\x01(?<ctrl_a>[KRGYBMCW0-7HN\-LJ<>\[\]\/\+]))|(@(?<pcboard_bg>[a-fA-F0-9])(?<pcboard_fg>[a-fA-F0-9])@{0,1})|(\|(?<pipe>\d\d))|(\x03(?<wwiv>[0-9]))|(\|(?<celerity>[kbgcrmywdS])))/i;

    const Codes = {
        ctrl_a: {
            K: { fg: 0 },
            R: { fg: 1 },
            G: { fg: 2 },
            Y: { fg: 3 },
            B: { fg: 4 },
            M: { fg: 5 },
            C: { fg: 6 },
            W: { fg: 7 },
            0: { bg: 0 },
            1: { bg: 1 },
            2: { bg: 2 },
            3: { bg: 3 },
            4: { bg: 4 },
            5: { bg: 5 },
            6: { bg: 6 },
            7: { bg: 7 },
            H: { high: 1 },
            N: { high: 0 },
            '-': { // optimized normal
                handler: () => {
                    if (lifo.length) {
                        const attr = lifo.pop();
                        fg = attr.fg;
                        bg = attr.bg;
                        high = attr.high;
                    } else {
                        high = 0;
                    }
                }
            },
            L: { // Clear the screen
                handler: () => data = [[]],
            },
            J: { // Clear to end of screen but keep cursor in place
                handler: () => {
                    for (let yy = y; yy < data.length; yy++) {
                        if (data[yy] === undefined) continue;
                        for (let xx = x + 1; xx < data[yy].length; xx++) {
                            if (data[yy][xx] === undefined) continue;
                            data[yy][xx] = { c: ' ', fg: fg + (high ? 8 : 0), bg }; // Could just make these undefined I guess
                        }
                    }
                },
            },
            '>': { // Clear to end of line but keep cursor in place
                handler: () => {
                    for (let xx = x; xx < data[y].length; xx++) {
                        if (data[y][xx] === undefined) continue;
                        data[y][xx] = { c: ' ', fg: fg + (high ? 8 : 0), bg }; // Could just make these undefined I guess
                    }
                },
            },
            '<': { // Cursor left
                handler: () => {
                    if (x > 0) {
                        x--;
                    } else if (y > 0) { // Not sure if this is what would happen on terminal side; must test.
                        x = 79;
                        y--;
                    }
                },
            },
            "'": {
                handler: () => {
                    x = 0;
                    y = 0;
                },
            },
            '[': { handler: () => x = 0 }, // CR
            ']': { // LF
                handler: () => {
                    y++;
                    if (data[y] === undefined) data[y] = [];
                },
            }, 
            '/': { // Conditional newline - Send a new-line sequence (CRLF) only when the cursor is not already in the first column
                handler: () => {
                    if (x < 1) return;
                    this['['].handler();
                    this[']'].handler();
                },
            }, 
            '+': { handler: () => lifo.push({ fg, bg, high }) },
            // '_': {}, // optimized normal, but only if blinking or (high?) background is set
            // I: {}, // blink
            // E: {}, // bright bg (ice)
            // f: {}, // blink font
            // F: {}, // high blink font
            // D: { // current date in mm/dd/yy or dd/mm/yy
            //     handler: () => {},
            // },
            // T: { // current system time in hh:mm am/pm or hh:mm:ss format
            //     handler: () => {},
            // },
            // '"': {}, // Display a file
        },
        pcboard_bg: {
            0: { bg: CGA_Colors[0] },
            1: { bg: CGA_Colors[1] },
            2: { bg: CGA_Colors[2] },
            3: { bg: CGA_Colors[3] },
            4: { bg: CGA_Colors[4] },
            5: { bg: CGA_Colors[5] },
            6: { bg: CGA_Colors[6] },
            7: { bg: CGA_Colors[7] },
            // 8-F are blinking fg
        },
        pcboard_fg: {
            0: {
                fg: 0,
                high: 0,
            },
            1: {
                fg: CGA_Colors[1],
                high: 0,
            },
            2: {
                fg: CGA_Colors[2],
                high: 0,
            },
            3: {
                fg: CGA_Colors[3],
                high: 0,
            },
            4: {
                fg: CGA_Colors[4],
                high: 0,
            },
            5: {
                fg: CGA_Colors[5],
                high: 0,
            },
            6: {
                fg: CGA_Colors[6],
                high: 0,
            },
            7: {
                fg: CGA_Colors[7],
                high: 0,
            },
            8: {
                fg: CGA_Colors[0],
                high: 1,
            },
            9: {
                fg: CGA_Colors[1],
                high: 1,
            },
            A: {
                fg: CGA_Colors[2],
                high: 1,
            },
            B: {
                fg: CGA_Colors[3],
                high: 1,
            },
            C: {
                fg: CGA_Colors[4],
                high: 1,
            },
            D: {
                fg: CGA_Colors[5],
                high: 1,
            },
            E: {
                fg: CGA_Colors[6],
                high: 1,
            },
            F: {
                fg: CGA_Colors[7],
                high: 1,
            },
        },
        pipe: {
            00: {
                fg: CGA_Colors[0],
                high: 0,
            },
            01: {
                fg: CGA_Colors[1],
                high: 0,
            },
            02: {
                fg: CGA_Colors[2],
                high: 0,
            },
            03: {
                fg: CGA_Colors[3],
                high: 0,
            },
            04: {
                fg: CGA_Colors[4],
                high: 0,
            },
            05: {
                fg: CGA_Colors[5],
                high: 0,
            },
            06: {
                fg: CGA_Colors[6],
                high: 0,
            },
            07: {
                fg: CGA_Colors[7],
                high: 0,
            },
            08: {
                fg: CGA_Colors[0],
                high: 1,
            },
            09: {
                fg: CGA_Colors[1],
                high: 1,
            },
            10: {
                fg: CGA_Colors[2],
                high: 1,
            },
            11: {
                fg: CGA_Colors[3],
                high: 1,
            },
            12: {
                fg: CGA_Colors[4],
                high: 1,
            },
            13: {
                fg: CGA_Colors[5],
                high: 1,
            },
            14: {
                fg: CGA_Colors[6],
                high: 1,
            },
            15: {
                fg: CGA_Colors[7],
                high: 1,
            },
            16: {
                bg: CGA_Colors[0],
                high: 0,
            },
            17: {
                bg: CGA_Colors[1],
                high: 0,
            },
            18: {
                bg: CGA_Colors[2],
                high: 0,
            },
            19: {
                bg: CGA_Colors[3],
                high: 0,
            },
            20: {
                bg: CGA_Colors[4],
                high: 0,
            },
            21: {
                bg: CGA_Colors[5],
                high: 0,
            },
            22: {
                bg: CGA_Colors[6],
                high: 0,
            },
            23: {
                bg: CGA_Colors[7],
                high: 0,
            },
            // 24 - 31 are ice bg or blinking fg
        },
        wwiv: {
            0: {
                fg: 7,
                bg: 0,
                high: 0,
            },
            1: {
                fg: 6,
                bg: 0,
                high: 1,
            },
            2: {
                fg: 3,
                bg: 0,
                high: 1,
            },
            3: {
                fg: 5,
                bg: 0,
                high: 0,
            },
            4: {
                fg: 7,
                bg: 4,
                high: 1,
            },
            5: {
                fg: 2,
                bg: 0,
                high: 0,
            },
            6: { // Supposed to blink, but whatever.
                fg: 2,
                bg: 0,
                high: 1,
            },
            7: {
                fg: 4,
                bg: 0,
                high: 1,
            },
            8: {
                fg: 4,
                bg: 0,
                high: 0,
            },
            9: {
                fg: 6,
                bg: 0,
                high: 0,
            },
        },
        celerity: {
            k: {
                fg: CGA_Colors[0],
                high: 0,
            },
            b: {
                fg: CGA_Colors[1],
                high: 0,
            },
            g: {
                fg: CGA_Colors[2],
                high: 0,
            },
            c: {
                fg: CGA_Colors[3],
                high: 0,
            },
            r: {
                fg: CGA_Colors[4],
                high: 0,
            },
            m: {
                fg: CGA_Colors[5],
                high: 0,
            },
            y: {
                fg: CGA_Colors[6],
                high: 0,
            },
            w: {
                fg: CGA_Colors[7],
                high: 0,
            },
            d: {
                fg: CGA_Colors[0],
                high: 1,
            },
            B: {
                fg: CGA_Colors[1],
                high: 1,
            },
            G: {
                fg: CGA_Colors[2],
                high: 1,
            },
            C: {
                fg: CGA_Colors[3],
                high: 1,
            },
            R: {
                fg: CGA_Colors[4],
                high: 1,
            },
            M: {
                fg: CGA_Colors[5],
                high: 1,
            },
            Y: {
                fg: CGA_Colors[6],
                high: 1,
            },
            W: {
                fg: CGA_Colors[7],
                high: 1,
            },
            S: {
                handler: () => {
                    const _fg = fg;
                    fg = bg;
                    bg = _fg;
                },
            },
        },
    };
    Object.values(Codes).forEach(v => {
        Object.keys(v).forEach(k => {
            if (v[k.toUpperCase()] === undefined) v[k.toUpperCase()] = v[k];
            if (v[k.toLowerCase()] === undefined) v[k.toLowerCase()] = v[k];
        });
    });

    while (body.length) {
        match = re.exec(body);
        if (match !== null) {
            body = body.substr(match[0].length);
            if (match.groups.ansi !== undefined) { // To do: mostly not dealing with illegal/invalid sequences yet
                if (match[3] === '') {
                    opts = [];
                } else {
                    opts = match[3].split(';').map(e => {
                        const i = parseInt(e, 10);
                        return isNaN(i) ? e : i;
                    });
                }
                switch (match[5]) {
                    case '@': // Insert Character(s)
                        if (data[y] !== undefined) {
                            move = data[y].splice(x);
                            let s = isNaN(opts[0]) ? 1 : opts[0];
                            data[y].splice(x + n, 0, ...([].fill({ c: ' ', fg: fg + (high ? 8 : 0), bg }, 0, s - 1)));
                            data[y].splice(s, 0, ...move.slice(0, 79 - s)); // To do: hard-coded to 80 cols
                            x = data[y].length - 1; // To do: where is the cursor supposed to be now?
                        }
                        break;
                    case 'A': // Cursor up
                        y = Math.max(y - (isNaN(opts[0]) ? 1 : opts[0]), 0);
                        break;
                    case 'B': // Cursor down
                        y += (isNaN(opts[0]) ? 1 : opts[0]);
                        if (data[y] === undefined) data[y] = [];
                        break;
                    case 'C': // Cursor right
                        x = Math.min(x + (isNaN(opts[0]) ? 1 : opts[0]), 79);
                        break;
                    case 'D': // Cursor left
                        x = Math.max(x - (isNaN(opts[0]) ? 1 : opts[0]), 0);
                        break;
                    case 'f': // Cursor position
                    case 'H':
                        y = isNaN(opts[0]) ? 1 : opts[0];
                        x = isNaN(opts[1]) ? 1 : opts[0];
                        if (data[y] === undefined) data[y] = [];
                        break;
                    case 'J': // Erase in page
                        if (!opts.length || opts[0] === 0) { // Erase from the current position to the end of the screen.
                            if (data[yy] !== undefined) data[yy].splice(xx);
                            data.splice(yy + 1);
                        } else if (opts[0] === 1) { // Erase from the current position to the start of the screen.
                            if (data[y] !== undefined) data[y].splice(0, x + 1, ...([].fill(undefined, 0, x)));
                            data.splice(0, y, ...([].fill(undefined, 0, y - 1)));
                        } else if (opts[0] == 2) { // Erase entire screen.
                            data = [[]];
                            // To do:
                            // "As a violation of ECMA-048, also moves the cursor to position 1/1 as a number of BBS programs assume this behaviour."
                            // http://www.ansi-bbs.org/ansi-bbs-core-server.html
                            x = 0;
                            y = 0;
                        }
                        break;
                    case 'K': // Erase in Line
                        if (data[y] !== undefined) {
                            if (!opts.length || opts[0] === 0) { // Erase from the current position to the end of the line.
                                data[y].splice(x);
                            } else if (opts[0] === 1) { // Erase from the current position to the start of the line.
                                data[y].splice(0, x, ...([].fill(undefined, 0, x)));
                            } else if (opts[0] === 2) { // Erase entire line.
                                data.splice(y, 1, undefined);
                            }
                        }
                        break;
                    case 'L': // Insert line(s)
                        i = isNaN(opts[0]) ? 1 : opts[0];
                        move = data.splice(y, data.length - y, ...([].fill({ c: ' ', fg: fg + (high ? 8 : 0), bg }, 0, i)));
                        data = data.concat(move);
                        break;
                    case 'M': // Delete line(s)
                        i = isNaN(opts[0]) ? 1 : opts[0];
                        data = data.splice(y, i);
                        data = data.concat([].fill({ c: ' ', fg: fg + (high ? 8 : 0 ), bg }, 0, i));
                        break;
                    case 'm':
                        for (let o of opts) {
                            if (isNaN(o)) continue;
                            if (o == 0) {
                                fg = 7;
                                bg = 0;
                                high = 0;
                            } else if (o == 1) {
                                high = 1;
                            } else if (o == 5) {
                                // blink
                            } else if (o >= 30 && o <= 37) {
                                fg = o - 30;
                            } else if (o >= 40 && o <= 47) {
                                bg = o - 40;
                            }
                        }
                        break;
                    case 'n': // Device Status Report
                        // Defaults: opts[0] = 0
                        // A request for a status report. ANSI-BBS terminals should handle at least the following values for p1
                        // Request active cursor position
                        // terminal must reply with CSIp1;p2R where p1 is the current line number counting from one, and p2 is the current column.
                        // NOTE: This sequences should not be present in saved ANSI-BBS files
                        // Source: http://www.ecma-international.org/publications/files/ECMA-ST/Ecma-048.pdf
                        break;
                    case 'P': // Delete character
                        break;
                    case 'S': // Scroll up
                        i = isNaN(opts[0]) ? 1 : opts[0];
                        data.splice(0, i);
                        for (let n = 0; n < i; n++) {
                            data.push([].fill({ c: ' ', fg: fg + (high ? 8 : 0), bg }, 0, 79));
                        }
                        break;
                    case 's': // push xy
                        _x = x;
                        _y = y;
                        break;
                    case 'T': // Scroll down
                        i = isNaN(opts[0]) ? 1 : opts[0];
                        for (let n = 0; n < i; n++) {
                            data.unshift([].fill({ c: ' ', fg: fg + (high ? 8 : 0), bg }, 0, 79));
                        }
                        break;
                    case 'u': // pop xy
                        x = _x;
                        y = _y;
                        break;
                    case 'X': // Erase character
                        if (data[y] !== undefined) {
                            i = isNaN(opts[0]) ? 1 : opts[0];
                            data[y].splice(x, i, ...([].fill({ c: ' ', fg: fg + (high ? 8 : 0), bg }, 0, i)));
                        }
                        break;
                    case 'Z': // Cursor backward tabulation
                        break;
                    default:
                        // Unknown or unimplemented command
                        break;
                }
            } else {
                Object.keys(match.groups).forEach(e => {
                    if (match.groups[e] === undefined) return;
                    if (Codes[e] === undefined || Codes[e][match.groups[e]] === undefined) return;
                    if (Codes[e][match.groups[e]].fg !== undefined) fg = Codes[e][match.groups[e]].fg;
                    if (Codes[e][match.groups[e]].bg !== undefined) bg = Codes[e][match.groups[e]].bg;
                    if (Codes[e][match.groups[e]].high !== undefined) high = Codes[e][match.groups[e]].high;
                    if (Codes[e][match.groups[e]].handler !== undefined) Codes[e][match.groups[e]].handler();
                });
            }
        } else {
            let ch = body.substr(0, 1);
            switch (ch) {
                case '\x00':
                case '\x07':
                    break;
                case '\x0C':
                    data = [[]];
                    x = 0;
                    y = 0;
                    break;
                case '\b':
                    if (x > 0) x--;
                    break;
                case '\n':
                    y++;
                    if (data[y] === undefined) data[y] = [];
                    break;
                case '\r':
                    x = 0;
                    break;
                default:
                    data[y][x] = { c: ch, fg: fg + (high ? 8 : 0), bg };
                    x++;
                    if (x > 79) {
                        x = 0;
                        y++;
                        if (data[y] === undefined) data[y] = [];
                    }
                    break;
            }
            body = body.substr(1);
        }
    }

    return data;

}

function toHTML(data) {

    const pre = document.createElement('div');
    pre.classList.add('bbs-view');
    let ofg;
    let obg;
    let span;
    for (let y = 0; y < data.length; y++) {
        if (data[y] === undefined) continue;
        for (let x = 0; x < data[y].length; x++) {
            if (data[y][x]) {
                if (!span || data[y][x].fg != ofg || data[y][x].bg != obg) {
                    ofg = data[y][x].fg;
                    obg = data[y][x].bg;
                    span = document.createElement('span');
                    span.style.setProperty('color', ANSI_Colors[data[y][x].fg]);
                    span.style.setProperty('background-color', ANSI_Colors[data[y][x].bg]);
                    pre.appendChild(span);
                }
                span.innerText += data[y][x].c;
            } else {
                if (!span || ofg !== 7 || obg !== 0) {
                    span = document.createElement('span');
                    span.style.setProperty('color', ANSI_Colors[7]);
                    span.style.setProperty('background-color', ANSI_Colors[0]);
                    pre.appendChild(span);
                }
                span.innerText += ' ';
            }
        }
        span = null;
        pre.innerHTML += '\r\n';
    }
    return pre;

}