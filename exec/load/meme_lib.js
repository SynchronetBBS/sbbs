// Meme generation library
// @format.tab-size 4, @format.use-tabs true

"use strict";

var BORDER_NONE = 0;
var BORDER_SINGLE = 1;
var BORDER_MIXED1 = 2;
var BORDER_MIXED2 = 3;
var BORDER_MIXED3 = 4;
var BORDER_DOUBLE = 5;
var BORDER_ORNATE1 = 6;
var BORDER_ORNATE2 = 7;
var BORDER_ORNATE3 = 8;
var BORDER_COUNT = 9;

// We don't have String.repeat() in ES5
function repeat(ch, length)
{
	var result = "";
	for (var i = 0; i < length; ++i)
		result += ch;
	return result;
}

function top_border(border, width)
{
	var str;
	switch (border) {
		case BORDER_NONE:
			str = format("%*s", width, "");
			break;
		case BORDER_SINGLE:
			str = format("\xDA%s\xBF", repeat("\xC4", width - 2));
			break;
		case BORDER_MIXED1:
			str = format("\xD6%s\xB7", repeat("\xC4", width - 2));
			break;
		case BORDER_MIXED2:
			str = format("\xD5%s\xB8", repeat("\xCD", width - 2));
			break;
		case BORDER_MIXED3:
			str = format("\xDA%s\xB7", repeat("\xC4", width - 2));
			break;
		case BORDER_DOUBLE:
			str = format("\xC9%s\xBB", repeat("\xCD", width - 2));
			break;
		case BORDER_ORNATE1:
			str = format("\xC4\xC5\xC4%s\xC4\xC5\xC4", repeat(" ", width - 6));
			break;
		case BORDER_ORNATE2:
			str = format("\xDA\xC4%s\xC4\xBF", repeat(" ", width - 4));
			break;
		case BORDER_ORNATE3:
			str = format("\xC9\xC4%s\xC4\xBB", repeat(" ", width - 4));
			break;
	}
	return str + "\x01N\r\n";
}

function mid_border(border, width, margin, line)
{
	var str;
	switch (border) {
		case BORDER_NONE:
		case BORDER_ORNATE1:
		case BORDER_ORNATE2:
			str = format("%*s%-*s", margin, "", width - margin, line);
			break;
		case BORDER_SINGLE:
		case BORDER_MIXED2:
		case BORDER_ORNATE3:
			str = format("\xB3%*s%-*s\xB3", margin - 1, "", width - (margin + 1), line);
			break;
		case BORDER_DOUBLE:
		case BORDER_MIXED1:
			str = format("\xBA%*s%-*s\xBA", margin - 1, "", width - (margin + 1), line);
			break;
		case BORDER_MIXED3:
			str = format("\xB3%*s%-*s\xBA", margin - 1, "", width - (margin + 1), line);
			break;
	}
	return str + "\x01N\r\n";
}

function bottom_border(border, width)
{
	var str;
	switch (border) {
		case BORDER_NONE:
			str = format("%*s", width, "");
			break;
		case BORDER_SINGLE:
			str = format("\xC0%s\xD9", repeat("\xC4", width - 2));
			break;
		case BORDER_MIXED1:
			str = format("\xD3%s\xBD", repeat("\xC4", width - 2));
			break;
		case BORDER_MIXED2:
			str = format("\xD4%s\xBE", repeat("\xCD", width - 2));
			break;
		case BORDER_MIXED3:
			str = format("\xD4%s\xBC", repeat("\xCD", width - 2));
			break;
		case BORDER_DOUBLE:
			str = format("\xC8%s\xBC", repeat("\xCD", width - 2));
			break;
		case BORDER_ORNATE1:
			str = format("\xC4\xC5\xC4%s\xC4\xC5\xC4", repeat(" ", width - 6));
			break;
		case BORDER_ORNATE2:
			str = format("\xC0\xC4%s\xC4\xD9", repeat(" ", width - 4));
			break;
		case BORDER_ORNATE3:
			str = format("\xC8\xC4%s\xC4\xBC", repeat(" ", width - 4));
			break;
	}
	return str + "\x01N\r\n";
}

function generate(attr, border, text)
{
	var width = 39;
	var msg = attr + top_border(border, width);
	var array = word_wrap(text, width - 4).split("\n");
	for (var i in array) {
		var line = truncsp(array[i]);
		if (!line && i >= array.length - 1)
			break;
		var margin = Math.floor((width - line.length) / 2);
		msg += attr + mid_border(border, width, margin, line);
	}
	msg += attr + bottom_border(border, width);
	return msg;
}

this;
