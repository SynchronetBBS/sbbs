if(!js.global || js.global.HTTPRequest==undefined)
	js.global.load("http.js");

var CallSign={
	Lookup:{
		US:function(callsign) {
			var ret={callsign:callsign.toUpperCase()};
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
					m[1]=m[1].replace(/[\r\n]/g,' ');
					m[1]=m[1].replace(/\s+/g,' ');
					var parts=m[1].split(/\s*<br>\s*/);
					m[1]=m[1].replace(/<[^>]*>/g,'');
					ret.address=m[1];
					ret.name=parts.shift().replace(/^\s*(.*?)\s*$/,"$1");
					if(parts[parts.length-1].search(/^\s*$/)==0) {
						parts.pop();
					}
					if(parts[parts.length-1].search(/^ATTN /)==0) {
						ret.attn=parts.pop().replace(/^ATTN /,'');
					}
					m=parts[parts.length-1].match(/^(.*),\s+([A-Z]{2})\s+([0-9]{5}(?:-[0-9]{4})?)\s*$/);
					if(m) {
						parts.pop();
						ret.city=m[1];
						ret.provstate=m[2];
						ret.postalzip=m[3];
						ret.address=parts.join(', ');
					}

					m=response.match(/>Type<\/td>[\s\S]*?>\s*([^<&]*?)\s*[<&]/);
					if(m) {
						m[1]=m[1].replace(/<[^>]*>/g,'');
						m[1]=m[1].replace(/[\r\n]/g,' ');
						m[1]=m[1].replace(/\s+/g,' ');
						ret.type=m[1];
					}

					m=response.match(/<!--Example: Amateur Extra -->\s*([^&<]*?)\s*[&<]/);
					if(m) {
						m[1]=m[1].replace(/<[^>]*>/g,'');
						m[1]=m[1].replace(/[\r\n]/g,' ');
						m[1]=m[1].replace(/\s+/g,' ');
						if(m[1] != '')
							ret.class=m[1];
					}

					m=response.match(/>Status<\/td>[\s\S]*?>\s*([^<&]*?)\s*[<&]/);
					if(m) {
						m[1]=m[1].replace(/<[^>]*>/g,'');
						m[1]=m[1].replace(/[\r\n]/g,' ');
						m[1]=m[1].replace(/\s+/g,' ');
						ret.status=m[1];
					}
					if(ret.class==undefined)
						ret.qualifications=ret.type;
					else {
						ret.qualifications=ret.class;
						m=ret.name.match(/^(.*), (.*)$/);
						if(m)
							ret.name=m[2]+' '+m[1];
					}
					return ret;
				}
			}
			throw("No ULS results");
		},

		Canada:function(callsign) {
			var result=new HTTPRequest().Post('http://apc-cap.ic.gc.ca/pls/apc_anon/query_amat_cs$callsign.actionquery','P_CALLSIGN='+callsign+'&P_SURNAME=&P_CITY=&P_PROV_STATE_CD=&P_POSTAL_ZIP_CODE=&Z_ACTION=QUERY&Z_CHK=0');
			var re=new RegExp("<a href=\"(query_amat_cs\\$callsign\\.QueryViewByKey\\?P_CALLSIGN="+callsign.toUpperCase()+"&amp;Z_CHK=[0-9]+)\">"+callsign.toUpperCase()+"</a>");
			var m=result.match(re);
			if(m!=null) {
				var ret={callsign:callsign.toUpperCase()};
				result=new HTTPRequest().Get('http://apc-cap.ic.gc.ca/pls/apc_anon/'+m[1].replace(/&amp;/g, '&'));
				result=result.replace(/&nbsp;/g,' ');
				result=result.replace(/\s+/g,' ');
				m=result.match(/Call Sign:[\s\S]*?\<td>([^<]+?)\s*<[\s\S]*?Amateur Name:[\s\S]*?\<td>([^<]+?)\s*<[\s\S]*?Address:[\s\S]*?\<td>([^<]+?)\s*<[\s\S]*?City:[\s\S]*?\<td>([^<]+?)\s*<[\s\S]*?Province:[\s\S]*?\<td>([^<]+?)\s*<[\s\S]*?Postal Code:[\s\S]*?\<td>([^<]+?)\s*<[\s\S]*?Qualifications:[\s\S]*?\<td>([^<]+?)\s*</);
				if(m!=null) {
					ret.callsign=m[1];
					ret.name=m[2];
					ret.address=m[3];
					ret.city=m[4];
					ret.provstate=m[5];
					ret.postalzip=m[6];
					ret.qualifications=m[7];
					return ret;
				}
			}
			throw("No IC results");
		},

		Hamcall:function(callsign) {
			var req=new HTTPRequest();
			var config = js.global.load("modopts.js","hamcall");

			req.SetupGet('http://hamcall.net/call?callsign='+callsign);
			req.request_headers.push("Cookie: callsign="+config.callsign+'; password='+config.password);
			req.SendRequest();
			req.ReadResponse();

			var m=req.body.match(/Tell them you found it on HamCall.net, the world's largest callsign database!.*?<BR>\s*([\s\S]*?)\s*(?:<font size|<\/td>)/);
			if(m) {
				m[1]=m[1].replace(/<[^>]*>/g,' ');
				m[1]=m[1].replace(/[\r\n]/g,' ');
				m[1]=m[1].replace(/\s+/g,' ');
				m[1]=m[1].replace(/^\s*(.*?)\s*$/g,'$1');
				m[1] += ' (Found on HamCall.net)';
				return {callsign:callsign.toUpperCase(),string:callsign.toUpperCase()+': '+m[1]};
			}
			throw("No match");
		},

		USSpecialEvent:function(callsign) {
			var result=new HTTPRequest().Post('http://www.arrl.org/special-event-stations', '_method=POST&data%5BSearch%5D%5Bcall_sign%5D='+callsign+'&data%5BSearch%5D%5Bkeywords%5D=&data%5BLocation%5D%5Bzip%5D=&data%5BLocation%5D%5Barea%5D=&data%5BLocation%5D%5Bcity%5D=&data%5BLocation%5D%5Bstate%5D=&data%5BLocation%5D%5Bdivision_id%5D=&data%5BLocation%5D%5Bsection_id%5D=&data%5BLocation%5D%5Bcountry%5D=&data%5BDate%5D%5Bstart%5D=&data%5BDate%5D%5Bend%5D=');
			var m=result.match(/<h3>\s*([\s\S]*?)<\/p/);
			if(m!=null) {
				m[1]=m[1].replace(/<[^>]*>/g,'');
				m[1]=m[1].replace(/[\r\n]/g,' ');
				m[1]=m[1].replace(/\s+/g,' ');
				return {callsign:callsign.toUpperCase(),string:m[1]};
			}
			throw("No match");
		},

		Any:function(callsign) {
			var country;
			var matched;
			try {
				country=CallSign.Country(callsign);
			}
			catch(e) {
				throw("Failed to locate callsign country");
			}
			if(country == 'Canada') {
				try {
					matched=CallSign.Lookup.Canada(callsign);
				}
				catch(e) {}
			}
			else if(country == 'United States of America') {
				if(matched==undefined && callsign.search(/^\s*(K|N|W)[0-9].\s*$/)==0) {
					try {
						matched=CallSign.Lookup.USSpecialEvent(callsign);
					}
					catch(e) {}
				}
				if(matched==undefined) {
					try {
						matched=CallSign.Lookup.US(callsign);
					}
					catch(e) {}
				}
			}
			if(matched==undefined) {
				try {
					matched=CallSign.Lookup.Hamcall(callsign);
				}
				catch(e) {log(e)}
			}
			if(matched==undefined) {
				throw("Unable to match callsign from "+country+" in any databases.");
			}
			return matched;
		},
	},

	Country:function(callsign) {
		callsign=callsign.toUpperCase();
		if(callsign.search(/^A[A-L]/)==0)
			return 'United States of America';
		if(callsign.search(/^A[M-Z]/)==0)
			return 'Spain';
		if(callsign.search(/^A[P-S]/)==0)
			return 'Pakistan';
		if(callsign.search(/^A[T-W]/)==0)
			return 'India';
		if(callsign.search(/^AX/)==0)
			return 'Australia';
		if(callsign.search(/^A[Y-Z]/)==0)
			return 'Argentine Republic';
		if(callsign.search(/^A2/)==0)
			return 'Botswana';
		if(callsign.search(/^A3/)==0)
			return 'Tonga';
		if(callsign.search(/^A4/)==0)
			return 'Oman';
		if(callsign.search(/^A5/)==0)
			return 'Bhutan';
		if(callsign.search(/^A6/)==0)
			return 'United Arab Emirates';
		if(callsign.search(/^A7/)==0)
			return 'Qatar';
		if(callsign.search(/^A8/)==0)
			return 'Liberia';
		if(callsign.search(/^A9/)==0)
			return 'Bahrain';
		if(callsign.search(/^B/)==0)
			return 'China';
		if(callsign.search(/^C[A-E]/)==0)
			return 'Chile';
		if(callsign.search(/^C[F-K]/)==0)
			return 'Canada';
		if(callsign.search(/^C[L-M]/)==0)
			return 'Cuba';
		if(callsign.search(/^CN/)==0)
			return 'Morocco';
		if(callsign.search(/^CO/)==0)
			return 'Cuba';
		if(callsign.search(/^CP/)==0)
			return 'Bolivia';
		if(callsign.search(/^C[Q-U]/)==0)
			return 'Portugal';
		if(callsign.search(/^C[V-X]/)==0)
			return 'Uruguay';
		if(callsign.search(/^C[Y-Z]/)==0)
			return 'Canada';
		if(callsign.search(/^C2/)==0)
			return 'Nauru';
		if(callsign.search(/^C3/)==0)
			return 'Andorra';
		if(callsign.search(/^C4/)==0)
			return 'Cyprus';
		if(callsign.search(/^C5/)==0)
			return 'Gambia';
		if(callsign.search(/^C6/)==0)
			return 'Bahamas';
		if(callsign.search(/^C7/)==0)
			return 'World Meterological Organization';
		if(callsign.search(/^C[8-9]/)==0)
			return 'Mozambique';
		if(callsign.search(/^D[A-R]/)==0)
			return 'Germany';
		if(callsign.search(/^D[S-T]/)==0)
			return 'Korea';
		if(callsign.search(/^D[U-Z]/)==0)
			return 'Philippines';
		if(callsign.search(/^D[2-3]/)==0)
			return 'Angola';
		if(callsign.search(/^D4/)==0)
			return 'Cape Verde';
		if(callsign.search(/^D5/)==0)
			return 'Liberia';
		if(callsign.search(/^D6/)==0)
			return 'Comoros';
		if(callsign.search(/^D[7-9]/)==0)
			return 'Korea';
		if(callsign.search(/^E[A-H]/)==0)
			return 'Spain';
		if(callsign.search(/^E[I-J]/)==0)
			return 'Ireland';
		if(callsign.search(/^EK/)==0)
			return 'Armenia';
		if(callsign.search(/^EL/)==0)
			return 'Liberia';
		if(callsign.search(/^E[M-O]/)==0)
			return 'Ukraine';
		if(callsign.search(/^E[P-Q]/)==0)
			return 'Iran';
		if(callsign.search(/^ER/)==0)
			return 'Moldova';
		if(callsign.search(/^ES/)==0)
			return 'Estonia';
		if(callsign.search(/^ET/)==0)
			return 'Ethiopia';
		if(callsign.search(/^E[U-W]/)==0)
			return 'Belarus';
		if(callsign.search(/^EX/)==0)
			return 'Kyrgyz Republic';
		if(callsign.search(/^EY/)==0)
			return 'Tajikistan';
		if(callsign.search(/^EZ/)==0)
			return 'Turkmenistan';
		if(callsign.search(/^E2/)==0)
			return 'Thailand';
		if(callsign.search(/^E3/)==0)
			return 'Eritrea';
		if(callsign.search(/^E4/)==0)
			return 'Palestinian Authority';
		if(callsign.search(/^E5/)==0)
			return 'New Zealand - Cook Islands';
		if(callsign.search(/^E7/)==0)
			return 'Bosnia and Herzegovina';
		if(callsign.search(/^F/)==0)
			return 'France';
		if(callsign.search(/^G/)==0)
			return 'United Kingdom of Great Britain and Northern Ireland';
		if(callsign.search(/^HA/)==0)
			return 'Hungary';
		if(callsign.search(/^HB/)==0)
			return 'Switzerland';
		if(callsign.search(/^H[C-D]/)==0)
			return 'Ecuador';
		if(callsign.search(/^HE/)==0)
			return 'Switzerland';
		if(callsign.search(/^HF/)==0)
			return 'Poland';
		if(callsign.search(/^HG/)==0)
			return 'Hungary';
		if(callsign.search(/^HH/)==0)
			return 'Haiti';
		if(callsign.search(/^HI/)==0)
			return 'Dominican Republic';
		if(callsign.search(/^H[J-K]/)==0)
			return 'Colombia';
		if(callsign.search(/^HL/)==0)
			return 'Korea';
		if(callsign.search(/^HM/)==0)
			return "Democratic People's Republic of Korea";
		if(callsign.search(/^HN/)==0)
			return 'Iraq';
		if(callsign.search(/^H[O-P]/)==0)
			return 'Panama';
		if(callsign.search(/^H[Q-R]/)==0)
			return 'Honduras';
		if(callsign.search(/^HS/)==0)
			return 'Thailand';
		if(callsign.search(/^HT/)==0)
			return 'Nicaragua';
		if(callsign.search(/^HU/)==0)
			return 'El Salvador';
		if(callsign.search(/^HV/)==0)
			return 'Vatican City State';
		if(callsign.search(/^H[W-Y]/)==0)
			return 'France';
		if(callsign.search(/^HZ/)==0)
			return 'Saudi Arabia';
		if(callsign.search(/^H2/)==0)
			return 'Cyprus';
		if(callsign.search(/^H3/)==0)
			return 'Panama';
		if(callsign.search(/^H4/)==0)
			return 'Solomon Islands';
		if(callsign.search(/^H[6-7]/)==0)
			return 'Nicaragua';
		if(callsign.search(/^H[8-9]/)==0)
			return 'Panama';
		if(callsign.search(/^I/)==0)
			return 'Italy';
		if(callsign.search(/^J[A-S]/)==0)
			return 'Japan';
		if(callsign.search(/^J[T-V]/)==0)
			return 'Mongolia';
		if(callsign.search(/^J[W-X]/)==0)
			return 'Norway';
		if(callsign.search(/^JY/)==0)
			return 'Jordan';
		if(callsign.search(/^JZ/)==0)
			return 'Indonesia';
		if(callsign.search(/^J2/)==0)
			return 'Djibouti';
		if(callsign.search(/^J3/)==0)
			return 'Grenada';
		if(callsign.search(/^J4/)==0)
			return 'Greece';
		if(callsign.search(/^J5/)==0)
			return 'Guinea-Bissau';
		if(callsign.search(/^J6/)==0)
			return 'Saint Lucia';
		if(callsign.search(/^J7/)==0)
			return 'Dominica';
		if(callsign.search(/^J8/)==0)
			return 'Saint Vincent and the Grenadines';
		if(callsign.search(/^K/)==0)
			return 'United States of America';
		if(callsign.search(/^L[A-N]/)==0)
			return 'Norway';
		if(callsign.search(/^L[O-W]/)==0)
			return 'Republic';
		if(callsign.search(/^LX/)==0)
			return 'Luxembourg';
		if(callsign.search(/^LY/)==0)
			return 'Lithuania';
		if(callsign.search(/^LZ/)==0)
			return 'Bulgaria';
		if(callsign.search(/^L[2-9]/)==0)
			return 'Argentine Republic';
		if(callsign.search(/^M/)==0)
			return 'United Kingdom of Great Britain and Northern Ireland';
		if(callsign.search(/^N/)==0)
			return 'United States of America';
		if(callsign.search(/^O[A-C]/)==0)
			return 'Peru';
		if(callsign.search(/^OD/)==0)
			return 'Lebanon';
		if(callsign.search(/^OE/)==0)
			return 'Austria';
		if(callsign.search(/^O[F-J]/)==0)
			return 'Finland';
		if(callsign.search(/^O[K-L]/)==0)
			return 'Czech Republic';
		if(callsign.search(/^OM/)==0)
			return 'Slovak Republic';
		if(callsign.search(/^O[N-T]/)==0)
			return 'Belgium';
		if(callsign.search(/^O[U-Z]/)==0)
			return 'Denmark';
		if(callsign.search(/^P[A-I]/)==0)
			return 'Netherlands';
		if(callsign.search(/^PJ/)==0)
			return 'Caribbean Netherlands';
		if(callsign.search(/^P[K-O]/)==0)
			return 'Indonesia';
		if(callsign.search(/^P[P-Y]/)==0)
			return 'Brazil';
		if(callsign.search(/^PZ/)==0)
			return 'Suriname';
		if(callsign.search(/^P2/)==0)
			return 'Papua New Guinea';
		if(callsign.search(/^P3/)==0)
			return 'Cyprus';
		if(callsign.search(/^P4/)==0)
			return 'Aruba (Netherlands)';
		if(callsign.search(/^P[5-9]/)==0)
			return "Democratic People's Republic of Korea";
		if(callsign.search(/^R/)==0)
			return 'Russian Federation';
		if(callsign.search(/^S[A-M]/)==0)
			return 'Sweden';
		if(callsign.search(/^S[N-R]/)==0)
			return 'Poland';
		if(callsign.search(/^SS[A-M]/)==0)
			return 'Egypt';
		if(callsign.search(/^SS[N-Z]/)==0)
			return 'Sudan';
		if(callsign.search(/^ST/)==0)
			return 'Sudan';
		if(callsign.search(/^SU/)==0)
			return 'Egypt';
		if(callsign.search(/^SV/)==0)
			return 'Greece';
		if(callsign.search(/^S[2-3]/)==0)
			return 'Bangladesh';
		if(callsign.search(/^S5/)==0)
			return 'Slovenia';
		if(callsign.search(/^S6/)==0)
			return 'Singapore';
		if(callsign.search(/^S7/)==0)
			return 'Seychelles';
		if(callsign.search(/^S8/)==0)
			return 'South Africa';
		if(callsign.search(/^S9/)==0)
			return 'Sao Tome and Principe';
		if(callsign.search(/^T[A-C]/)==0)
			return 'Turkey';
		if(callsign.search(/^TD/)==0)
			return 'Guatemala';
		if(callsign.search(/^TE/)==0)
			return 'Costa Rica';
		if(callsign.search(/^TF/)==0)
			return 'Iceland';
		if(callsign.search(/^TG/)==0)
			return 'Guatemala';
		if(callsign.search(/^TH/)==0)
			return 'France';
		if(callsign.search(/^TI/)==0)
			return 'Costa Rica';
		if(callsign.search(/^TJ/)==0)
			return 'Cameroon';
		if(callsign.search(/^TK/)==0)
			return 'France';
		if(callsign.search(/^TL/)==0)
			return 'Central African Republic';
		if(callsign.search(/^TM/)==0)
			return 'France';
		if(callsign.search(/^TN/)==0)
			return 'Congo';
		if(callsign.search(/^T[O-Q]/)==0)
			return 'France';
		if(callsign.search(/^TR/)==0)
			return 'Gabonese Republic';
		if(callsign.search(/^TS/)==0)
			return 'Tunisia';
		if(callsign.search(/^TT/)==0)
			return 'Chad';
		if(callsign.search(/^TU/)==0)
			return "Cote d'Ivoire";
		if(callsign.search(/^T[V-X]/)==0)
			return 'France';
		if(callsign.search(/^TY/)==0)
			return 'Benin';
		if(callsign.search(/^TZ/)==0)
			return 'Mali';
		if(callsign.search(/^T2/)==0)
			return 'Tuvalu';
		if(callsign.search(/^T3/)==0)
			return 'Kiribati';
		if(callsign.search(/^T4/)==0)
			return 'Cuba';
		if(callsign.search(/^T5/)==0)
			return 'Somali Democratic Republic';
		if(callsign.search(/^T6/)==0)
			return 'Afghanistan';
		if(callsign.search(/^T7/)==0)
			return 'San Marino';
		if(callsign.search(/^T8/)==0)
			return 'Palau';
		if(callsign.search(/^U[A-I]/)==0)
			return 'Russian Federation';
		if(callsign.search(/^U[J-M]/)==0)
			return 'Uzbekistan';
		if(callsign.search(/^U[N-Q]/)==0)
			return 'Kazakhstan';
		if(callsign.search(/^U[R-Z]/)==0)
			return 'Ukraine';
		if(callsign.search(/^V[A-G]/)==0)
			return 'Canada';
		if(callsign.search(/^V[H-N]/)==0)
			return 'Australia';
		if(callsign.search(/^VO/)==0)
			return 'Canada';
		if(callsign.search(/^V[P-Q]/)==0)
			return 'United Kingdom of Great Britain and Northern Ireland';
		if(callsign.search(/^VR/)==0)
			return 'China';
		if(callsign.search(/^VS/)==0)
			return 'United Kingdom of Great Britain and Northern Ireland';
		if(callsign.search(/^V[T-W]/)==0)
			return 'India';
		if(callsign.search(/^V[X-Y]/)==0)
			return 'Canada';
		if(callsign.search(/^VZ/)==0)
			return 'Australia';
		if(callsign.search(/^V2/)==0)
			return 'Antigua and Barbuda';
		if(callsign.search(/^V3/)==0)
			return 'Belize';
		if(callsign.search(/^V4/)==0)
			return 'Saint Kitts and Nevis';
		if(callsign.search(/^V5/)==0)
			return 'Namibia';
		if(callsign.search(/^V6/)==0)
			return 'Micronesia';
		if(callsign.search(/^V7/)==0)
			return 'Marshall Islands';
		if(callsign.search(/^V8/)==0)
			return 'Brunei Darussalam';
		if(callsign.search(/^W/)==0)
			return 'United States of America';
		if(callsign.search(/^X[A-I]/)==0)
			return 'Mexico';
		if(callsign.search(/^X[J-O]/)==0)
			return 'Canada';
		if(callsign.search(/^XP/)==0)
			return 'Denmark';
		if(callsign.search(/^X[Q-R]/)==0)
			return 'Chile';
		if(callsign.search(/^XS/)==0)
			return 'China';
		if(callsign.search(/^XT/)==0)
			return 'Burkina Faso';
		if(callsign.search(/^XU/)==0)
			return 'Cambodia';
		if(callsign.search(/^XV/)==0)
			return 'Viet Nam';
		if(callsign.search(/^XW/)==0)
			return "Lao People's Democratic Republic";
		if(callsign.search(/^XX/)==0)
			return 'Macao (China)';
		if(callsign.search(/^X[Y-Z]/)==0)
			return 'Myanmar';
		if(callsign.search(/^YA/)==0)
			return 'Afghanistan';
		if(callsign.search(/^Y[B-H]/)==0)
			return 'Indonesia';
		if(callsign.search(/^YI/)==0)
			return 'Iraq';
		if(callsign.search(/^YJ/)==0)
			return 'Vanuatu';
		if(callsign.search(/^YK/)==0)
			return 'Syrian Arab Republic';
		if(callsign.search(/^YL/)==0)
			return 'Latvia';
		if(callsign.search(/^YM/)==0)
			return 'Turkey';
		if(callsign.search(/^YN/)==0)
			return 'Nicaragua';
		if(callsign.search(/^Y[O-R]/)==0)
			return 'Romania';
		if(callsign.search(/^YS/)==0)
			return 'El Salvador';
		if(callsign.search(/^Y[T-U]/)==0)
			return 'Serbia';
		if(callsign.search(/^Y[U-Y]/)==0)
			return 'Venezuela';
		if(callsign.search(/^Y[2-9]/)==0)
			return 'Germany';
		if(callsign.search(/^ZA/)==0)
			return 'Albania';
		if(callsign.search(/^Z[B-J]/)==0)
			return 'United Kingdom of Great Britain and Northern Ireland';
		if(callsign.search(/^Z[K-M]/)==0)
			return 'New Zealand';
		if(callsign.search(/^Z[N-O]/)==0)
			return 'United Kingdom of Great Britain and Northern Ireland';
		if(callsign.search(/^ZP/)==0)
			return 'Paraguay';
		if(callsign.search(/^ZQ/)==0)
			return 'United Kingdom of Great Britain and Northern Ireland';
		if(callsign.search(/^Z[R-U]/)==0)
			return 'South Africa';
		if(callsign.search(/^Z[V-Z]/)==0)
			return 'Brazil';
		if(callsign.search(/^Z2/)==0)
			return 'Zimbabwe';
		if(callsign.search(/^Z3/)==0)
			return 'The Former Yugoslav Republic of Macedonia';
		if(callsign.search(/^Z8/)==0)
			return 'South Sudan';
		if(callsign.search(/^2/)==0)
			return 'United Kingdom of Great Britain and Northern Ireland';
		if(callsign.search(/^3A/)==0)
			return 'Monaco';
		if(callsign.search(/^3B/)==0)
			return 'Mauritius';
		if(callsign.search(/^3C/)==0)
			return 'Equatorial Guinea';
		if(callsign.search(/^3D[A-M]/)==0)
			return 'Swaziland';
		if(callsign.search(/^3D[N-Z]/)==0)
			return 'Fiji';
		if(callsign.search(/^3[E-F]/)==0)
			return 'Panama';
		if(callsign.search(/^3G/)==0)
			return 'Chile';
		if(callsign.search(/^3[H-U]/)==0)
			return 'China';
		if(callsign.search(/^3V/)==0)
			return 'Tunisia';
		if(callsign.search(/^3W/)==0)
			return 'Viet Nam';
		if(callsign.search(/^3X/)==0)
			return 'Guinea';
		if(callsign.search(/^3Y/)==0)
			return 'Norway';
		if(callsign.search(/^3Z/)==0)
			return 'Poland';
		if(callsign.search(/^4[A-C]/)==0)
			return 'Mexico';
		if(callsign.search(/^4[D-I]/)==0)
			return 'Philippines';
		if(callsign.search(/^4[J-K]/)==0)
			return 'Azerbaijani Republic';
		if(callsign.search(/^4L/)==0)
			return 'Georgia';
		if(callsign.search(/^4M/)==0)
			return 'Venezuela';
		if(callsign.search(/^4O/)==0)
			return 'Montenegro';
		if(callsign.search(/^4[P-S]/)==0)
			return 'Sri Lanka';
		if(callsign.search(/^4T/)==0)
			return 'Peru';
		if(callsign.search(/^4U/)==0)
			return 'United Nations';
		if(callsign.search(/^4V/)==0)
			return 'Haiti';
		if(callsign.search(/^4W/)==0)
			return 'Democratic Republic of Timor-Leste';
		if(callsign.search(/^4X/)==0)
			return 'Israel';
		if(callsign.search(/^4Y/)==0)
			return 'International Civil Aviation Organization';
		if(callsign.search(/^4Z/)==0)
			return 'Israel';
		if(callsign.search(/^5A/)==0)
			return 'Libya';
		if(callsign.search(/^5B/)==0)
			return 'Cyprus';
		if(callsign.search(/^5[C-G]/)==0)
			return 'Morocco';
		if(callsign.search(/^5[H-I]/)==0)
			return 'Tanzania';
		if(callsign.search(/^5[J-K]/)==0)
			return 'Colombia';
		if(callsign.search(/^5[L-M]/)==0)
			return 'Liberia';
		if(callsign.search(/^5[N-O]/)==0)
			return 'Nigeria';
		if(callsign.search(/^5[P-Q]/)==0)
			return 'Denmark';
		if(callsign.search(/^5[R-S]/)==0)
			return 'Madagascar';
		if(callsign.search(/^5T/)==0)
			return 'Mauritania';
		if(callsign.search(/^5U/)==0)
			return 'Niger';
		if(callsign.search(/^5V/)==0)
			return 'Togolese Republic';
		if(callsign.search(/^5W/)==0)
			return 'Samoa';
		if(callsign.search(/^5X/)==0)
			return 'Uganda';
		if(callsign.search(/^5[Y-Z]/)==0)
			return 'Kenya';
		if(callsign.search(/^6[A-B]/)==0)
			return 'Egypt';
		if(callsign.search(/^6C/)==0)
			return 'Syrian Arab Republic';
		if(callsign.search(/^6[D-J]/)==0)
			return 'Mexico';
		if(callsign.search(/^6[K-N]/)==0)
			return 'Korea';
		if(callsign.search(/^6O/)==0)
			return 'Somali Democratic Republic';
		if(callsign.search(/^6[P-S]/)==0)
			return 'Pakistan';
		if(callsign.search(/^6[T-U]/)==0)
			return 'Sudan';
		if(callsign.search(/^6[V-W]/)==0)
			return 'Senegal';
		if(callsign.search(/^6X/)==0)
			return 'Madagascar';
		if(callsign.search(/^6Y/)==0)
			return 'Jamaica';
		if(callsign.search(/^6Z/)==0)
			return 'Liberia';
		if(callsign.search(/^7[A-I]/)==0)
			return 'Indonesia';
		if(callsign.search(/^7[J-N]/)==0)
			return 'Japan';
		if(callsign.search(/^7O/)==0)
			return 'Yemen';
		if(callsign.search(/^7P/)==0)
			return 'Lesotho';
		if(callsign.search(/^7Q/)==0)
			return 'Malawi';
		if(callsign.search(/^7R/)==0)
			return 'Algeria';
		if(callsign.search(/^7S/)==0)
			return 'Sweden';
		if(callsign.search(/^7[T-Y]/)==0)
			return 'Algeria';
		if(callsign.search(/^7Z/)==0)
			return 'Saudi Arabia';
		if(callsign.search(/^8[A-I]/)==0)
			return 'Indonesia';
		if(callsign.search(/^8[J-N]/)==0)
			return 'Japan';
		if(callsign.search(/^8O/)==0)
			return 'Botswana';
		if(callsign.search(/^8P/)==0)
			return 'Barbados';
		if(callsign.search(/^8Q/)==0)
			return 'Maldives';
		if(callsign.search(/^8R/)==0)
			return 'Guyana';
		if(callsign.search(/^8S/)==0)
			return 'Sweden';
		if(callsign.search(/^8[T-Y]/)==0)
			return 'India';
		if(callsign.search(/^8Z/)==0)
			return 'Saudi Arabia';
		if(callsign.search(/^9A/)==0)
			return 'Croatia';
		if(callsign.search(/^9[B-D]/)==0)
			return 'Iran';
		if(callsign.search(/^9[E-F]/)==0)
			return 'Ethiopia';
		if(callsign.search(/^9G/)==0)
			return 'Ghana';
		if(callsign.search(/^9H/)==0)
			return 'Malta';
		if(callsign.search(/^9[I-J]/)==0)
			return 'Zambia';
		if(callsign.search(/^9K/)==0)
			return 'Kuwait';
		if(callsign.search(/^9L/)==0)
			return 'Sierra Leone';
		if(callsign.search(/^9M/)==0)
			return 'Malaysia';
		if(callsign.search(/^9N/)==0)
			return 'Nepal';
		if(callsign.search(/^9[O-T]/)==0)
			return 'Democratic Republic of the Congo';
		if(callsign.search(/^9U/)==0)
			return 'Burundi';
		if(callsign.search(/^9V/)==0)
			return 'Singapore';
		if(callsign.search(/^9W/)==0)
			return 'Malaysia';
		if(callsign.search(/^9X/)==0)
			return 'Rwandese Republic';
		if(callsign.search(/^9[Y-Z]/)==0)
			return 'Trinidad and Tobago';
		throw("No country match for "+callsign);
	},

	CTYDAT:function(callsign, ctydat, ctydatfname) {
		var curcty={};
		var line;
		var incountry=false;
		var continents={
			AF:'Africa', 
			AN:'Antarctica',
			AS:'Asia',
			EU:'Europe',
			NA:'North Americal',
			OC:'Oceania',
			SA:'South America'
		};

		if(ctydatfname.length > 0) {
			var cf = new File(ctydatfname);
			if(ctydat.time == undefined || ctydat.time < cf.date) {
				for(var i in ctydat) {
					delete ctydat[i];
				}
				if(cf.open("rb")) {
					ctydat.time=cf.date;
					ctydat.prefix={};
					ctydat.exact={};
					ctydat.countries=[];
					while(line=cf.readln()) {
						line=line.replace(/[\r\n]/,'');
						line=line.trim();
						if(!incountry) {
							var x=line.split(/ *[:\r\n]+ */);
							if(x.length == 9) {
								var continent=continents[x[3]];
								if(continent==undefined) {
									log("Undefined continent "+x[3]);
									continent=x[3];
								}
								curcty={name:x[0], cq:x[1], itu:x[2], continent:continent, lat:x[4], lon:x[5], GMToff:x[6], prefix:x[7]};
								if(curcty.prefix.charAt(0)=='*') {
									curcty.prefix.replace(/^\*/, '');
									curcty.WAEDC=true;
								}
								else {
									curcty.WAEDC=false;
								}
								ctydat.countries.push(curcty);
								incountry=true;
							}
							else {
								log("Unable to parse country line "+line+'--'+x.toSource());
							}
						}
						else {
							if(line.search(/^[ \t]/)!=-1) {
								log("ERROR PARSING LINE "+line);
							}
							if(line.search(/;$/)!=-1) {
								incountry=false;
							}
							var x=line.split(/[,;]/);
							for(var i in x) {
								var call={country:curcty};
								var ecallsign=x[i].replace(/^\*/,'');
								var m=ecallsign.match(/\(([0-9]+)\)/);
								if(m) {
									call.cq=m[1];
									ecallsign=ecallsign.replace(/\([0-9]+\)/,'');
								}
								var m=ecallsign.match(/\[([0-9]+)\]/);
								if(m) {
									call.itu=m[1];
									ecallsign=ecallsign.replace(/\[[0-9]+\]/,'');
								}
								var m=ecallsign.match(/\<([^>\/]*)\/([^>\/]*)\>/);
								if(m) {
									call.lat=m[1];
									call.lon=m[2];
									ecallsign=ecallsign.replace(/\<[^>\/]*\/[^>\/]*\>/,'');
								}
								var m=ecallsign.match(/\{([^\}]*)\}/);
								if(m) {
									call.continent=continents[m[1]];
									if(call.continent==undefined) {
										log("Undefined continent "+m[1]);
										call.continent=m[1];
									}
									ecallsign=ecallsign.replace(/\{[^\}]+\}/,'');
								}
								var m=ecallsign.match(/\~([^~]*)\~/);
								if(m) {
									call.GMToff=m[1];
									ecallsign=ecallsign.replace(/\~[^~]*\~/,'');
								}
								if(ecallsign.charAt(0)=='=') {
									ecallsign=ecallsign.substr(1);
									ctydat.exact[ecallsign]=call;
								}
								else {
									ctydat.prefix[ecallsign]=call;
								}
							}
						}
					}
					cf.close();
				}
			}
		}
		var match;
		var matched;
		if(ctydat.exact[callsign] != undefined) {
			match = ctydat.exact[callsign];
			matched=callsign;
		}
		for(var i=0; i<callsign.length && match==undefined; i++) {
			if(ctydat.prefix[callsign.substr(0, i)]!=undefined) {
				match=ctydat.prefix[callsign.substr(0, i)];
				matched=callsign.substr(0, i);
			}
		}
		if(match==undefined)
			return match;
		var ret={
			matched:matched,
			name:match.country.name,
			cq:match.country.cq,
			itu:match.country.itu,
			continent:match.country.continent,
			lat:match.country.lat,
			lon:match.country.lon,
			GMToff:match.country.GNToff,
			prefix:match.country.prefix
		};
		for(i in ret) {
			if(match[i]!=undefined)
				ret[i]=match[i];
		}
		return ret;
	},
};
