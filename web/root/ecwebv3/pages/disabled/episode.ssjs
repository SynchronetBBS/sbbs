//HIDDEN:Episode

load('webInit.ssjs');
load(webIni.WebDirectory + "/lib/forum.ssjs");

// TODO: Move into exec/load somewhere...
function encode_xml(str)
{
	str = str.toString();
	str=str.replace(/&/g, '&amp;');
	str=str.replace(/</g, '&lt;');
	str=str.replace(/>/g, '&gt;');
	str=str.replace(/"/g, '&quot;');
	str=str.replace(/'/g, '&apos;');
	return str;
}

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
var all_hdrs={};
var hdrs = podcast_load_headers(base, podcast_opts.From, podcast_opts.To, all_hdrs);
var index = http_request.query.episode[0]-1;
if (index < 0 || index > hdrs.length) {
	print("Invalid episode!");
	exit(1);
}
var item_info = podcast_get_info(base, hdrs[index]);
if (item_info == undefined) {
	print("Invalid episode!");
	exit(1);
}
print('<h1>'+strftime("%B %e %Y", hdrs[index].when_written_time) + " " + item_info.title+'</h1>');
print('<audio controls="controls"><source src="'+item_info.enclosure.url+'"></audio>');
print('<p>'+item_info.description+'</p>');
print('<h2>Comments</h2>');
var i;
var body;
for (i in all_hdrs) {
	if (all_hdrs[i].thread_id != hdrs[index].thread_id)
		continue;
	if (all_hdrs[i].thread_id == i)
		continue;
	print('<div>'+html_encode(all_hdrs[i].subject,false, false, false)+' &mdash; <i>'+all_hdrs[i].from+'</i> '+system.timestr(all_hdrs[i].when_written_time)+' '+system.zonestr(all_hdrs[i].when_written_zone)+'<br>');
	body = base.get_msg_body(all_hdrs[i].number, true);
	body = word_wrap(body, 65535, 79);
	body = html_encode(body, false, false, false);
	body = body.replace(/\r?\n/g, '<br>');
	print(body);
	print('</div><br>');
}
if (user.number != 0 && msg_area.sub[base.cfg.code].can_post) {
	print('<div id="comment-post">');
	print('<form id="comment-form">');
	print('<input type="hidden" name="postmessage" value="true" />');
	print('<input type="hidden" name="sub" value="'+base.cfg.code+'" />');
	print('<input type="hidden" name="irt" value="'+hdrs[index].number+'" />');
	print('<input type="hidden" name="to" value="'+hdrs[index].to+'" />');
	print('<input type="hidden" name="from" value="'+user.alias+'" />');
	print('Subject: <input class="border" type="text" name="subject" value="'+hdrs[index].subject+'" />');
	print('<textarea class="border" name="body" cols="80" rows="10"></textarea><br />');
	print('<input type="button" value="Submit" onClick="submitForm(\'http://'+http_request.host+'/'+webIni.appendURL+'forum-async.ssjs\', \'comment-post\', \'comment-form\')">');
	print('</form>');
	print('</div>');
}
