function USCallsign(callsign) {
	var ret={callsign:callsign};
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

	m=req.body.match(/license.jsp\?licKey=[^\s]*/g);

	if(m && m.length) {
		var response=new HTTPRequest().Get('http://wireless2.fcc.gov/UlsApp/UlsSearch/'+m[m.length-1]);
		m=response.match(/In the case of City, state and zip[^-]*?-->([\s\S]*?)<\/td>/);
		if(m) {
			m[1]=m[1].replace(/<[^>]*>/g,'');
			m[1]=m[1].replace(/[\r\n]/g,' ');
			m[1]=m[1].replace(/\s+/g,' ');
			ret.address=m[1];

			m=response.match(/>Type<\/td>[\s\S]*?>([^<&]*)[<&]/);
			if(m) {
				m[1]=m[1].replace(/<[^>]*>/g,'');
				m[1]=m[1].replace(/[\r\n]/g,' ');
				m[1]=m[1].replace(/\s+/g,' ');
				ret.type=m[1];
			}

			m=response.match(/<!--Example: Amateur Extra -->([^&<]*?)[&<]/);
			if(m) {
				m[1]=m[1].replace(/<[^>]*>/g,'');
				m[1]=m[1].replace(/[\r\n]/g,' ');
				m[1]=m[1].replace(/\s+/g,' ');
				ret.class=m[1];
			}

			m=response.match(/>Status<\/td>[\s\S]*?>([^<&]*)[<&]/);
			if(m) {
				m[1]=m[1].replace(/<[^>]*>/g,'');
				m[1]=m[1].replace(/[\r\n]/g,' ');
				m[1]=m[1].replace(/\s+/g,' ');
			}
			return ret;
		}
	}
	throw("No ULS results");
}

function CanadaCallsign(callsign) {
	var result=new HTTPRequest().Post('http://apc-cap.ic.gc.ca/pls/apc_anon/query_amat_cs$callsign.actionquery','P_CALLSIGN='+callsign+'&P_SURNAME=&P_CITY=&P_PROV_STATE_CD=&P_POSTAL_ZIP_CODE=&Z_ACTION=QUERY&Z_CHK=0');
	var re=new RegExp("<a href=\"(query_amat_cs\\$callsign\\.QueryViewByKey\\?P_CALLSIGN="+callsign.toUpperCase()+"&amp;Z_CHK=[0-9]+)\">"+callsign.toUpperCase()+"</a>");
	var m=result.match(re);
	if(m!=null) {
		var ret={callsign:callsign};
		result=new HTTPRequest().Get('http://apc-cap.ic.gc.ca/pls/apc_anon/'+m[1].replace(/&amp;/g, '&'));
		result=result.replace(/&nbsp;/g,' ');
		result=result.replace(/\s+/g,' ');
		m=result.match(/Call Sign:[\s\S]*?\<td>([^<]+)<[\s\S]*?Amateur Name:[\s\S]*?\<td>([^<]+)<[\s\S]*?Address:[\s\S]*?\<td>([^<]+)<[\s\S]*?City:[\s\S]*?\<td>([^<]+)<[\s\S]*?Province:[\s\S]*?\<td>([^<]+)<[\s\S]*?Postal Code:[\s\S]*?\<td>([^<]+)<[\s\S]*?Qualifications:[\s\S]*?\<td>([^<]+)</);
		if(m!=null) {
			ret.callsign=m[1];
			ret.name=m[2];
			ret.address=m[3];
			ret.city=m[4];
			ret.province=m[5];
			ret.postalzip=m[6];
			ret.qualifications=m[7];
			return ret;
		}
	}
	throw("No IC results");
}

function HamcallCallsign(callsign) {
	var req=new HTTPRequest();
	var config = new File(system.ctrl_dir + js.global.config_filename);
	if(!config.open('r')) {
		throw("Unable to open config!");
	}

	req.SetupGet('http://hamcall.net/call?callsign='+callsign);
	req.request_headers.push("Cookie: callsign="+config.iniGetValue("module_Ham", 'callsign')+'; password='+config.iniGetValue("module_Ham", 'password'));
	req.SendRequest();
	req.ReadResponse();

	config.close();

	var m=req.body.match(/Tell them you found it on HamCall.net, the world's largest callsign database!.*?<BR>([\s\S]*?)(<font size|<\/td>)/);
	if(m) {
		m[1]=m[1].replace(/<[^>]*>/g,' ');
		m[1]=m[1].replace(/[\r\n]/g,' ');
		m[1]=m[1].replace(/\s+/g,' ');
		m[1] += ' (Found on HamCall.net)';
		return {callsign:callsign,string:m[1]};
	}
	throw("No match");
}

function USSpecialEvent(callsign) {
	var result=new HTTPRequest().Post('http://www.arrl.org/special-event-stations', '_method=POST&data%5BSearch%5D%5Bcall_sign%5D='+callsign+'&data%5BSearch%5D%5Bkeywords%5D=&data%5BLocation%5D%5Bzip%5D=&data%5BLocation%5D%5Barea%5D=&data%5BLocation%5D%5Bcity%5D=&data%5BLocation%5D%5Bstate%5D=&data%5BLocation%5D%5Bdivision_id%5D=&data%5BLocation%5D%5Bsection_id%5D=&data%5BLocation%5D%5Bcountry%5D=&data%5BDate%5D%5Bstart%5D=&data%5BDate%5D%5Bend%5D=');
	var m=result.match(/<h3>\s*([\s\S]*?)<\/p/);
	if(m!=null) {
		m[1]=m[1].replace(/<[^>]*>/g,'');
		m[1]=m[1].replace(/[\r\n]/g,' ');
		m[1]=m[1].replace(/\s+/g,' ');
		return {callsign:callsign,string:m[1]};
	}
	throw("No match");
}
