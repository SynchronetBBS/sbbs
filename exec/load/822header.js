require("utf8_ascii.js", 'utf8_ascii');
require("smbdefs.js", 'RFC822HEADER');

MsgBase.HeaderPrototype.get_rfc822_header=function(force_update, unfold)
{
	var content_type;
	var i;

	if(force_update===true)
		delete this.rfc822;

	if(this.rfc822==undefined) {
		this.rfc822='';
		this.rfc822 += "To: "+ (this.to_list || this.to || this.forward_path) +"\r\n";
		if(this.cc_list)
			this.rfc822 += "Cc: " + this.cc_list + "\r\n";
		this.rfc822 += "Subject: "+this.subject+"\r\n";
		this.rfc822 += "Message-ID: "+this.id+"\r\n";
		this.rfc822 += "Date: "+this.date+"\r\n";

		var quoted_from = '"' + this.from + '"';
		if(!this.from_net_type || this.from_net_addr.length==0)    /* local message */
			this.rfc822 += "From: " + quoted_from + " <" + this.from.replace(/ /g,".").toLowerCase() + "@" + system.inetaddr + ">\r\n";
		else if(!this.from_net_addr.length)
			this.rfc822 += "From: " + quoted_from + "\r\n";
		else if(this.from_net_addr.indexOf('@')!=-1)
			this.rfc822 += "From: " + quoted_from +" <"+this.from_net_addr+">\r\n";
		else
			this.rfc822 += "From: " + quoted_from +" <"+this.from.replace(/ /g,".").toLowerCase()+"@"+this.from_net_addr+">\r\n";

//		this.rfc822 += "X-Comment-To: "+this.to+"\r\n";
		if(this.path != undefined)
			this.rfc822 += "Path: "+system.inetaddr+"!"+this.path+"\r\n";
		if(this.from_org != undefined)
			this.rfc822 += "Organization: "+this.from_org+"\r\n";
		if(this.newsgroups != undefined)
			this.rfc822 += "Newsgroups: "+this.newsgroups+"\r\n";
		
		if(this.replyto_list != undefined)
			this.rfc822 += "Reply-To: "+this.replyto_list+"\r\n";
		else if(this.replyto != undefined)
			this.rfc822 += "Reply-To: "+this.replyto+"\r\n";
		else {
			if(this.subnum !== undefined && this.subnum != -1) {
				this.rfc822 += 'Reply-To: "'+this.from+'"';
                if (this.cfg!=undefined) {
                    this.rfc822 += ' <sub:'+this.cfg.code+'@'+system.inet_addr+'>';
                }
                this.rfc822 += '\r\n';
            }
		}
		if(this.reply_id != undefined)
			this.rfc822 += "In-Reply-To: "+this.reply_id+"\r\n";
		if(this.references != undefined)
			this.rfc822 += "References: "+this.references+"\r\n";
		else if(this.reply_id != undefined)
			this.rfc822 += "References: "+this.reply_id+"\r\n";
		if(this.reverse_path != undefined)
			this.rfc822 += "Return-Path: "+this.reverse_path+"\r\n";
		
		// "Received" headers
		if(this.field_list!=undefined) {
			for(i in this.field_list) 
				if(this.field_list[i].type == SMTPRECEIVED)
					this.rfc822 += "Received: " + this.field_list[i].data + "\r\n";
		}
		
		if(this.priority)
			this.rfc822 += "X-Priority: " + this.priority + "\r\n";

		// Fidonet headers
		if(this.ftn_area != undefined)
			this.rfc822 += "X-FTN-AREA: "+this.ftn_area+"\r\n";
		if(this.ftn_pid != undefined)
			this.rfc822 += "X-FTN-PID: "+this.ftn_pid+"\r\n";
		if(this.ftn_TID != undefined)
			this.rfc822 += "X-FTN-TID: "+this.ftn_tid+"\r\n";
		if(this.ftn_flags != undefined)
			this.rfc822 += "X-FTN-FLAGS: "+this.ftn_flags+"\r\n";
		if(this.ftn_msgid != undefined)
			this.rfc822 += "X-FTN-MSGID: "+this.ftn_msgid+"\r\n";
		if(this.ftn_reply != undefined)
			this.rfc822 += "X-FTN-REPLY: "+this.ftn_reply+"\r\n";
		
		// Other RFC822 headers
		if(this.field_list!=undefined) {
			for(i in this.field_list) 
				if(this.field_list[i].type==FIDOSEENBY)
					this.rfc822 += "X-FTN-SEEN-BY: " + this.field_list[i].data + "\r\n";
			for(i in this.field_list) 
				if(this.field_list[i].type==FIDOPATH)
					this.rfc822 += "X-FTN-PATH: " + this.field_list[i].data + "\r\n";
		}
	
		// Other RFC822 headers
		if(this.field_list!=undefined) {
			for(i in this.field_list) 
				if(this.field_list[i].type==RFC822HEADER) {
					if(this.field_list[i].data.toLowerCase().indexOf("content-type:")==0)
						content_type = this.field_list[i].data;
					this.rfc822 += this.field_list[i].data+"\r\n";
				}
		}
		if(content_type==undefined) {
			/* No content-type specified, so assume IBM code-page 437 (full ex-ASCII) */
			this.rfc822 += "Content-Type: text/plain; charset=IBM437\r\n";
			this.rfc822 += "Content-Transfer-Encoding: 8bit\r\n";
		}

		if(unfold !== false)
			this.rfc822=this.rfc822.replace(/\s*\r\n\s+/g, " ");
		this.rfc822 += "\r\n";
		// Illegal characters in header?
		if (this.rfc822.search(/[^\x01-\x7f]/) != -1) {
			/* Is it UTF-8? */
			if (this.rfc822.replace(/[\xc0-\xfd][\x80-\xbf]+/g,'').search(/[^\x01-\x7f]/) == -1) {
				this.rfc822 = utf8_ascii(this.rfc822);
			}
			else {
				// If not, just strip them all...
				this.rfc822 = this.rfc822.replace(/[^\x01-\x7f]/g, '?');
			}
		}
	}
	return this.rfc822;
};
