function URL(url)
{
	var m=url.match(/^(([^:\/?#]+):)?(\/\/([^\/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))?/);

	if(m!=null) {
		this.scheme=m[2];
		this.authority=m[4];
		this.path=m[5];
		this.query=m[7];
		this.fragment=m[9];
		if(this.authority != undefined) {
			m=this.authority.match(/^((.*?)@)?(.*?)(:([0-9]*))?$/);
			if(m!=null) {
				this.userinfo=m[2];
				this.host=m[3];
				this.port=m[5];
			}
		}
	}
}
