/* $Id: url.js,v 1.6 2011/10/09 17:57:12 cyan Exp $ */

function URL(url, base)
{
	var loop,m,tmp;

	this.orig_url=url;
	if(base!=undefined) {
		this.base=new URL(base);
	}

	do {
		loop=false;

		this.url=url;
		m=url.match(/^(([^:\/?#]+):)?(\/\/([^\/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))?/);
		if(m!=null) {
			if(m[2]!=undefined) this.scheme=m[2];
			if(m[4]!=undefined) this.authority=m[4];
			if(m[5]!=undefined) this.path=m[5];
			if(m[7]!=undefined) this.query=m[7];
			if(m[9]!=undefined) this.fragment=m[9];
			if(this.authority != undefined) {
				m=this.authority.match(/^((.*?)@)?(.*?)(:([0-9]*))?$/);
				if(m!=null) {
					if(m[2]!=undefined) this.userinfo=m[2];
					if(m[3]!=undefined) this.host=m[3];
					if(m[5]!=undefined) this.port=m[5];
				}
			}
		}
		if(this.scheme == undefined && this.base != undefined && this.base.scheme != undefined) {
			if(this.url.substr(0,2)=='//') {
				// Relative w/authority (super-rare)
				url=this.base.scheme+"://"+this.authority;
				if(this.path != undefined)
					url+=this.path;
				if(this.query != undefined)
					url+="?"+this.query;
				if(this.fragment != undefined)
					url+="#"+this.fragment;
			}
			else if(this.url.substr(0,1)=='/') {
				// Relative w/absolute path
				url=this.base.scheme+":";
				if(this.base.authority != undefined) {
					url += "//"+this.base.authority;
				}
				if(this.path != undefined)
					url+=this.path;
				if(this.query != undefined)
					url+="?"+this.query;
				if(this.fragment != undefined)
					url+="#"+this.fragment;
			}
			else {
				// Relative to current path
				tmp=true;	// Still using base

				url=this.base.scheme+":";
				if(this.base.path != undefined) {
					url += this.base.path;
					if(this.base.path.substr(-1)!='/' && this.base.path.indexOf('/')!=-1)
						url=url.replace(/\/[^\/]*$/,'/');
				}
				if(this.path != undefined) {
					url+=this.path;
					tmp=false;
				}

				if(tmp && this.base.query != undefined && this.query == undefined)
					url += '?'+this.base.query;
				if(this.query != undefined) {
					url+="?"+this.query;
					tmp=true;
				}

				if(tmp && this.base.fragment != undefined && this.query == undefined)
					url += '#'+this.base.fragment;
				if(this.fragment != undefined) {
					url+="#"+this.fragment;
					tmp=true;
				}
			}
			loop=true;
		}
	} while(loop);

	this.__defineGetter__("request_path", function() {
		var ret='';

		if(this.path != undefined)
			ret += this.path;
		if(this.query != undefined)
			ret += '?'+this.query;
		if(this.fragment != undefined)
			ret += '#'+this.fragment;
		return ret;
	});
}
