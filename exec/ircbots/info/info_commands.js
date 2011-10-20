Bot_Commands["DEF"] = new Bot_Command(0,1,false);
Bot_Commands["DEF"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	if(!cmd[0]) {
		srv.o(target,"You didn't specify a word");
		return;
	}
	var xmldef=define(cmd[0],"wn");
	if(!xmldef) {
		srv.o(target, "word not found: " + cmd[0]);
		return;
	} 
	var word=xmldef.Word;
	var defstring=parseDefXML(xmldef);
	
	var type=getType(cmd.shift());
	if(!type) {
		var n=getDefOutput("noun",defstring);
		var v=getDefOutput("verb",defstring);
		var a=getDefOutput("adj",defstring);
		
		if(!(n || v || a)) {
			srv.o(target, "word not found: " + word);
			return;
		} 
		
		if(n) srv.o(target,word + " (noun): " + n);
		if(v) srv.o(target,word + " (verb): " + v);
		if(a) srv.o(target,word + " (adj): " + a);
		return;
	}
	var def=getDefOutput(type,defstring);
	if(!def) {
		srv.o(target, "word not found: " + word);
		return;
	} 
	srv.o(target,word + " (" + type + "): " + def);
	return;
}

Bot_Commands["SYN"] = new Bot_Command(0,1,false);
Bot_Commands["SYN"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	if(!cmd[0]) {
		srv.o(target,"You didn't specify a word");
		return;
	}
	var xmldef=define(cmd[0],"wn");
	if(!xmldef) {
		srv.o(target, "no synonyms found: " + cmd[0]);
		return;
	} 

	var word=xmldef.Word;
	var defstring=parseDefXML(xmldef);
	
	var synonyms=defstring.match(/\[[^\[\]]*:\s[^\[\]]*\]/g);
	if(!synonyms.length > 0) {
		srv.o(target, "no synonyms found: " + word);
		return;
	} 
	for(var s=0;s<synonyms.length;s++) {
		synonyms[s]=synonyms[s].replace(/\[[^\[\]]*:/g,"").replace(/\]/g,"");
	}
	//var examples=text.match(/\".*"/g);
	srv.o(target,word + " (synonyms): " + synonyms);
	return;
}

Bot_Commands["INFO"] = new Bot_Command(0,1,false);
Bot_Commands["INFO"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	if(!cmd[0]) {
		srv.o(target,"You didn't specify a word");
		return;
	}
	var xmldef=define(cmd[0],"wn");
	if(!xmldef) {
		srv.o(target, "no usage examples found: " + cmd[0]);
		return;
	} 
	var word=xmldef.Word;
	var defstring=parseDefXML(xmldef);
	
	var examples=defstring.match(/"[^"]*"/g);
	if(!examples.length > 0) {
		srv.o(target, "no usage examples found: " + word);
		return;
	} 
	srv.o(target,word + " (usage): " + examples);
	return;
}

Bot_Commands["WTF"] = new Bot_Command(0,1,false);
Bot_Commands["WTF"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	cmd.shift();
	if(!cmd[0]) {
		srv.o(target,"usage: " + get_cmd_prefix() + "wtf is <acronym>");
		return;
	}
	
	var result=wtf_is(cmd[0]);
	if(!result) {
		srv.o(target, "acronym not found: " + cmd[0]);
		return;
	} 
	
	var str="";
	for(var a in result) {
		str+=", " + result[a].expan;
	}
	srv.o(target,cmd[0].toUpperCase() + ":" + str.substr(1));
	return;
}
