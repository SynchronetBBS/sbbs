/*
 * Implements the local console using conio
 */

require('graphic.js', 'Graphic');
conio.init();
conio.setcursortype(2);
dk.console.input_queue_callback.push(function() {
	if (conio.kbhit)
		return conio.getch();
});

dk.console.local_io = {
	screen:new Screen(dk.cols, dk.height, 7, ' '),

	input_queue:new Queue('dorkit_input'),
	update:function() {
		if (this.screen.touched) {
			var b = [];
			var x;
			var y;

			for (y = 0; y < this.screen.graphic.height; y++) {
				for (x = 0; x < this.screen.graphic.width; x++) {
					b.push(ascii(this.screen.graphic.data[x][y].ch));
					b.push(this.screen.graphic.data[x][y].attr.value);
				}
			}
			conio.puttext(1, 1, this.screen.graphic.width, this.screen.graphic.height, b);
			conio.gotoxy(this.screen.pos.x, this.screen.pos.y);
			conio.textattr = this.screen.attr.value;
			this.screen.touched = false;
		}
	},
	clear:function() {
		conio.clrscr();
		this.update();
	},
	cleareol:function() {
		conio.clreol();
		this.update();
	},
	gotoxy:function(x,y) {
		conio.gotoxy(x+1, y+1);
		this.update();
	},
	movex:function(pos) {
		conio.gotoxy(conio.wherex + pos, conio.wherey);
		this.update();
	},
	movey:function(pos) {
		conio.gotoxy(conio.wherex, conio.wherey + pos);
		this.update();
	},
	print:function(string) {
		this.screen.print(string);
		this.update();
	},
};
