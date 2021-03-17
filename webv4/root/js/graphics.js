const Graphics = (() => {

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

    // Value at index of a CGA colour in this array is index in ANSI_Colors
    const CGA_Colors = [
        0,  // Black
        4,  // Blue
        2,  // Green
        6,  // Cyan
        1,  // Red
        5,  // Magenta
        3,  // Brown
        7,  // Light Grey
        8,  // Dark Grey
        12, // Light Blue
        10, // Light Green
        14, // Light Cyan
        9,  // Light Red
        13, // Light Magenta
        11, // Yellow
        15  // White
    ];

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

    // Not sure what's worse - including this in the file, or computing it from CP437_UTF8; we'll go with this for now.
    const UTF8_CP437 = {
        '\u00c7': '\x80', // LATIN CAPITAL LETTER C WITH CEDILLA
        '\u00fc': '\x81', // LATIN SMALL LETTER U WITH DIAERESIS
        '\u00e9': '\x82', // LATIN SMALL LETTER E WITH ACUTE
        '\u00e2': '\x83', // LATIN SMALL LETTER A WITH CIRCUMFLEX
        '\u00e4': '\x84', // LATIN SMALL LETTER A WITH DIAERESIS
        '\u00e0': '\x85', // LATIN SMALL LETTER A WITH GRAVE
        '\u00e5': '\x86', // LATIN SMALL LETTER A WITH RING ABOVE
        '\u00e7': '\x87', // LATIN SMALL LETTER C WITH CEDILLA
        '\u00ea': '\x88', // LATIN SMALL LETTER E WITH CIRCUMFLEX
        '\u00eb': '\x89', // LATIN SMALL LETTER E WITH DIAERESIS
        '\u00e8': '\x8a', // LATIN SMALL LETTER E WITH GRAVE
        '\u00ef': '\x8b', // LATIN SMALL LETTER I WITH DIAERESIS
        '\u00ee': '\x8c', // LATIN SMALL LETTER I WITH CIRCUMFLEX
        '\u00ec': '\x8d', // LATIN SMALL LETTER I WITH GRAVE
        '\u00c4': '\x8e', // LATIN CAPITAL LETTER A WITH DIAERESIS
        '\u00c5': '\x8f', // LATIN CAPITAL LETTER A WITH RING ABOVE
        '\u00c9': '\x90', // LATIN CAPITAL LETTER E WITH ACUTE
        '\u00e6': '\x91', // LATIN SMALL LIGATURE AE
        '\u00c6': '\x92', // LATIN CAPITAL LIGATURE AE
        '\u00f4': '\x93', // LATIN SMALL LETTER O WITH CIRCUMFLEX
        '\u00f6': '\x94', // LATIN SMALL LETTER O WITH DIAERESIS
        '\u00f2': '\x95', // LATIN SMALL LETTER O WITH GRAVE
        '\u00fb': '\x96', // LATIN SMALL LETTER U WITH CIRCUMFLEX
        '\u00f9': '\x97', // LATIN SMALL LETTER U WITH GRAVE
        '\u00ff': '\x98', // LATIN SMALL LETTER Y WITH DIAERESIS
        '\u00d6': '\x99', // LATIN CAPITAL LETTER O WITH DIAERESIS
        '\u00dc': '\x9a', // LATIN CAPITAL LETTER U WITH DIAERESIS
        '\u00a2': '\x9b', // CENT SIGN
        '\u00a3': '\x9c', // POUND SIGN
        '\u00a5': '\x9d', // YEN SIGN
        '\u20a7': '\x9e', // PESETA SIGN
        '\u0192': '\x9f', // LATIN SMALL LETTER F WITH HOOK
        '\u00e1': '\xa0', // LATIN SMALL LETTER A WITH ACUTE
        '\u00ed': '\xa1', // LATIN SMALL LETTER I WITH ACUTE
        '\u00f3': '\xa2', // LATIN SMALL LETTER O WITH ACUTE
        '\u00fa': '\xa3', // LATIN SMALL LETTER U WITH ACUTE
        '\u00f1': '\xa4', // LATIN SMALL LETTER N WITH TILDE
        '\u00d1': '\xa5', // LATIN CAPITAL LETTER N WITH TILDE
        '\u00aa': '\xa6', // FEMININE ORDINAL INDICATOR
        '\u00ba': '\xa7', // MASCULINE ORDINAL INDICATOR
        '\u00bf': '\xa8', // INVERTED QUESTION MARK
        '\u2310': '\xa9', // REVERSED NOT SIGN
        '\u00ac': '\xaa', // NOT SIGN
        '\u00bd': '\xab', // VULGAR FRACTION ONE HALF
        '\u00bc': '\xac', // VULGAR FRACTION ONE QUARTER
        '\u00a1': '\xad', // INVERTED EXCLAMATION MARK
        '\u00ab': '\xae', // LEFT-POINTING DOUBLE ANGLE QUOTATION MARK
        '\u00bb': '\xaf', // RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK
        '\u2591': '\xb0', // LIGHT SHADE
        '\u2592': '\xb1', // MEDIUM SHADE
        '\u2593': '\xb2', // DARK SHADE
        '\u2502': '\xb3', // BOX DRAWINGS LIGHT VERTICAL
        '\u2524': '\xb4', // BOX DRAWINGS LIGHT VERTICAL AND LEFT
        '\u2561': '\xb5', // BOX DRAWINGS VERTICAL SINGLE AND LEFT DOUBLE
        '\u2562': '\xb6', // BOX DRAWINGS VERTICAL DOUBLE AND LEFT SINGLE
        '\u2556': '\xb7', // BOX DRAWINGS DOWN DOUBLE AND LEFT SINGLE
        '\u2555': '\xb8', // BOX DRAWINGS DOWN SINGLE AND LEFT DOUBLE
        '\u2563': '\xb9', // BOX DRAWINGS DOUBLE VERTICAL AND LEFT
        '\u2551': '\xba', // BOX DRAWINGS DOUBLE VERTICAL
        '\u2557': '\xbb', // BOX DRAWINGS DOUBLE DOWN AND LEFT
        '\u255d': '\xbc', // BOX DRAWINGS DOUBLE UP AND LEFT
        '\u255c': '\xbd', // BOX DRAWINGS UP DOUBLE AND LEFT SINGLE
        '\u255b': '\xbe', // BOX DRAWINGS UP SINGLE AND LEFT DOUBLE
        '\u2510': '\xbf', // BOX DRAWINGS LIGHT DOWN AND LEFT
        '\u2514': '\xc0', // BOX DRAWINGS LIGHT UP AND RIGHT
        '\u2534': '\xc1', // BOX DRAWINGS LIGHT UP AND HORIZONTAL
        '\u252c': '\xc2', // BOX DRAWINGS LIGHT DOWN AND HORIZONTAL
        '\u251c': '\xc3', // BOX DRAWINGS LIGHT VERTICAL AND RIGHT
        '\u2500': '\xc4', // BOX DRAWINGS LIGHT HORIZONTAL
        '\u253c': '\xc5', // BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL
        '\u255e': '\xc6', // BOX DRAWINGS VERTICAL SINGLE AND RIGHT DOUBLE
        '\u255f': '\xc7', // BOX DRAWINGS VERTICAL DOUBLE AND RIGHT SINGLE
        '\u255a': '\xc8', // BOX DRAWINGS DOUBLE UP AND RIGHT
        '\u2554': '\xc9', // BOX DRAWINGS DOUBLE DOWN AND RIGHT
        '\u2569': '\xca', // BOX DRAWINGS DOUBLE UP AND HORIZONTAL
        '\u2566': '\xcb', // BOX DRAWINGS DOUBLE DOWN AND HORIZONTAL
        '\u2560': '\xcc', // BOX DRAWINGS DOUBLE VERTICAL AND RIGHT
        '\u2550': '\xcd', // BOX DRAWINGS DOUBLE HORIZONTAL
        '\u256c': '\xce', // BOX DRAWINGS DOUBLE VERTICAL AND HORIZONTAL
        '\u2567': '\xcf', // BOX DRAWINGS UP SINGLE AND HORIZONTAL DOUBLE
        '\u2568': '\xd0', // BOX DRAWINGS UP DOUBLE AND HORIZONTAL SINGLE
        '\u2564': '\xd1', // BOX DRAWINGS DOWN SINGLE AND HORIZONTAL DOUBLE
        '\u2565': '\xd2', // BOX DRAWINGS DOWN DOUBLE AND HORIZONTAL SINGLE
        '\u2559': '\xd3', // BOX DRAWINGS UP DOUBLE AND RIGHT SINGLE
        '\u2558': '\xd4', // BOX DRAWINGS UP SINGLE AND RIGHT DOUBLE
        '\u2552': '\xd5', // BOX DRAWINGS DOWN SINGLE AND RIGHT DOUBLE
        '\u2553': '\xd6', // BOX DRAWINGS DOWN DOUBLE AND RIGHT SINGLE
        '\u256b': '\xd7', // BOX DRAWINGS VERTICAL DOUBLE AND HORIZONTAL SINGLE
        '\u256a': '\xd8', // BOX DRAWINGS VERTICAL SINGLE AND HORIZONTAL DOUBLE
        '\u2518': '\xd9', // BOX DRAWINGS LIGHT UP AND LEFT
        '\u250c': '\xda', // BOX DRAWINGS LIGHT DOWN AND RIGHT
        '\u2588': '\xdb', // FULL BLOCK
        '\u2584': '\xdc', // LOWER HALF BLOCK
        '\u258c': '\xdd', // LEFT HALF BLOCK
        '\u2590': '\xde', // RIGHT HALF BLOCK
        '\u2580': '\xdf', // UPPER HALF BLOCK
        '\u03b1': '\xe0', // GREEK SMALL LETTER ALPHA
        '\u00df': '\xe1', // LATIN SMALL LETTER SHARP S
        '\u0393': '\xe2', // GREEK CAPITAL LETTER GAMMA
        '\u03c0': '\xe3', // GREEK SMALL LETTER PI
        '\u03a3': '\xe4', // GREEK CAPITAL LETTER SIGMA
        '\u03c3': '\xe5', // GREEK SMALL LETTER SIGMA
        '\u00b5': '\xe6', // MICRO SIGN
        '\u03c4': '\xe7', // GREEK SMALL LETTER TAU
        '\u03a6': '\xe8', // GREEK CAPITAL LETTER PHI
        '\u0398': '\xe9', // GREEK CAPITAL LETTER THETA
        '\u03a9': '\xea', // GREEK CAPITAL LETTER OMEGA
        '\u03b4': '\xeb', // GREEK SMALL LETTER DELTA
        '\u221e': '\xec', // INFINITY
        '\u03c6': '\xed', // GREEK SMALL LETTER PHI
        '\u03b5': '\xee', // GREEK SMALL LETTER EPSILON
        '\u2229': '\xef', // INTERSECTION
        '\u2261': '\xf0', // IDENTICAL TO
        '\u00b1': '\xf1', // PLUS-MINUS SIGN
        '\u2265': '\xf2', // GREATER-THAN OR EQUAL TO
        '\u2264': '\xf3', // LESS-THAN OR EQUAL TO
        '\u2320': '\xf4', // TOP HALF INTEGRAL
        '\u2321': '\xf5', // BOTTOM HALF INTEGRAL
        '\u00f7': '\xf6', // DIVISION SIGN
        '\u2248': '\xf7', // ALMOST EQUAL TO
        '\u00b0': '\xf8', // DEGREE SIGN
        '\u2219': '\xf9', // BULLET OPERATOR
        '\u00b7': '\xfa', // MIDDLE DOT
        '\u221a': '\xfb', // SQUARE ROOT
        '\u207f': '\xfc', // SUPERSCRIPT LATIN SMALL LETTER N
        '\u00b2': '\xfd', // SUPERSCRIPT TWO
        '\u25a0': '\xfe', // BLACK SQUARE
        '\u00a0': '\xff', // NO-BREAK SPACE
        '\uffed': '\xfe', // MIDDLE BLOCK
    };

    const Font = 'data:image/png;base64,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';
    const FontColumns = 64;
    const FontRows = 4;
    const FontCellWidth = 8;
    const FontCellHeight = 16;

    function parseText(text) {

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

        while (text.length) {
            match = re.exec(text);
            if (match !== null) {
                text = text.substr(match[0].length);
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
                                data[y].splice(x + n, 0, ...((new Array(s)).fill({ c: ' ', fg: fg + (high ? 8 : 0), bg }, 0, s - 1)));
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
                                if (data[y] !== undefined) data[y].splice(0, x + 1, ...((new Array(x + 1)).fill(undefined, 0, x)));
                                data.splice(0, y, ...((new Array(y)).fill(undefined, 0, y - 1)));
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
                                    data[y].splice(0, x, ...((new Array(x + 1)).fill(undefined, 0, x)));
                                } else if (opts[0] === 2) { // Erase entire line.
                                    data.splice(y, 1, undefined);
                                }
                            }
                            break;
                        case 'L': // Insert line(s)
                            i = isNaN(opts[0]) ? 1 : opts[0];
                            move = data.splice(y, data.length - y, ...((new Array(i + 1)).fill({ c: ' ', fg: fg + (high ? 8 : 0), bg }, 0, i)));
                            data = data.concat(move);
                            break;
                        case 'M': // Delete line(s)
                            i = isNaN(opts[0]) ? 1 : opts[0];
                            data = data.splice(y, i);
                            data = data.concat((new Array(i + 1)).fill({ c: ' ', fg: fg + (high ? 8 : 0 ), bg }, 0, i));
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
                                data.push((new Array(80)).fill({ c: ' ', fg: fg + (high ? 8 : 0), bg }, 0, 79));
                            }
                            break;
                        case 's': // push xy
                            _x = x;
                            _y = y;
                            break;
                        case 'T': // Scroll down
                            i = isNaN(opts[0]) ? 1 : opts[0];
                            for (let n = 0; n < i; n++) {
                                data.unshift((new Array(80)).fill({ c: ' ', fg: fg + (high ? 8 : 0), bg }, 0, 79));
                            }
                            break;
                        case 'u': // pop xy
                            x = _x;
                            y = _y;
                            break;
                        case 'X': // Erase character
                            if (data[y] !== undefined) {
                                i = isNaN(opts[0]) ? 1 : opts[0];
                                data[y].splice(x, i, ...((new Array(i + 1)).fill({ c: ' ', fg: fg + (high ? 8 : 0), bg }, 0, i)));
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
                let ch = text.substr(0, 1);
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
                text = text.substr(1);
            }
        }

        return data;

    }

    function getWorkspace(cols, rows) {

        const container = document.createElement('div');

        const canvas = document.createElement('canvas');
        const ctx = canvas.getContext('2d');
        ctx.canvas.width = cols * FontCellWidth;
        ctx.canvas.height = rows * FontCellHeight;
        ctx.fillStyle = ANSI_Colors[0];
        ctx.fillRect(0, 0, ctx.canvas.width, ctx.canvas.height);

        const fontCanvas = document.createElement('canvas');
        const fontCtx = fontCanvas.getContext('2d');
        fontCtx.canvas.width = FontColumns * FontCellWidth;
        fontCtx.canvas.height = FontRows * FontCellHeight;
        fontCanvas.style.display = 'none';

        container.appendChild(canvas);
        container.appendChild(fontCanvas);

        const img = new Image();
        return new Promise(res => {
            img.addEventListener('load', () => {
                fontCtx.drawImage(img, 0, 0);
                res({ container, ctx, fontCtx });
            });
            img.src = Font;
        })

    }

    function deleteWorkspace(ws) {
        ws.container.remove();
    }

    // Clip character # 'char' from the spritesheet and return it as ImageData
    function getCharacter(ws, char) {
        const x = (char * FontCellWidth) % (FontCellWidth * FontColumns);
        const y = FontCellHeight * Math.floor(char / FontColumns);
        return ws.fontCtx.getImageData(x, y, FontCellWidth, FontCellHeight);
    }

    // Draw ImageData 'char' at coordinates 'x', 'y' on the canvas
    // Colours 'fg' and 'bg' are values from the COLOURS array
    function putCharacter(ws, cc, x, y, fg, bg) {
        if (cc >= FontColumns * FontRows) return;
        const char = getCharacter(ws, cc);
        // Draw the character
        ws.ctx.globalCompositeOperation = 'source-over';
        ws.ctx.putImageData(char, x, y);
        // Overlay it with the foreground colour
        ws.ctx.fillStyle = fg;
        ws.ctx.globalCompositeOperation = 'source-atop';
        ws.ctx.fillRect(x, y, FontCellWidth, FontCellHeight);
        // Draw the background colour behind it
        ws.ctx.globalCompositeOperation = 'destination-over';
        ws.ctx.fillStyle = bg;
        ws.ctx.fillRect(x, y, FontCellWidth, FontCellHeight);
    }

    function getPNG(ws) {
        return new Promise(res => {
            const data = ws.ctx.canvas.toDataURL();
            deleteWorkspace(ws);
            const img = new Image();
            img.addEventListener('load', () => res(img));
            img.src = data;
        });
    }

    function getDataURL(ws) {
        const dataURL = ws.ctx.canvas.toDataURL();
        deleteWorkspace(ws);
        return dataURL;
    }
    
    function cgaColor(n) {
        return ANSI_Colors[CGA_Colors[n]];
    }

    function longestRow(data) {
        return data.reduce((a, c) => {
            const len = c.reduce((a, c, i) => c === undefined ? a : i, 0);
            return len > a ? len : a;
        }, 0) + 1;
    }

    async function binToPNG(bin, cols, dataURL) {
        const ws = await getWorkspace(cols, (bin.length / 2) / cols);
        let x = 0;
        let y = 0;
        while (bin.length) {
            const char = bin.shift();
            const attr = bin.shift();
            putCharacter(ws, char, x * FontCellWidth, y * FontCellHeight, cgaColor(attr&15), cgaColor((attr>>4)&7));
            x = (x + 1) % cols;
            if (x === 0) y++;
        }
        if (dataURL) return getDataURL(ws);
        return getPNG(ws);
    }

    function binToHTML(bin, cols) {

        const elem = document.createElement('div');
        elem.classList.add('bbs-view');
        let a;
        let c = 0;
        let span;
        while (bin.length) {
            const char = bin.shift();
            const attr = bin.shift();
            if (!span || attr != a) {
                span = document.createElement('span');
                span.style.setProperty('color', cgaColor(attr&15));
                span.style.setProperty('background-color', cgaColor((attr>>4)&7));
                elem.appendChild(span);
            }
            console.debug(char, attr);
            const s = String.fromCharCode(char);
            const u = CP437_UTF8[s];
            span.innerText += u === undefined ? s : u;
            c = (c + 1) % cols;
            if (c === 0) {
                span = null;
                elem.innerHTML += '\r\n';
            }
        }
        return elem;

    }

    async function textToPNG(text, dataURL) {
        const data = parseText(text);
        const lr = longestRow(data);
        const ws = await getWorkspace(lr, data.length);
        data.forEach((y, r) => {
            if (y === undefined) return;
            y.forEach((x, c) => {
                if (x === undefined) return;
                const cc = (UTF8_CP437[x.c] !== undefined ? UTF8_CP437[x.c] : x.c).charCodeAt(0);
                putCharacter(ws, cc, c * FontCellWidth, r * FontCellHeight, ANSI_Colors[x.fg], ANSI_Colors[x.bg]);
            });
        });
        if (dataURL) return getDataURL(ws);
        return getPNG(ws);
    }

    function textToHTML(text, options) {

        const data = parseText(text, options);
        const lr = longestRow(data);

        const elem = document.createElement('div');
        elem.classList.add('bbs-view');
        let ofg;
        let obg;
        let span;
        for (let y = 0; y < data.length; y++) {
            if (data[y] === undefined) continue;
            for (let x = 0; x < lr; x++) {
                if (data[y][x]) {
                    if (!span || data[y][x].fg != ofg || data[y][x].bg != obg) {
                        ofg = data[y][x].fg;
                        obg = data[y][x].bg;
                        span = document.createElement('span');
                        span.style.setProperty('color', ANSI_Colors[data[y][x].fg]);
                        span.style.setProperty('background-color', ANSI_Colors[data[y][x].bg]);
                        elem.appendChild(span);
                    }
                    span.innerText += data[y][x].c;
                } else {
                    if (!span || ofg !== 7 || obg !== 0) {
                        ofg = 7;
                        obg = 0;
                        span = document.createElement('span');
                        span.style.setProperty('color', ANSI_Colors[7]);
                        span.style.setProperty('background-color', ANSI_Colors[0]);
                        elem.appendChild(span);
                    }
                    span.innerText += ' ';
                }
            }
            span = null;
            elem.innerHTML += '\r\n';
        }
        return elem;

    }

    return {
        binToPNG,   // async Graphics.binToPNG([]bin, columns, dataURL), returns an HTMLImageElement or dataURL
        binToHTML,  // Graphics.binToHTML([]bin, columns), returns a <div> HTMLElement
        textToPNG,  // async Graphics.textToPNG(text, dataURL), returns an HTMLImageElement or dataURL
        textToHTML, // Graphics.textToHTML(text), returns a <div> HTMLElement
    };

})();