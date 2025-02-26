function w(str)
{
	console.write(str);
}

function read_apc()
{
	var ret = '';
	var ch;
	var state = 0;

	for(;;) {
		ch = console.getbyte(1000);
		if (ch === null)
			return undefined;
		switch(state) {
			case 0:
				if (ch == 0x1b) {
					state++;
					break;
				}
				break;
			case 1:
				if (ch == 95) {
					state++;
					break;
				}
				state = 0;
				break;
			case 2:
				if (ch == 0x1b) {
					state++;
					break;
				}
				ret += ascii(ch);
				break;
			case 3:
				if (ch == 92) {
					return ret;
				}
				return undefined;
		}
	}
	return undefined;
}

function read_dim()
{
	var ret = {width:0, height:0};
	var ch;
	var state = 0;

	for(;;) {
		ch = console.getbyte();
		switch(state) {
			case 0:
				if (ch == 0x1b) {
					state++;
					break;
				}
				break;
			case 1:
				if (ch == 91) {
					state++;
					break;
				}
				state = 0;
				break;
			case 2:
				if (ch == 63) {
					state++;
					break;
				}
				state = 0;
				break;
			case 3:
				if (ch == 50) {
					state++;
					break;
				}
				state = 0;
				break;
			case 4:
				if (ch == 59) {
					state++;
					break;
				}
				state = 0;
				break;
			case 5:
				if (ch == 48) {
					state++;
					break;
				}
				state = 0;
				break;
			case 6:
				if (ch == 59) {
					state++;
					break;
				}
				state = 0;
				break;
			case 7:
				if (ch >= ascii('0') && ch <= ascii('9')) {
					ret.width = ret.width * 10 + (ch - ascii('0'));
					break;
				}
				else if(ch == 59) {
					state++;
					break;
				}
				state = 0;
				break;
			case 8:
				if (ch >= ascii('0') && ch <= ascii('9')) {
					ret.height = ret.height * 10 + (ch - ascii('0'));
					break;
				}
				else if(ch == 83) {
					return ret;
				}
				state = 0;
				break;
		}
	}
	return undefined;
}

function pixel_capability()
{
	var ret = false;
	var ch;
	var state = 0;
	var optval = 0;

	for(;;) {
		ch = console.getbyte(1000);
		if (ch == null)
			break;
		switch(state) {
			case 0:
				if (ch == 0x1b) { // ESC
					state++;
					break;
				}
				break;
			case 1:
				if (ch == 91) {   // [
					state++;
					break;
				}
				state = 0;
				break;
			case 2:
				if (ch == 60) {   // <
					state++;
					break;
				}
				state = 0;
				break;
			case 3:
				if (ch == 48) {   // 0
					state++;
					break;
				}
				state = 0;
				break;
			case 4:
				if (ch == 59) {   // ;
					state++;
					break;
				}
				state = 0;
				break;
			case 5:
				if (ch >= ascii('0') && ch <= ascii('9')) {
					optval = optval * 10 + (ch - ascii('0'));
					break;
				}
				else if(ch == 59) {
					if (optval === 3)
						ret = true;
					optval = 0;
					break;
				}
				else if (ch === 99) { // c
					if (optval === 3)
						ret = true;
					return ret;
				}
				state = 0;
				break;
		}
	}
	return ret;
}

