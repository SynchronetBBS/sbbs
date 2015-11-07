/*
 * Implements the local console using conio
 */

if (js.global.Graphic === undefined)
	load("graphic.js");

dk.local = {
	graphic:new Graphic(dk.cols, dk.height, 7, ' ');
	
};
