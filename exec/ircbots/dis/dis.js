// Comment these...
//Bot_Commands={};
//function Bot_Command(x,y,z){}
if(!js.global || js.global.MSG_DELETE==undefined)
	js.global.load("sbbsdefs.js");

function get_posts_by(name, subs)
{
	var txt='';
	var sub;
	var mb;
	var i,j;
	var msgs;
	var crc=crc16_calc(name.toLowerCase());
	var idx;
	var hdr;
	var body;
	var all={};

	if(subs==undefined) {
		subs=[];
		for(sub in msg_area.sub) {
			if(msg_area.sub[sub].settings & SUB_LZH)
				continue;
			if(sub=='syncprog')
				continue;
			if(sub.search(/^fidosync/)!=-1)
				continue;
			subs.push(sub);
		}
	}

	for(sub in subs) {
		mb=new MsgBase(subs[sub]);
		if(mb.open()) {
			msgs=mb.total_msgs;
			for(i=0; i<msgs; i++) {
				idx=mb.get_msg_index(true, i);
				if (idx == null)
					continue;
				if(idx.attr & MSG_DELETE)
					continue;
				if(idx.from==crc) {
					hdr=mb.get_msg_header(true, i);
					if(hdr.from.toLowerCase()==name.toLowerCase()) {
						body=mb.get_msg_body(true, i, false, false, false).split(/\r?\n/);
						for(j=0; j<body.length; j++) {
							try {
								if(all[body[j]] !== undefined) {
									body.splice(j,1);
									j--;
									continue;
								}
								all[body[j]]='';
								js.gc(false);
							} catch (e) { log("Out of memory "+e); }
						}
						body=word_wrap(body.join('\n', 65535)).split(/\n/);
						for(j=body.length-1; j>=body.length/2; j--) {
							if(body[j].search(/^\s*$/)==0) {
								body=body.slice(0, j);
								break;
							}
							if(body[j].search(/^\s{20}/)==0) {
								body=body.slice(0, j);
								break;
							}
						}
						for(j in body) {
							if(body[j].search(/[\x00-\x1f\x7F-\xFF]/)!=-1)
								continue;
							if(body[j].search(/[0-9]{3}-[0-9]{3}-[0-9]{4}/)!=-1)
								continue;
							if(body[j].search(/^\s*[^a-zA-Z]/)!=-1)
								continue;
							if(body[j].search(/ "Real Fact" #[0-9]+:/)!=-1)
								break;
							if(body[j].search(/s*[\w]{0,2}[>:] /)==-1) {
								txt += body[j]+' ';
								js.flatten_string(txt);
							}
						}
					}
				}
			}
		}
	}
	return txt;
}

Bot_Commands["QUOTE"] = new Bot_Command(0, false, false);
Bot_Commands["QUOTE"].command = function (target, onick, ouh, srv, lbl, cmd) {
	// Remove empty cmd args
	for(i=1; i<cmd.length; i++) {
		if(cmd[i].search(/^\s*$/)==0) {
			cmd.splice(i,1);
			i--;
		}
	}

	if(cmd.length == 1)
		return true;

	var posts=get_posts_by(cmd.slice(1).join(' '));
	if(posts.search(/[^\.][\.\!\?]\s+(.*?[^.][\.\!\?])[^.]/)==-1)
		srv.o(target, "Nothing quotable ever posted by "+cmd.slice(1).join(' '));
	else {
		var m=null;
		while(m==null) {
			m=posts.substr(random(posts.length)).match(/[^\.][\.\!\?]\s+(.*?[^.][\.\!\?])[^.]/);
		}
		srv.o(target, cmd.slice(1).join(' ')+': "'+m[1]+'"');
	}
}

Bot_Commands["DIS"] = new Bot_Command(0, false, false);
Bot_Commands["DIS"].command = function (target, onick, ouh, srv, lbl, cmd) {
	function dissociate(in_txt, arg, iteration_limit) {
		function clean_string(str)
		{
			str=str.quote().replace(/^"(.*)"$/,"$1");
			str=str.replace(/[\(\)\^\$\*\+\?\.\{\}\[\]]/g,'\\$1');
		}

		if(arg===undefined)
			arg=2;
		if(iteration_limit===undefined)
			iteration_limit=100;
		var out='';
		var last_match_point;
		in_txt=in_txt.replace(/[\s\x00-\x1f]/,' ');
	 
		var new_matcher;
		var i;
		var deads = 0;
		new_matcher = "[\\.\\?\\!]\\s+(";
		for(i=0;i<arg;i++) {
			if(i)
				new_matcher += '\\s+';
			new_matcher += '[^\\s]+';
		}
		new_matcher += ')';

		if(in_txt.substr(pos).search(new RegExp(new_matcher, 'i'))==-1)
			return 'No sentences found in any of ths posts.';

		// In use in the loop.
		var re,orig=[],matched,remainder,last_matched='',iteration=0,pos=0;

		last_match_point = -1;
		while(out.length < 510 && deads < iteration_limit) {
			iteration++;
			do {
				pos = random(in_txt.length);
				while(in_txt.charAt(pos)!=' ' && pos < in_txt.length)
					pos++;
				while(in_txt.charAt(pos)!==' ' && pos < in_txt.length)
					pos++;
			} while(pos >= in_txt.length || (pos<=last_match_point && pos >=last_match_point-12));
			if(last_matched) { // last thing we matched -- '' means take a stab
				last_match_point = pos;
				orig = last_matched.match(/([^\s]+)/).slice(1).map(clean_string);
				if(out=='' || out.search(/[\.\?\!]\s*$/)!=-1) {
					if(orig.length==0)
						orig.push('');
					if(orig[0]==undefined)
						orig[0]='';
					orig[orig.length-1]+='[\\.\\?\\!]';
				}
				re = new RegExp("\\b" + orig.join("\\s+") + "\\s+([^\\s]+" + (new Array(arg)).join("\\s+[^\\s]+") + ")(\\s+)",'i');
				matched = remainder = '';
				last_match_point = pos;

				m = in_txt.substr(pos).match(re);
				if(m==null) {
					pos = 0;
					m = in_txt.match(re);
				}
				if(m != null) {
					matched = m[1];
					remainder = m[2];
					pos += m.index + m[0].length;
				}

				if( last_match_point == pos ||  pos == 0) {
					log(LOG_DEBUG, "Hm, dead end at pos " + (0 + pos));
					last_matched = '';
					continue;
				}

				last_matched = matched;
				log(LOG_DEBUG, "Matched ("+matched+") at "+(pos));
				out += last_matched + remainder;
				if(out.search(/[\.\!\?\;]\s*$/)!=-1) {
					if(iteration >= iteration_limit)
						break;
				}
				continue;
			} else {
				// We don't have a last_matched -- take a stab.

				log(LOG_DEBUG, "Taking a stab at pos "+(pos));
				m=in_txt.substr(pos).match(new RegExp(new_matcher, 'i'));
				if(m==null) {
					pos=0;	// We assume the start is the beginning of a sentence.
					m=in_txt.match(new RegExp(new_matcher, 'i'));
				}
				if(m != null) {
					pos += m.index + m[0].length;
					last_matched = m[1];
				} else {
					log(LOG_DEBUG, "Can't get an initial "+(arg)+"-token match");
					break;
				}
				deads++;
			}
		}

		return out.replace(/\s{3,}/g,'  ').replace(/(\b(.*?)\s)\s*?\2\s+/g,'$1');
	}

	// Remove empty cmd args
	for(i=1; i<cmd.length; i++) {
		if(cmd[i].search(/^\s*$/)==0) {
			cmd.splice(i,1);
			i--;
		}
	}

	if(cmd.length == 1)
		return true;

	var posts=get_posts_by(cmd.slice(1).join(' '));
	if(posts.length < 200)
		srv.o(target, "Not enough posts by "+cmd.slice(1).join(' '));
	else
		srv.o(target, dissociate(posts,2,random(5)+10));

	return true;
}

//var dumb={o:function(x,y) {log(y);}};
//Bot_Commands["DIS"].command(undefined, undefined, undefined,dumb,undefined,['DIS','deuce']);