if (console.cterm_version < 1316) {
	console.pause();
}
else {
	// Disable escape parsing
	var opt = console.ctrlkey_passthru;
	console.ctrlkey_passthru = "+[";

	// Check that pixel graphics are available
	w('\x1b[<c');
	if (!pixel_capability()) {
		console.pause()
		console.ctrlkey_passthru = opt;
	}
	else {

		// Check for the icons in the cache...
		w('\x1b_SyncTERM:C;L;SyncTERM.p?m\x1b\\');
		var lst = read_apc();
		var m = lst.match(/\nSyncTERM.ppm\t([0-9a-f]+)\n/);
		if (m == null || m[1] !== '69de4f5fe394c1a8221927da0bfe9845') {
			w('\x1b_SyncTERM:C;S;SyncTERM.ppm;UDYKNjQgNjQKMjU1C'+
			    'v///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////3h4eHBwcGhoaGJiYl1dXVpaWldXV1ZWVlZWVldXV1paWl1dXWJiYmhoaHBwcHh4eP///////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////3JycmJiYlVVVUpKSkFBQTo6OjMzMy0tLSgoKCQkJCIiIh8fHx4eHh4eHh4eHh4eHh8fHyIiIiQkJCgoKC0tLTMzMzo6OkFBQUpKSlVVVWJiYnJycv///////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////29vb1paWklJSTw8PC8vLyUlJR4eHh4eHh8fHyAgICEhISIiIiEhIRsbGx4eHiMjIyMjIyMjIyMjIyMjIyMjIx4eHhsbGyEhISIiIiEhISAgIB8fHx4eHh4eHiUlJS8vLzs7O0lJSVpaWm9vb////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////2pqalJSUj4+Pi0tLSIiIh4eHiAgICIiIhsbGyMjIyMjIxYWFg0NDQ0NDQ0NDQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA0NDQ0NDRYWFhYWFiMjIxsbGyIiIiAgIB4eHiIiIi0tLT4+PlFRUWpqav///////////////////////////////////////////'+
			    '////////////////////////////////3V1dVdXVz8/PywsLB4eHh8fHyIiIhsbGyMjIxYWFg0NDQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA0NDRYWFiMjIx4eHiIiIh8fHx4eHiwsLD8/P1dXV3V1df///////////////////////////////'+
			    '////////////////////////21tbU1NTTMzMyIiIh8fHyIiIiMjIxYWFg0NDQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAADAAAegAAdAAAAAAAAAAAagAABgAAAAAAAA0NDRYWFiMjIyIiIh8fHyEhITMzM0xMTG1tbf///////////////////////'+
			    '////////////////3BwcEtLSy8vLx4eHiEhIRsbGxYWFg0NDQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABwAACgAAAAAAEAAAFgAAKQAADwAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAEAAAfwAARwAAMgAAAAAAAAAALQAAAgAAAAYGBggICAwMDAcHBxscGxYWFhsbGyEhIR4eHi8vL0tLS3BwcP///////////////'+
			    '////////319fVNTUzIyMh4eHiEhIR4eHhYWFgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAICAgMDAwAAAAAAAAAAAAAAOwAAVAAAAwAAjAAAkwAAOwAAIQAADgAAAwAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAgAAlAAAUQAAAwAAAAAAAAAAAAAAAAAAAAAAABUVFRscGxcXFxcXFxEREQAAAAAAAA0NDR4eHiEhIR4eHjIyMlNTU319ff///////'+
			    '////2FhYT4+PiMjIyAgIB4eHhYWFgAAAAAAAAAAAAAAAAAAAAAAAAAAAAEBARYWFhUVFRESERcYFwAAAAAABwAAOgAAAAAAAAAAAAAAAAAAAAAALAAAiwAAwAAAXwAAPQAAAAAAAAAALwAASAAAAQAAAAAAIQAAnwAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABYWFh4eHiAgICMjIz09PWFhYf///'+
			    'zs7OykpKR4eHiEhIRYWFgAAAQAAAgAABAAAAAAAAAAAAAEBAQEBASYmJk9QT1FSUSMkIwAAAAAAAAAAAQAAAgAADAAAHgAAVgAAVgAAVgAAWAAAOQAANgAAwAAAvAAAqAAAOgAABgAAqQAA1wAAKQAAEQAAFAAEoAcHHD8/Py0uLVdXV2BfYGBfYGBfYFRTVCIiIgAAAAAABgAAQQAAAAAAJQAAVQAAFAAAAQAAAAAAABYWFiEhIR4eHikpKTs7O'+
			    'yAgICEhISMjIw0NDQAABgAAcQAAFQAAMgAAAAAAAA8QD3h5eIODg5iYmLq7umxsbAAAAAAAAAAAAAAAYQAAjgAAjgAApAAA3wAA3AAAzQAA4gAAuAAAZAAAwQAAogAAjAAAKwAADgAA2wAA5gAAXgAAUwAAngAK4AUFNy8vL0hJSMPDw8rJysrJysrJyrW1tVFSUQAAAAAABQAAMAAAAAAAHAAAPwAAfAAAeQAABwAAAAAAAA0NDSMjIyEhISAgI'+
			    'BYWFg0NDQAACAAAVwAAKQAAJgMDAw8PD2VkZZmZmZaXlrm6ub2+vby9vICAgCYmJgAAAwAARAAAoQAA2QAA5AAA2AAAjAgIPg8PRA8PPwAAMwAAcwAAxQAAvwEBPw4OLQ8PDwgICwAAWAAA8AAFPgArlQAQ2wAD6gAAkwAAEA0NDXh3eOPi4+/w7+/w7+Dh4H5+fgAAAAwMDA8PDw8PDw8PDw8PDwQEKAAAiAAAUQAAAAAAAAAAAAAAAAAAAA0ND'+
			    'QAAAAAAAAAAagAAYwAAAgAAADIyMra3tru8u72+vb2+vb2+vb2+vb2+vXNzcw4ODgABRgID3QAC/QAA/AQESB0dJx4eI2tsa7S1tJ2enQAAJgAAzwAAxxYWVi0tLqanpra3tmpqagAAKQAa5wAHwgAvkAASXxUVHyEhKSEhIiEhITEyMbKysvX19fX19erq6ouLiwYGBo6Pjra3tra3tqqrqpiZmCMjPAAAjgAAWQAAAAAAAAAAAAAAAAAAAAAAA'+
			    'AAAAAAAAAAApgAAGwAAAAAAAGtsa76/vr2+vb2+vcnJycrJyr2+vb2+vYeIhwEBBAAF6gsQ+wAK/QAA8xgXP7CvsLW2tbm6ubCxsCkpKQAAfgAAzyUlaZ+eorm5uby9vL2+va2trQABBABcqQADgQkMFTIzN5ycnMjHyMjHyMjHyI+Oj5WWlcLCwsDBwL/Av4iJiCMjI5SUlL2+vcHBwXh3eAgIOQICbgAAlQAAcgAAWwAAIAAAAAAAAAAAAAAAA'+
			    'AAAAAAAAAAJZAALFwAAABQUFLGxsdzc3Nvc29TU1IWDhYmIib2+vb2+vZ6fng8PEQADmwcK/QAGwQAAyxkYP7i3uL2+vbm6uaChoBITRAAE6AAAXVJSUsDAwMHBwb2+vb2+vbGxsR4eHwAAWhERE2lpaZ2encXFxcjIyMjIyMjIyMPDw4eHh6Ojo9na2dvc27i4uEBAQHFxccPEw7/Av5SUlAAAfAAAfwICiQIC+AAA7AAAhwAAMQAABwAAFAAAA'+
			    'AAAAAAAAAAVFQAUFwAAASorKvr6+v39/f7+/vr5+omJiRkZGW5tbtbT1sDAwIyNjAAADQAAQwICIgAAoBkYP7i3uL2+vbO0s4yNjAABaQAOzRISFlxdXFRUVKampr6/vr2+vbS1tEJDQgEBAZaWlszLzL/Av72+vb2+vb2+vb2+vb2+vUVFRX59fvv7+/////Ly8mJiYkZHRsrJyr2+vbm6uQICIwAAtwYG9wYG+QAA7QAAvAAAbAAADgAALAAAA'+
			    'QAAAAAABQAFHQAEcAAAQgwMD3l6ecDAwNDQ0M3Nzby8vJCOkCAgIDIxMjIyMjs7OxUWFSAgIDw9PAYGYxkYMLi3uL2+vbKzsoCAgB8gNgBxnAUICBYXIgsLE0xNTLOzs72+vcHCwUhHSDU1Nb6/vsHCwb6/vsDAwL6/vr2+vba3tra3tg8PDzExMff39/////39/W9vby4uLrCxsL2+vbS1tFVVVRUWOxcXTBcXTBUWSgkJMg8PJwMDBgAAXgAAF'+
			    'gAABAAARAAAzAAAxQAAwwABQQMJHU1LTcC9wL6+vr2+vb2+vYGBgRERES4vLqOjo7e4t5WWlVpbWggIRhwcLL+/v8fIx7O0s4uLi7S0tCgtLRkYGQAAUwAAODExMbi4uMfIx8zMzE1NTVtcW8jJyMjJyMLDwry6vLGxsbu8u7W2taqqqgAAABcYF+Pj4/7+/v///9na2cLDwsfIx8fIx7/Av8TFxKKjopmamZmamZmamT4/PmtraxUVGAAAdAAAF'+
			    'wAAGgAAwAAA7QAAcgAArwAF8wAlswEBWAEBKU1NTXh4eMfFx87Nzo6Njh8gH56fnr2+vb2+vaChoAkJCjExMebl5v///7GxscjIyP7+/vz7/MXExV9fYBsbHBgYGOPj4/////Ty9GNiY9ra2v39/f///7m5uSgoKEtLS7Kzsr2+vXNycwAAPQAADoCAgPr6+v////////////////////7+/v///zs7OwEBVwEYaAEzYgEBYQEBWgAAawAAVwAAA'+
			    'AACPgAYxwAQ4gAAXwAA2AAD9AASZwAAZwAALwAAAA4NDmpoara1tsjHyIKCgqqqqr6/vr2+vbe3tykoKUFBQdLS0t7f3q6vrqWlpd/f397f3t7f3s7OzoqJiikpKcfIx+Hh4d7d3lxbXNbV1v79/v7+/oaGhgAAACkpKYKDgr2+vZeYlyMjJQAAAHd3d+Pj497f3svMy4CAgIqKit/f397f3t7f3lpbWgAAcwAb4AA6yAAEvAAAhwAAcQAAAAAAA'+
			    'AAGYgAyzQAe2AAATQAAhQAANx8fH5OTk5OTkysrKwAAeAAADZSVlMLCwr2+vbe4t7/Av72+vcHBwaOjo2hoaL2+vb2+vaytrIKCgr/Av72+vb2+vb2+vcLDwqGhobq7usTExMjHyFRVVNHQ0f39/f79/oaGhgAAAAEBdBITJCorKiorKhAQEAAAAGpqas7Nzr2+vZeYlwAAABUVFb/Av72+vb2+vXt7ewAABwAALwAAfAAL5QAAPgAAAAAAAAAAA'+
			    'AABIQAIswAR9gAR3wAAvCsrK5mXmbW2tXd4dycoJx8fN0A/QsTDxL2+vdDO0FtaW8jGyNnZ2fHw8fP08+np6fX19fX19YWFhY2MjcHBwcDBwJOSk15eXrO0s72+vb2+vcLCwsjHyEZHRqmnqfTz9Pr5+szMzE1OTQYGKQAAfwAAYg4ODj0+PTEyMSoqKru8u72+vZeYlwAANQMDM46Ojr2+vb2+vbO0swsVJgBEsgAAvwACbgAACQAAAAAAAAAAA'+
			    'AAAAQAANAA8vwBOuwAApTIzM62rrbW2tbm6ubW2tbW2tb6+vs7MzsPDw9XT1bq6uldZV1NSU7u4u9LR0vPz8/Ly8vDw8G5ubpiXmMTExMPEw5WUlQcHB2hoaLe4t72+vcHBwcfHx1NTUzY3Nufm5/Hw8dTU1L29vUhHSCYmLSopL2doZ4SFhEhJSBkZGbCwsL/Av5ucmwAATQAIsYaGhsHCwcHCwb6/vh8qOgBcnAAGEQAABAAAAAAAAAAAAAAAA'+
			    'AAAAAAAAAAAAAAAAAAAADIzMq2rrb2+vb2+vb2+vb2+vcHCwdHQ0drY2snJyb2+vbW1tWtraxERETIzMr/Av72+vbGysSsrK76/vtDO0M7Nzr2+vRAQEAAAAFtcW7S1tL2+vcfHx5iYmAAAAHl5ecvJy72+vdfV19XT1cTExMrJyr2+vZ+gn4CBgAAATI2NkcjIyKusqwACWQAovYuKi9LQ0tLQ0sLCwm9wbwCKjAAfIAAAAAAAAAAAAAAAAAAAA'+
			    'AAAAAAAAAAJCQAeHgAGBxwcHLe2t9LR0tLR0r+/v5qbmrO0s8PDw8fGx3t7e1dYW1tbYFBQVBAREl1dXcnIycjIyJycnEZGRrq7usLCwtrX2tPR029ubwsMUggIH15eXry8vMLCwqWlpRYXFhISE15dYIaHhsbFxsDBwMHBwba1tq+wr6usq1BQUAAAS4mJjMHBwXV1dQA7dwAdZYCAgMbGxsLAwtDP0DxQUACamwAODgAAAAAAAAAAAAAAAAAAA'+
			    'AAAAAAAAAATEwA3OAAMDAICAkhISMPCw8PCw7KzsmpraiQkJCcqKicqKgwPFQACqQABwgAAowAAT4ODg9DP0NLQ0ImIiBQUFCgoKCcnJ8PCw8PCw6+wrxYWMgABmgYGHlhYWLq7urOzs56fngUGIQAAahISEicnKZ2enry8vJiWmGJiYicnJwgICAAAAIaHhnl6eQ8PEABokAMYHXp7er2+vbSztNzb3BAYGAAkJAAAAAAAAAAAAAAAAAAAAAAAA'+
			    'AAAAAAAAAADAwAHBwBiYwAXFwUMDBktLRkZGRcvLw4vLwA9PgCPkQCgowC9xAB35QAkyQAApRkZa8bExsnJw+vqeWZmGiEhITMzMwgICBkZHxkZLllaWTAwMABJcwAkswgIMRkZLm9vb56fnnt8gCgpOhAQEAAAXRQUMxkZNhQUNQsLHAAAAAAAADY2NpSUlFdXVwAALAoYTXV4eJaWlp6fnp2enTw8PAICAgAAAAAAAAAAAAAAAAAAAAAAAAAAA'+
			    'AAAAAAAAAAAAAAAAAAAAAAAAAAhIQBfYAAAAABsbgCdnwCWlwAABABRWQCksgBnuwAfkwAASzMzM9XT1dXVmvPyUYeHHFtcW2tqakVFEgAAHQAAZQAAGQAAMgBQfgCnzQAk8wAIlQAAOgAAEhkZGYuLi0tLSwAAAAAADQAAiQAAmwAAUx0dHQoKCpmZmQgICAAAAAAAJgAAKgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA'+
			    'AAAAAAAAAAAAAAAAAAAAAAAAAABAQACAgAAAAACAgADAwADAwAAEAACHAADJgACNQABAyIiI5mZmejmj/z8E///Af39V/z8/Pr695GRLQAAAQAAAgAAMQAAYgALugBZzQBNzgAfzAAAlwAAjAAAAwMDAwEBAQAAAEZGRgICBAAAAwAAAjk5ORMTEwMDAwAAAAAAAAAAAQAASwAAAwAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA'+
			    'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABwAACwAADwAAFQAAABwdHFBQUGNjGWxsAIaGAP//Jf//bJqaahMTEwAAAAAAAAAAAAAAAAAARwAAVgAFVgANVgAAHwAAhgAAWgAAOQAADQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA'+
			    'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABcXAMzMA/r6Cp6eAAwMAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAVwAAkAAAXQAAFAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA'+
			    'A0NDQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAEZGAOPjHY6OaxUVAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAADwAAFwAAAQAAdQAAFwAADQAAAwAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA0ND'+
			    'SAgICEhISMjIw0NDQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAM3NM6CgBwQEBAAAAAAAAAAAAgUF8wYG+QYG+QYG+QYG+QAAIwMDmQYG+QYG+QYG+QYG+QAAIgMDhwYG+QYG+QUF8wQEqAAAAgUF6QUF6gAADwAADwUF6gUF5wAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA0NDSMjIyEhISAgI'+
			    'Do6OigoKB4eHiEhIRYWFgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABgYMzh0paWGFxcAAAAAAAAAAAACgUF9QEBMgYG+QEBUgUF1QEBMAAADAYG+QEBRQMDggUF3QAAGwAADgYG+QEBSAEBSQYG+QAAHwICdgYG+QICXQICWwYG+QICdQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABYWFiEhIR8fHygoKDo6O'+
			    'v///2FhYT09PSMjIyEhIR4eHg0NDQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABDQ218V83NCUBAAAAAAAAAAAAABwQEqwAAAAYG+QAAJwMDkgAAIQAAAAYG+QYG+QYG+QAAJwAAAAAAAAYG+QYG+QYG+QMDpQAAAAICagQEwwQEsgQErQQEwwICaQAAAAAAAAAAAAAAAAAAAAAAAAAAAA0NDR4eHiEhISMjIz09PWFhYf///'+
			    '////////319fVNTUzIyMh4eHiEhIR4eHg0NDQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAK+vAD09AAAAAAAAAAAAAAAAAAAAAAAAAAYG+QAAJwAAAAAAAAAAAAYG+QEBRQQExQMDiAAACQAAAAYG+QEBUgUF7AMDjgAAAAICdAMDmwQExgQEwgMDnAICcwAAAAAAAAAAAAAAAAAAAA0NDSMjIyIiIh4eHjIyMlJSUn19ff///////'+
			    '////////////////29vb0tLSy8vLx4eHiEhIRsbGxYWFgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIeHAAEBAAAAAAAAAAAAAAAAAAAACgEBMgYG+QEBUgAAEgAAAAAADQYG+QEBSAAAJwYG+QAAJwAAEQYG+QEBOgICewUF6wAAHgMDjAQEpgEBRAEBQwQEpgMDiwAAAAAAAAAAABYWFhsbGyEhIR4eHi8vL0pKSm9vb////////////////'+
			    '////////////////////////21tbUxMTDMzMyIiIh8fHyIiIiMjIxYWFgAAAAAAAAAAAAAAAAAAAAAAAFxcAAAAAAAAAAAAAAAAAAAAAAICXAYG+QYG+QYG+QMDggAAAAMDgQYG+QYG+QYG+QYG+QAAIQMDhQYG+QQEvQAADAUF4gQEugUF6QUF+AAAJQAAJgYG+QUF6hYWFiMjIyIiIh8fHyEhITMzM0xMTG1tbf///////////////////////'+
			    '////////////////////////////////3V1dVdXVz4+PisrKx4eHh8fHyIiIh4eHhYWFg0NDQ0NDQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA0NDQ0NDRYWFh4eHiIiIh8fHx4eHisrKz4+PldXV3V1df///////////////////////////////'+
			    '////////////////////////////////////////////2pqalJSUj4+Pi0tLSEhIR4eHiAgICIiIhsbGyMjIxYWFg0NDQ0NDQ0NDQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA0NDQ0NDRYWFiMjIxsbGyIiIiAgIB4eHiEhIS0tLT4+PlFRUWpqav///////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////29vb1paWklJSTs7Oy4uLiUlJR4eHh4eHh8fHyAgICIiIiIiIiEhIRsbGx4eHiMjIyMjIyMjIyMjIyMjIyMjIx4eHhsbGyEhISIiIiIiIiAgIB8fHx4eHh4eHiUlJS4uLjs7O0lJSVlZWW5ubv///////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////3JycmJiYlVVVUpKSkFBQTo6OjIyMi0tLSgoKCQkJCEhIR8fHx4eHh0dHR0dHR4eHh8fHyEhISQkJCgoKC0tLTIyMjk5OUFBQUpKSlVVVWJiYnJycv///////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////3h4eG9vb2hoaGJiYl1dXVpaWldXV1ZWVlZWVldXV1paWl1dXWJiYmhoaG9vb3h4eP///////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    '////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////'+
			    'w==\x1b\\');
		}
		w('\x1b_SyncTERM:C;LoadPPM;B=1;SyncTERM.ppm\x1b\\');

		m = lst.match(/\nSyncTERM.pbm\t([0-9a-f]+)\n/);
		if (m == null || m[1] !== '9b8a444559d6982566f87caf575a5fe2') {
			w('\x1b_SyncTERM:C;S;SyncTERM.pbm;UDQKNzIgMTM2C'+
			    'gAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAP//AAAA/'+
			    'wAAP////AAA/'+
			    'wAD/////8AA/'+
			    'wAf//////gA/'+
			    'wD///////8A/'+
			    'wP////////A/'+
			    'w/////////w/'+
			    'z/////////8/'+
			    '3/////////+/'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '3/////////+/'+
			    'z/////////8/'+
			    'w/////////w/'+
			    'wP////////A/'+
			    'wD///////8A/'+
			    'wAf//////gA/'+
			    'wAD/////8AA/'+
			    'wAAP////AAA/'+
			    'wAAAP//AAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    'wAAAAAAAAAA/'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '/////AAD////'+
			    '////AAAAD///'+
			    '///wAAAAAP//'+
			    '//+AAAAAAB//'+
			    '//wAAAAAAAP/'+
			    '//AAAAAAAAD/'+
			    '/8AAAAAAAAA/'+
			    '/wAAAAAAAAAP'+
			    '/gAAAAAAAAAH'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/AAAAAAAAAAD'+
			    '/gAAAAAAAAAH'+
			    '/wAAAAAAAAAP'+
			    '/8AAAAAAAAA/'+
			    '//AAAAAAAAD/'+
			    '//wAAAAAAAP/'+
			    '//+AAAAAAB//'+
			    '///wAAAAAP//'+
			    '////AAAAD///'+
			    '/////AAD////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    '////////////'+
			    'w==\x1b\\');
		}
		w('\x1b_SyncTERM:C;LoadPBM;SyncTERM.pbm\x1b\\');

		// Get the screen dimensions
		w('\x1b[?2;1S');
		var dim = read_dim()

		// Copy current screen...
		w('\x1b_SyncTERM:P;Copy;B=0\x1b\\');

		var imgdim = {width:64, height:64};
		var pos = {x:random(dim.width - imgdim.width - 8) + 4, y:random(dim.height - imgdim.height - 8) + 4};
		var remain;
		var dir = {x:4 * (random(1) ? -1 : 1), y:2 * (random(1) ? -1 : 1)};

		while (console.inkey(1) == '') {
			mswait(10);
			pos.x += dir.x;
			pos.y += dir.y;
			if (pos.x - 4 < 0) {
				dir.x = 4;
				pos.x += 4;
			}
			if (pos.x + 4 + imgdim.width + 8 >= dim.width) {
				dir.x = -4;
				pos.x -= 4;
			}
			if (pos.y - 4 < 0) {
				dir.y = 2;
				pos.y += 2;
			}
			if (pos.y + 4 + imgdim.height + 8 >= dim.height) {
				dir.y = -2;
				pos.y -= 2;
			}
			// Draw in new location
			w('\x1b_SyncTERM:P;Paste;DX='+pos.x+';DY='+pos.y+';MBUF;B=1\x1b\\');
			// Erase old location
			w('\x1b_SyncTERM:P;Paste;SX='+(pos.x - 4)+';SY='+(pos.y - 4)+';SW='+(imgdim.width + 8)+';SH='+(imgdim.height + 8)+';DX='+(pos.x - 4)+';DY='+(pos.y - 4)+';MBUF;MY=64\x1b\\');
		}
		// Erase final
		w('\x1b_SyncTERM:P;Paste;SX=' + pos.x + ';SY=' + pos.y + ';SW=' + imgdim.width + ';SH=' + imgdim.height + ';DX=' + pos.x + ';DY=' + pos.y + '\x1b\\');
		console.ctrlkey_passthru = opt;
	}
}
console.line_counter=0;
console.ungetstr(' ');
