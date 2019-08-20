/*
 * Implements the local console using conio
 */

require('graphic.js', 'Graphic');

dk.console.local_io = {
	graphic:new Graphic(dk.cols, dk.height, 7, ' '),

	clear:function() {
	},
	cleareol:function() {
	},
	gotoxy:function(x,y) {
	},
	movex:function(pos) {
	},
	movey:function(pos) {
	},
	print:function(string) {
	},
};
