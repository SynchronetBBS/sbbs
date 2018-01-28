function GraphicsConverter(spritesheet_src, font_width, font_height, spritesheet_columns, spritesheet_rows) {

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
        "#FFFFFF"  // White
    ];

    function get_workspace(cols, rows, callback) {

        const container = document.createElement('div');

        const canvas = document.createElement('canvas');
        const ctx = canvas.getContext('2d');
        canvas.width = cols * font_width;
        canvas.height = rows * font_height;

        const spritesheet_canvas = document.createElement('canvas');
        const spritesheet_ctx = spritesheet_canvas.getContext('2d');
        spritesheet_canvas.width = spritesheet_columns * font_width;
        spritesheet_canvas.height = spritesheet_rows * font_height;

        container.appendChild(canvas);
        container.appendChild(spritesheet_canvas);

        const img = new Image();
        img.addEventListener(
            'load', function () {
                spritesheet_ctx.drawImage(img, 0, 0);
                callback(
                    {   container : container,
                        canvas : canvas,
                        ctx : ctx,
                        spritesheet_canvas : spritesheet_canvas,
                        spritesheet_ctx : spritesheet_ctx
                    }
                )
            }
        );
        img.src = spritesheet_src;

    }

    function delete_workspace(workspace) {
        $(workspace.container).remove();
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
        const data = workspace.canvas.toDataURL();
        const img = new Image();
        img.addEventListener('load', function () { callback(img); });
        img.src = data;
    }

    this.from_bin = function (bin, cols, rows, callback) {

        get_workspace(
            cols, rows, function (workspace) {
                var x = 0;
                var y = 0;
                for (var n = 0; n < cols * rows * 2; n = n + 2) {
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
                get_png(
                    workspace, function (img) {
                        delete_workspace(workspace);
                        callback(img);
                    }
                );
            }
        );

    }

}
