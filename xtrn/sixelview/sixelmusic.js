/* sixelmusic.js
 *
 * Play a music file (MP3, Ogg Vorbis, Opus, FLAC, WAV, AIFF or AU) in SyncTERM, with the other music files in the same
 * directory as a playlist.  Written entirely in Synchronet JS; see sixelmusic_lib.js.
 *
 * Usage:  ?sixelmusic <file>		play that file, then the ones after it
 *         ?sixelmusic <directory>	play every music file in the directory
 *
 * Tab size is 4.
 */

"use strict";

load("sbbsdefs.js");
load("key_defs.js");
load("sixelview_lib.js");
load("sixelmusic_lib.js");

function main()
{
	var arg = argv.length > 0 ? argv.join(" ") : "";
	if (!arg.length) {
		console.print("\x01n\x01cusage: \x01h?sixelmusic <file or directory>\x01n\r\n");
		return;
	}
	var path = arg;
	if (!file_exists(path) && !file_isdir(path)) {
		console.print("\x01n\x01h\x01rNo such file or directory.\x01n\r\n");
		return;
	}
	var pl;
	if (file_isdir(path)) {
		path = backslash(path);
		pl = music_playlist_for(path + "x");
		if (!pl.list.length || pl.list[0].path == path + "x") {
			console.print("\x01n\x01h\x01rNo music files in that directory.\x01n\r\n");
			return;
		}
		pl.index = 0;
	}
	else
		pl = music_playlist_for(path);
	try {
		music_player(pl.list, pl.index);
	}
	finally {
		mouse_stop();
	}
}

main();
