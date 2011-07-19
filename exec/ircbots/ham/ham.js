if(!js.global || js.global.HTTPRequest==undefined)
	load("http.js");

//Bot_Commands={};
//function Bot_Command(x,y,z){}

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

	function USCallsign(callsign, srv, target) {
		var i;
		var CookieRequest=new HTTPRequest();
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

		m=req.body.match(/(license.jsp\?licKey=[^\s]*)/);

		if(m) {
			var response=new HTTPRequest().Get('http://wireless2.fcc.gov/UlsApp/UlsSearch/'+m[1]);
			m=response.match(/In the case of City, state and zip[^-]*?-->([\x00-\xff]*?)<\/td>/);
			if(m) {
				var info=callsign+':';
				m[1]=m[1].replace(/<[^>]*>/g,'');
				m[1]=m[1].replace(/[\r\n]/g,' ');
				m[1]=m[1].replace(/\s+/g,' ');
				info += m[1];

				m=response.match(/>Type<\/td>[\x00-\xff]*?>([^<&]*)[<&]/);
				if(m) {
					m[1]=m[1].replace(/<[^>]*>/g,'');
					m[1]=m[1].replace(/[\r\n]/g,' ');
					m[1]=m[1].replace(/\s+/g,' ');
					info += ' -  Type: '+m[1];
				}

				m=response.match(/<!--Example: Amateur Extra -->([^&<]*?)[&<]/);
				if(m) {
					m[1]=m[1].replace(/<[^>]*>/g,'');
					m[1]=m[1].replace(/[\r\n]/g,' ');
					m[1]=m[1].replace(/\s+/g,' ');
					info += ' -  Class: '+m[1];
				}

				m=response.match(/>Status<\/td>[\x00-\xff]*?>([^<&]*)[<&]/);
				if(m) {
					m[1]=m[1].replace(/<[^>]*>/g,'');
					m[1]=m[1].replace(/[\r\n]/g,' ');
					m[1]=m[1].replace(/\s+/g,' ');
					info += ' ('+m[1]+')';
				}
				srv.o(target,info);
			}
			else {
				srv.o(target, "Unable to parse US callsign info!");
			}
		}
		else {
			srv.o(target, "Unable to match US callsign!");
		}
	}

	function CanadaCallsign(callsign, srv, target) {
		var result=new HTTPRequest().Post('http://www.callsign.ca/ind.php','indicatif='+callsign);
		var m=result.match(/<H5>\s*([\x00-\xff]*?)<\/H5/);
		if(m!=null) {
			m[1]=m[1].replace(/<[^>]*>/g,'');
			m[1]=m[1].replace(/[\r\n]/g,' ');
			m[1]=m[1].replace(/\s+/g,' ');
			srv.o(target, m[1]);
		}
		else {
			srv.o(target, "Unable to match Canadian callsign!");
		}
	}

	if(callsign.search(/(C[F-K]|C[Y-Z]|V[A-GOXY]|X[J-O])/)==0) {
		CanadaCallsign(callsign, srv, target);
	}
	else if(callsign.search(/(A[A-L]|K|N|W)/)==0) {
		USCallsign(callsign,srv,target);
	}
	else {
		srv.o(target,"Callsign country not supported.");
	}

	return true;
}

//var dumb={o:function(x,y) {log(y);}};
//Bot_Commands["CALLSIGN"].command(undefined, undefined, undefined,dumb,undefined,['asdf','kj6pxy']);
//Bot_Commands["CALLSIGN"].command(undefined, undefined, undefined,dumb,undefined,['asdf','va6rrx']);
