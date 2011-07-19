if(!js.global || js.global.HTTPRequest==undefined)
	load("http.js");

Bot_Commands["CALLSIGN"] = new Bot_Command(0,false,false);
Bot_Commands["CALLSIGN"].command = function (target,onick,ouh,srv,lvl,cmd) {
	var callsign;
	var i;

	// Remove empty cmd args
	for(i=1; i<cmd.length; i++) {
		if(cmd[i].search(/^\s*$/)==0) {
			cmd.splice(i,1);
			i--;
		}
	}

	if(cmd.length==2)
		callsign=cmd[1].toUpperCase();
	else {
		srv.o(target,"Usage: callsign <sign>");
		return true;
	}

	var CookieRequest=new HTTPRequest;
	var m=CookieRequest.Get('http://wireless2.fcc.gov/UlsApp/UlsSearch/searchLicense.jsp');
	var jsess='';
	var cookies=['refineIndex=0'];
	for(i in CookieRequest.response_headers) {
		if(CookieRequest.response_headers[i].search(/^Set-Cookie:/i)!=-1) {
			if(m=CookieRequest.response_headers[i].match(/^Set-Cookie:\s*(.*?);/)) {
				cookies.push(m[1]);
				if(m[1].search(/JSESSIONID_/)==0)
					jsess=m[1];
			}
		}
	}
	req=new HTTPRequest();
	req.SetupPost('http://wireless2.fcc.gov/UlsApp/UlsSearch/results.jsp;'+jsess, undefined, undefined, 'fiUlsSearchByType=uls_l_callsign++++++++++++++++&fiUlsSearchByValue='+callsign+'&fiUlsExactMatchInd=Y&hiddenForm=hiddenForm&jsValidated=true&x=0&y=0');
	req.request_headers.push("Cookie: "+cookies.join('; '));
	req.request_headers.push("Referer: http://wireless2.fcc.gov/UlsApp/UlsSearch/searchLicense.jsp");
	req.SendRequest();
	req.ReadResponse();

	//m=req.body.match(/<a href=license.jsp\?.*?>\s*([^<]+)<.*?nowrap>\s*(^[<]*?)\s*<.*?nowrap>\s*(^[<]*?)\s*<.*?nowrap>\s*(^[<]*?)\s*<.*?nowrap>\s*(^[<]*?)\s*<.*?nowrap>\s*(^[<]*?)\s*</);
	m=req.body.match(/<a href=license.jsp\?.*?>\s*([^<]+)\s*<[\x00-\xff]*?nowrap>\s*([^<]*?)\s*<[\x00-\xff]*?nowrap>\s*([^<]*?)\s*<[\x00-\xff]*?nowrap>\s*([^<]*?)\s*<[\x00-\xff]*?nowrap>\s*([^<]*?)\s*<[\x00-\xff]*?nowrap>\s*([^<]*?)\s*</);

	if(m) {
		srv.o(target, callsign+" is : "+m[2]+" - "+m[5]+", Expiry: "+m[6]);
	}
	else {
		srv.o(target, "Unable to match callsign!");
	}

	return true;
}
