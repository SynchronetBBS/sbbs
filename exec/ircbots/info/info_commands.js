Bot_Commands["DEF"] = new Bot_Command(0,1,false);
Bot_Commands["DEF"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	if(!cmd[0]) {
		srv.o(target,"You didn't specify a word");
		return;
	}
	var xmldef=define(cmd.shift(),"wn");
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
js.global.Bot_Commands["DEF"]=Bot_Commands["DEF"];

Bot_Commands["SYN"] = new Bot_Command(0,1,false);
Bot_Commands["SYN"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	if(!cmd[0]) {
		srv.o(target,"You didn't specify a word");
		return;
	}
	var xmldef=define(cmd.shift(),"wn");
	var word=xmldef.Word;
	var defstring=parseDefXML(xmldef);
	
	var synonyms=defstring.match(/\[[^\[\]]*:\s[^\[\]]*\]/g);
	if(!synonyms || !synonyms.length) {
		srv.o(target, "no synonyms found: " + word);
		return;
	} 
	for(var s=0;s<synonyms.length;s++) {
		synonyms[s]=synonyms[s].replace(/\[[^\[\]]*:/g,"").replace(/\]/g,"");
	}
	srv.o(target,word + " (synonyms): " + synonyms);
	return;
}
js.global.Bot_Commands["SYN"]=Bot_Commands["SYN"];

Bot_Commands["EX"] = new Bot_Command(0,1,false);
Bot_Commands["EX"].command = function (target,onick,ouh,srv,lvl,cmd) {
	cmd.shift();
	if(!cmd[0]) {
		srv.o(target,"You didn't specify a word");
		return;
	}
	var xmldef=define(cmd.shift(),"wn");
	var word=xmldef.Word;
	var defstring=parseDefXML(xmldef);
	
	var examples=defstring.match(/"[^"]*"/g);
	if(!examples || !examples.length) {
		srv.o(target, "no usage examples found: " + word);
		return;
	} 
	srv.o(target,word + " (usage): " + examples);
	return;
}
js.global.Bot_Commands["EX"]=Bot_Commands["EX"];

/* 
dog
 n 
	1: a member of the genus Canis (probably descended from the common wolf) that has been domesticated by man since prehistoric times; occurs in many breeds; 
		"the dog barked all night" 
		[syn: {domestic dog}, {Canis familiaris}] 
	2: a dull unattractive unpleasant girl or woman; 
		"she got a reputation as a frump"; 
		"she's a real dog" 
		[syn: {frump}] 
	3: informal term for a man; 
		"you lucky dog" 
	4: someone who is morally reprehensible; 
		"you dirty dog" 
		[syn: {cad}, {bounder}, {blackguard}, {hound}, {heel}] 
	5: a smooth-textured sausage of minced beef or pork usually smoked; often served on a bread roll 
		[syn: {frank}, {frankfurter}, {hotdog}, {hot dog}, {wiener}, {wienerwurst}, {weenie}] 
	6: a hinged catch that fits into a notch of a ratchet to move a wheel forward or prevent it from moving backward 
		[syn: {pawl}, {detent}, {click}] 
	7: metal supports for logs in a fireplace; 
		"the andirons were too hot to touch" 
		[syn: {andiron}, {firedog}, {dog-iron}]
 v 
	: go after with the intent to catch; "The policeman chased the mugger down the alley"; "the dog chased the rabbit" 
		[syn: {chase}, {chase after}, {trail}, {tail}, {tag}, {give chase}, {go after}, {track}] [also: {dogging}, {dogged}]
*/