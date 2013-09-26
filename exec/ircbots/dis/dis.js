// Comment these...
//Bot_Commands={};
//function Bot_Command(x,y,z){}
if(!js.global || js.global.MSG_DELETE==undefined)
	js.global.load("sbbsdefs.js");

Bot_Commands["DIS"] = new Bot_Command(0, false, false);
Bot_Commands["DIS"].command = function (target, onick, ouh, srv, lbl, cmd) {
	function dissociate(in_txt, arg, iteration_limit) {
		function clean_string(str)
		{
			str=str.quote().replace(/^"(.*)"$/,"$1");
			str=str.replace(/[\(\)\^\$\*\+\?\.\{\}\[\]]/g,'\\$1');
		}

		if(arg===undefined | arg==1)
			arg=2;
		if(iteration_limit===undefined)
			iteration_limit=100;
		var out='';
		var last_match_point;
		in_txt=in_txt.replace(/[\s\x00-\x1f]/,' ');
	 
		var new_matcher;
		var i;
		new_matcher = "\\s+(";
		for(i=0;i<arg;i++) {
			if(i)
				new_matcher += '\\s+';
			new_matcher += '[^\\s]+';
		}
		new_matcher += ')';

		// In use in the loop.
		var re,orig=[],matched,remainder,last_matched,iteration=0,pos=0;

		last_match_point = -1;
		while(1) {
			iteration++;
			do {
				pos = random(in_txt.length);
				while(in_txt.charAt(pos)!=' ' && pos < in_txt.length)
					pos++;
				while(in_txt.charAt(pos)!==' ' && pos < in_txt.length)
					pos++;
			} while(pos >= in_txt.length);
			if(last_matched) { // last thing we matched -- '' means take a stab
				last_match_point = pos;
				orig = last_matched.match(/([^\s]+)/).slice(1).map(clean_string);
				re = new RegExp("\\b" + orig.join("\\s+") + "\\s+([^\\s]+" + (new Array(degree)).join("\\s+[^\\s]+") + ")(\\s+)",'i');
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
				var frame, frame_size;
				frame_size =  (arg + 3) * 8;
				// Generously assume 8 chars per word.

				pos = random(in_txt.length - frame_size);
				log(LOG_DEBUG, "Taking a stab at pos "+(pos));
				m=in_txt.substr(pos).match(new RegExp(new_matcher, 'i'));
				if(m==null) {
					pos=0;
					m=in_txt.match(new RegExp(new_matcher, 'i'));
				}
				if(m != null) {
					pos += m.index + m[0].length;
					last_matched = m[1];
				} else {
					log(LOG_DEBUG, "Can't get an initial "+(arg)+"-token match");
					break;
				}
			}
		}

		return out.replace(/\s{3,}/,'  ');
	}

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
					if(idx.attr & MSG_DELETE)
						continue;
					if(idx.from==crc) {
						hdr=mb.get_msg_header(true, i);
						if(hdr.from.toLowerCase()==name.toLowerCase()) {
							body=mb.get_msg_body(true, i, false, false, false);
							body=word_wrap(body, 65535).split(/\r?\n/);
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
								if(body[j].search(/s*[\w]{0,2}[>:] /)==-1)
									txt += body[j]+' ';
							}
						}
					}
				}
			}
		}
		return txt;
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
		srv.o(target, dissociate(posts,3,random(5)+10));

	return true;
}

//var dumb={o:function(x,y) {log(y);}};
//Bot_Commands["DIS"].command(undefined, undefined, undefined,dumb,undefined,['DIS','deuce']);
