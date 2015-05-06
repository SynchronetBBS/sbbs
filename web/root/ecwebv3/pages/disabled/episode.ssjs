//HIDDEN:Episode
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
var index = http_request.query.episode[0]-1;
if (index < 0 || index > hdrs.length) {
	print("Invalid episode!");
	exit(1);
}
var item_info = podcast_get_info(base, hdrs[index]);
print('<h1>'+item_info.title+'</h1>');
print('<audio controls="controls"><source src="'+item_info.enclosure+'"></audio>');
print('<p>'+item_info.description+'</p>');
