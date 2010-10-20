if(js.global.HTTPRequest==undefined)
	load(js.global,"http.js");

function define(word,dict) {
	var dict_url = "http://services.aonaware.com/DictService/DictService.asmx/DefineInDict?dictId=" + dict + "&word=" + word;
	var body = new HTTPRequest().Get(dict_url).replace(/<\?.*\?>[\r\n]*/,'').replace(/\sxmlns=".*"/,'');
	var xml_obj = new XML(body);
	var definition = xml_obj.Definitions.Definition;
	if(!definition[0]) {
		return false;
	} else {
		return(definition[0]);
	}
}

function parseDefXML(xmldef) {
	return(strip_ctrl(xmldef.WordDefinition).replace(/\s+/g,' ').replace(/[{}]/g,''));
}

function sortIndices(list) {
	for(n = 0;n < list.length;n++)
	{
		for(m = 0; m < list.length-1; m++) 
		{
			if(list[m].pos < list[m+1].pos) 
			{
				holder = list[m+1];
				list[m+1] = list[m];
				list[m] = holder;
			}
		}
	}
	return list;
}

function getIndices(defstring) {
	var indices=[];
	var adj=defstring.indexOf(" adj ");
	var noun=defstring.indexOf(" n ");
	var verb=defstring.indexOf(" v ");
	
	if(adj >= 0) indices.push(new Index(adj,"adj"));
	if(noun >= 0) indices.push(new Index(noun,"noun"))
	if(verb >= 0) indices.push(new Index(verb,"verb"));
	
	indices=sortIndices(indices);
	return indices;
}

function getIndex(defstring,type) {
	switch(type) {
	case "verb":
		var index=defstring.search(/\sv\s/);
		if(index >= 0) return index+3;
		break;
	case "noun":
		var index=defstring.search(/\sn\s/);
		if(index >= 0) return index+3;
		break;
	case "adj":
		var index=defstring.search(/\sadj\s/);
		if(index >= 0) return index+5;
		break;
	}
	return -1;
}

function getType(cmd) {
	if(!cmd) return false;
	if("verb".search(cmd.toLowerCase()) >= 0) {
		return "verb";
	}
	if("noun".search(cmd.toLowerCase()) >= 0) {
		return "noun";
	}
	if("adjective".search(cmd.toLowerCase()) >= 0) {
		return "adj";
	}
	return false;
}

function parseDefs(text) {
	var defs=[];
	var defIndex=text.indexOf(":")-1;

	if(text[defIndex] == "1") {
		var count=1;
		while(defIndex >= 0) {
			var nextIndex=text.indexOf((++count)+":");
			var defstr="";
			if(nextIndex >= 0) {
				defstr=text.substring(defIndex+2,nextIndex);
			} else {
				defstr=text.substr(defIndex+2);
			}
			defs.push(parseDef(defstr));
			defIndex=nextIndex;
		}
	} else {
		var defstr=text.substr(defIndex+2);
		defs.push(parseDef(defstr));
	}
	return defs;
}

function getDefStr(text,index) {
	var defstr=text.substr(index);
	var next=defstr.search(/\s(v|n|adj)\s/);
	if(next >= 0) {
		defstr=defstr.substring(0,next);
	}
	return defstr;
}

function parseDef(text) {
	var cutIndex=text.search(/(\[syn|")/);
	var deftext=text;
	if(cutIndex >= 0) deftext=text.substring(0,cutIndex);
	return new Definition(deftext);
}

function getDefOutput(type,defstring) {
	var index=getIndex(defstring,type);
	if(index < 0) {
		return false ;
	}
	
	var text=getDefStr(defstring,index);
	var defs=parseDefs(text);
	
	var str="";
	for(var d in defs) {
		var addstr="; " + truncsp(defs[d].def).replace(/;$/,"");
		if(str.length+addstr.length > 500) break;
		str+=addstr;
	}
	return str.substr(2);
}
/*
<WordDefinition xmlns="http://services.aonaware.com/webservices/">
  <Word>string</Word>
  <Definitions>
    <Definition>
      <Word>string</Word>
      <Dictionary>
        <Id>string</Id>
        <Name>string</Name>
      </Dictionary>
      <WordDefinition>string</WordDefinition>
    </Definition>
    <Definition>
      <Word>string</Word>
      <Dictionary>
        <Id>string</Id>
        <Name>string</Name>
      </Dictionary>
      <WordDefinition>string</WordDefinition>
    </Definition>
  </Definitions>
</WordDefinition>
*/