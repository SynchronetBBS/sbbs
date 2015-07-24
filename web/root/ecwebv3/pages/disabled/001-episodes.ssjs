// Episode List
if (js.global.podcast_get_info == undefined)
	js.global.load("podcast_routines.js");
if (js.global.podcast_opts == undefined) {
	js.global.podcast_opts = load({}, "modopts.js", "Podcast");
}

var base = new MsgBase(podcast_opts.Sub);
if (base == null) {
	print('Error creating message base object!');
	exit(1);
}
if (!base.open()) {
	print('Error opening message base!');
	exit(1);
}
var hdrs = podcast_load_headers(base, podcast_opts.From, podcast_opts.To);
for (i in hdrs) {
	var item_info = podcast_get_info(base, hdrs[i]);
	if (item_info == undefined)
		continue;
	print('<a href="index.xjs?page=episode.ssjs&episode='+(parseInt(i)+1)+'">'+strftime("%B %e %Y", hdrs[i].when_written_time) + " " + item_info.title+'</a><br>');
	print('<p>'+item_info.description+'</p>');
}
