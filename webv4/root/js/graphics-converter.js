/*
 * You should use webv4/root/js/graphics.js instead.
 * I'll leave this file here for now in case some old pages depend on it,
 * but I will probably remove it at some point, perhaps years from now.
 * - ec, March 16, 2021
 */

function GraphicsConverter(spritesheet_src, font_width, font_height, spritesheet_columns, spritesheet_rows) {

    if (!spritesheet_src) spritesheet_src = '/images/cp437-ibm-vga8.png';
    if (!font_width) font_width = 8;
    if (!font_height) font_height = 16;
    if (!spritesheet_columns) spritesheet_columns = 64;
    if (!spritesheet_rows) spritesheet_rows = 4;

    const COLORS = [
        "#000000", // Black
        "#0000A8", // Blue
        "#00A800", // Green
        "#00A8A8", // Cyan
        "#A80000", // Red
        "#A800A8", // Magenta
        "#A85400", // Brown
        "#A8A8A8", // Light Grey
        "#545454", // Dark Grey (High Black)
        "#5454FC", // Light Blue
        "#54FC54", // Light Green
        "#54FCFC", // Light Cyan
        "#FC5454", // Light Red
        "#FC54FC", // Light Magenta
        "#FCFC54", // Yellow (High Brown)
        "#FFFFFF", // White
    ];

    const ANSI_COLORS = [
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
        "#FFFFFF", // White
    ];

    function get_workspace(cols, rows, callback) {

        const container = document.createElement('div');

        const canvas = document.createElement('canvas');
        const ctx = canvas.getContext('2d');
        ctx.canvas.width = cols * font_width;
        ctx.canvas.height = rows * font_height;
        ctx.fillStyle = COLORS[0];
        ctx.fillRect(0, 0, ctx.canvas.width, ctx.canvas.height);

        const spritesheet_canvas = document.createElement('canvas');
        const spritesheet_ctx = spritesheet_canvas.getContext('2d');
        spritesheet_ctx.canvas.width = spritesheet_columns * font_width;
        spritesheet_ctx.canvas.height = spritesheet_rows * font_height;
        spritesheet_canvas.style.display = 'none';

        container.appendChild(canvas);
        container.appendChild(spritesheet_canvas);

        const img = new Image();
        img.addEventListener('load', () => {
            spritesheet_ctx.drawImage(img, 0, 0);
            callback({ container, ctx, spritesheet_ctx });
        });
        img.src = spritesheet_src;

    }

    function delete_workspace(workspace) {
        workspace.container.remove();
    }

    // Clip character # 'char' from the spritesheet and return it as ImageData
    function get_character(workspace, char) {
        const x = (char * font_width) % (font_width * spritesheet_columns);
        const y = font_height * Math.floor(char / spritesheet_columns);
        return workspace.spritesheet_ctx.getImageData(x, y, font_width, font_height);
    }

    // Draw ImageData 'char' at coordinates 'x', 'y' on the canvas
    // Colours 'fg' and 'bg' are values from the COLOURS array
    function put_character(workspace, char, x, y, fg, bg) {
        // Draw the character
        workspace.ctx.globalCompositeOperation = 'source-over';
        workspace.ctx.putImageData(char, x, y);
        // Overlay it with the foreground colour
        workspace.ctx.fillStyle = fg;
        workspace.ctx.globalCompositeOperation = 'source-atop';
        workspace.ctx.fillRect(x, y, font_width, font_height);
        // Draw the background colour behind it
        workspace.ctx.globalCompositeOperation = 'destination-over';
        workspace.ctx.fillStyle = bg;
        workspace.ctx.fillRect(x, y, font_width, font_height);
    }

    function get_png(workspace, callback) {
        const data = workspace.ctx.canvas.toDataURL();
        const img = new Image();
        img.addEventListener('load', () => callback(img));
        img.src = data;
    }

    this.from_bin = function (bin, cols, rows, callback, dataOnly) {
        get_workspace(cols, rows, workspace => {
            let x = 0;
            let y = 0;
            for (let n = 0; n < cols * rows * 2; n = n + 2) {
                const char = bin.substr(n, 1).charCodeAt(0);
                const attr = bin.substr(n + 1, 1).charCodeAt(0);
                put_character(
                    workspace,
                    get_character(workspace, char),
                    x * font_width,
                    y * font_height,
                    COLORS[attr&15],
                    COLORS[(attr>>4)&7]
                );
                x++;
                if (x >= cols) {
                    x = 0;
                    y++;
                }
            }
            if (dataOnly) {
                callback(workspace.ctx.canvas.toDataURL());
            } else {
                get_png(workspace, img => {
                    delete_workspace(workspace);
                    callback(img);
                });
            }
        });
    }

    this.from_ans = function (ans, target, rate) {
        let x = 0;
        let y = 0;
        let _x = 0;
        let _y = 0;
        let fg = 7;
        let bg = 0;
        let high = 0;
        let match;
        let opts;
        const re = /^\u001b\[((?:[0-9]{0,2};?)*)([a-zA-Z])/;
        const data = [[]];
        const seq = [];
        while (ans.length) {
            match = re.exec(ans);
            if (match !== null) {
                ans = ans.substr(match[0].length);
                opts = match[1].split(';').map(e => parseInt(e, 10));
                switch (match[2]) {
                    case 'A':
                        y = Math.max(y - (isNaN(opts[0]) ? 1 : opts[0]), 0);
                        break;
                    case 'B':
                        y += (isNaN(opts[0]) ? 1 : opts[0]);
                        data[y] = [];
                        break;
                    case 'C':
                        x = Math.min(x + (isNaN(opts[0]) ? 1 : opts[0]), 79);
                        break;
                    case 'D':
                        x = Math.max(x - (isNaN(opts[0]) ? 1 : opts[0]), 0);
                        break;
                    case 'f':
                    case 'H':
                        y = isNaN(opts[0]) ? 1 : opts[0];
                        x = isNaN(opts[1]) ? 1 : opts[0];
                        if (y >= data.length) data[y] = [];
                        break;
					case 'm':
                        for (let o of opts) {
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
					case 's': // push xy
						_x = x;
                        _y = y;
						break;
					case 'u': // pop xy
						x = _x;
                        y = _y;
						break;
					case 'J':
						if (opts.length == 1 && opts[0] == 2) {
                            for (let yy = 0; yy < data.length; yy++) {
                                if (!Array.isArray(data[yy])) data[yy] = [];
								for (let xx = 0; xx < 80; xx++) {
                                    data[yy][xx] = { c: ' ', a: fg|(bg<<3)|(high<<7) };
								}
							}
						}
						break;
					case 'K':
                        for (let xx = 0; xx < 80; xx++) {
                            data[y][xx] = { c: ' ', a: fg|(bg<<3)|(high<<7) };
                        }
						break;
					default:
						// Unknown or unimplemented command
						break;
                }
            } else {
                let ch = ans.substr(0, 1);
                switch (ch) {
                    case '\x1a':
                        ans = '';
                        break;
                    case '\n':
                        y++;
                        data.push([]);
                        break;
                    case '\r':
                        x = 0;
                        break;
                    default:
                        data[y][x] = { c: ch, a: fg|(bg<<3)|(high<<7) };
                        seq.push({y, x});
                        x++;
                        if (x > 79) {
                            x = 0;
                            y++;
                            data.push([]);
                        }
                        break;
                }
                ans = ans.substr(1);
            }
        }
        get_workspace(80, data.length, workspace => {
            if (typeof target == 'string') {
                let tgt = document.getElementById(target);
                if (tgt === null) tgt = document.getElementById(target.replace(/^#/, ''));
                tgt.prepend(workspace.container);
            }
            seq.forEach((e, i) => {
                function draw() {
                    put_character(
                        workspace,
                        get_character(workspace, data[e.y][e.x].c.charCodeAt(0)),
                        e.x * font_width,
                        e.y * font_height,
                        ANSI_COLORS[(data[e.y][e.x].a&7) + ((data[e.y][e.x].a&(1<<7)) ? 8 : 0)],
                        ANSI_COLORS[(data[e.y][e.x].a>>3)&7]
                    );
                }
                if (rate) {
                    setTimeout(draw, i * Math.ceil(1000 / (rate / 8)));
                } else {
                    draw();
                }
            });
            if (typeof target == 'function') get_png(workspace, target);
        });
    }

}
