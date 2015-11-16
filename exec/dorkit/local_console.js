/*
 * Implements the local console using conio
 */

if (typeof(Graphic) === 'undefined')
	load("graphic.js");

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
