log = function() {

}

load("json-client.js");
load("sbbsdefs.js");				// K_*
load("uifcdefs.js");				// WIN_*

if(uifc == undefined) {
	writeln("UIFC interface not available!");
	exit(1);
}

var db;

/* verify db connection */
function verifyConnection() {
	if(!db.socket.is_connected) {
		uifc.msg("Socket disconnected");
		return false;
	}
	return true;
}

/* wait for confirmation or error from db */
function wait(func) {
	var start = Date.now();
	do {
		var packet = db.receive();
		if(!packet)
			continue;
		else if(packet.func == func) 
			return packet.data;
		else 
			db.updates.push(packet.data);
	} while(Date.now() - start < db.settings.SOCK_TIMEOUT);
	throw("timed out waiting for server response");
}

/* process a menu choice */
function processSelection(object) {
	var str;
	try {
		switch(object.type) {
		case "string":
			str=uifc.input(WIN_MID, object.name, object.val, 0, K_EDIT);
			if(str!=undefined)
				object.val=str;
			break;
		case "date":
			str=uifc.input(WIN_MID, object.name, object.val, 8, K_EDIT);
			if(str!=undefined)
				object.val=str;
			break;
		case "number":
			str=uifc.input(WIN_MID, object.name, object.val.toString(), 0, K_NUMBER|K_EDIT);
			if(str!=undefined)
				object.val=Number(str);
			break;
		case "boolean":
			object.val=!object.val;
			break;
		case "password":
			str=uifc.input(WIN_MID, object.name, object.val, 0, K_EDIT);
			if(str!=undefined)
				object.val=str;
			break;
		case "function":
			object.func();
			break;
		}
	} catch(e) {
		uifc.msg(e);
	}
}

/* initialize uifc list */
function initList(object) {
	var list=new Array();
	for(i=0; i<object.length; i++) {
		switch(object[i].type) {
			case "string":
			case "date":
			case "number":
			case "boolean":
				list.push(format("%-35s %s",object[i].name,object[i].val));
				break;
			case "password":
				list.push(format("%-35s %s",object[i].name,object[i].val?"********":""));
				break;
			case "function":
				list.push(format("%-35s %s",object[i].name,""));
				break;
		}
	}
	return list;
}

/* server configuration menu */
function configureServer() {
	var last=0;
	var serverProperties=[
		{	
			name:"Server hostname"
			,type:"string"
			,val:"localhost"
		}
		,{	
			name:"Server port"
			,type:"number"
			,val:"10088"
		}
		,{	
			name:"User Name"
			,type:"string"
			,val:"your name"
		}
		,{
			name:"User Password"
			,type:"password"
			,val:"your password"
		}
		,{
			name:"Connect to Server"
			,type:"function"
			,func:connect
		}
	];
	
	/* connect to server */
	function connect() {
		if(!db)
			db = new JSONClient(serverProperties[0].val,serverProperties[1].val);
		if(!db.socket.is_connected)
			db.connect();
		db.ident("ADMIN",serverProperties[2].val,serverProperties[3].val);
		var trash = wait("OK");
		serviceAdmin();
	}

	for(;;) {
		var list=initList(serverProperties);
		var i=uifc.list(WIN_MID|WIN_ORG|WIN_ACT|WIN_ESC, 0, 0, 0, last, last, "Server Configuration", list);
		
		/* close connection and exit */
		if(i==-1) {
			if(db && db.socket.is_connected) 
				db.disconnect();
			exit();
		}
		
		/* process selection */
		else if(serverProperties[i]) {
			last=i;
			processSelection(serverProperties[i]);
		}
	}
}

/* control service and modules */
function serviceAdmin() {
	var i;
	var last=0;
	var serviceMenu=[
		{	
			 prop:"Restart"
			,name:"Restart Service"
			,type:"function"
			,func:function() {
				db.send({scope:"ADMIN",func:"RESTART"});
				uifc.msg(wait("OK"));
			}
		}
		,{	
			 prop:"Close"
			,name:"Close Service"
			,type:"function"
			,func:function() {
				db.send({scope:"ADMIN",func:"CLOSE"});
				uifc.msg(wait("OK"));
			}
		}
		,{	
			 prop:"Open"
			,name:"Open Service"
			,type:"function"
			,func:function() {
				db.send({scope:"ADMIN",func:"OPEN"});
				uifc.msg(wait("OK"));
			}
		}
		,{	
			 prop:"Modules"
			,name:"Modules"
			,type:"function"
			,func:moduleList
		}
	];
	var list=initList(serviceMenu);

	for(;;) {
		if(!verifyConnection())
			return;
		i=uifc.list(WIN_MID|WIN_ORG, 0, 0, 0, last, last, "Server Administration", list);
		/* process selection */
		if(i==-1) {
			db.disconnect();
			return;
		}
		else if(serviceMenu[i]) {
			last=i;
			processSelection(serviceMenu[i]);
		}
	}
}

/* list service modules */
function moduleList() {
	
	var last=0;
	var list;
	var moduleMenu;
	
	function init() {
		db.send({scope:"ADMIN",func:"MODULES"});
		var modules = db.wait();
		
		list = [];
		moduleMenu=[];
		for each(var m in modules) {
			moduleMenu.push(m);
			m.type = "function";
			m.func = function() {
				moduleAdmin(this);
			}
			list.push(format("%-35s %s",m.name,m.status?"online":"offline"));
		}
	}
	
	for(;;) {
		if(!verifyConnection())
			return;

		init();
		i=uifc.list(WIN_MID|WIN_ACT|WIN_ESC, 0, 0, 0, last, last, "Service Modules", list);
		
		/* process selection */
		if(i==-1)
			return;
		else if(moduleMenu[i]) {
			last=i;
			processSelection(moduleMenu[i]);
		}

	}
}

/* configure a module */
function moduleAdmin(module) {
	var i;
	var last=0;
	var list;
	
	var moduleOptions=[
		{	
			 prop:"Set Offline"
			,name:"Set Offline"
			,type:"function"
			,func:function() {
				db.send({scope:module.name,func:"CLOSE"});
				uifc.msg(wait("OK"));
			}
		}
		,{	
			 prop:"Set Online"
			,name:"Set Online"
			,type:"function"
			,func:function() {
				db.send({scope:module.name,func:"OPEN"});
				uifc.msg(wait("OK"));
			}
		}
		,{	
			 prop:"Reload"
			,name:"Reload"
			,type:"function"
			,func:function() {
				db.send({scope:module.name,func:"RELOAD"});
				uifc.msg(wait("OK"));
			}
		}
		,{	
			 prop:"Browse"
			,name:"Browse"
			,type:"function"
			,func:function() {
				browseData(module);
			}
		}
		,{	
			 prop:"Users"
			,name:"Users"
			,type:"function"
			,func:function() {
				whoData(module);
			}
		}
		,{	
			 prop:"Save Data"
			,name:"Save Data"
			,type:"function"
			,func:function() {
				db.send({scope:module.name,func:"SAVE"});
				uifc.msg(wait("OK"));
			}
		}
	];
	
	for(;;) {
		if(!verifyConnection())
			return;

		list=initList(moduleOptions);
		i=uifc.list(WIN_MID|WIN_ORG, 0, 0, 0, last, last, "Module Administration", list);
		/* process selection */
		if(i==-1) 
			return;
		else if(moduleOptions[i]) {
			last=i;
			processSelection(moduleOptions[i]);
		}
	}
	
}

/* browse a module's database (recursive) */
function browseData(module,location,dataType) {
		
	var last=0;
	var list;
	var keys;
	var dataMenu;
	var path;
	
	if(dataType == undefined) {
		dataType = "object";
	}
	if(location == undefined) {
		path = "";
	}
	else {
		path = location + ".";
	}
	
	function init() {
		/* load in object list */
		if(location == undefined) {
			keys = db.keyTypes(module.name,"",1);
		}
		else {
			keys = db.keyTypes(module.name,location,1);
		}

		list = [];
		dataMenu = [];
		for(var k=0;k<keys.length;k++) {
			var data = db.read(module.name,path+keys[k],1);
			switch(typeof data) {
			case "object":
				var item = {
					name:keys[k],
					type:"function",
					val:data
				};
				if(data instanceof Array) {
					list.push(format("%-35s %s",keys[k],"[...]"));
					item.func=function() {
						browseData(module,path+this.name,"array");
					}
				}
				else {
					list.push(format("%-35s %s",keys[k],"{...}"));
					item.func=function() {
						browseData(module,path+this.name,"object");
					}
				}
				dataMenu.push(item);
				break;
			case "string":
			case "number":
			case "boolean":
			case "undefined":
				var item = {
					name:keys[k],
					type:typeof data,
					val:data
				};
				list.push(format("%-35s %s",keys[k],data));
				dataMenu.push(item);
				break;
			default:
				break;
			}
		}
	}
	
	function process(object) {
		
		/* if we are an array or object, browse */
		switch(object.type) {
		case "function":
			object.func();
			return;
		}
		/* otherwise, edit value */
		var new_val = object.val;
		switch(object.type) {
		case "string":
			str=uifc.input(WIN_MID, object.name, object.val, 0, K_EDIT);
			if(str != undefined) 
				new_val=str;
			break;
		case "number":
			str=uifc.input(WIN_MID, object.name, object.val.toString(), 0, K_NUMBER|K_EDIT);
			if(str != undefined) 
				new_val=parseInt(str);
			break;
		case "boolean":
			new_val=!object.val;
			break;
		default:
			// fall through?
			return;
		}
		
		/* if the value has changed, write it to the db */
		if(new_val !== object.val) {
			db.lock(module.name,path+object.name,2);
			var cur_val = db.read(module.name,path+object.name);
			if(cur_val !== object.val) {
				object.val = cur_val;
				uifc.msg("Value has been modified remotely, aborting change");
			}
			else {
				object.val = new_val;
				db.write(module.name,path+object.name,object.val);
			}
			db.unlock(module.name,path+object.name);
		}
	}
	
	for(;;) {
		if(!verifyConnection())
			return;

		init();
		i=uifc.list(WIN_MID|WIN_ORG|WIN_XTR|WIN_INS|WIN_DEL, 0, 0, 0, last, last, "Browse Data", list);
		
		/* process selection */
		if(i == -1) {
			return;
		}
		if(i-MSK_DEL>=0 && dataMenu[i-MSK_DEL]) {
			var index = i-MSK_DEL;
			delObject(module,path + dataMenu[index].name);
		}
		else if(i-MSK_INS>=0 && (dataMenu[i-MSK_INS] || i-MSK_INS == dataMenu.length)) {
			var index = i-MSK_INS;
			insObject(module,location,index,dataType);
		}
		else if(dataMenu[i]) {
			last=i;
			process(dataMenu[i]);
		}
	}
}

/* list module subscribers */
function whoData(module) {
	
	var userList, userMenu, list;
	
	function init() {
		userList = db.who(module.name,"");
		list=[];
		
		for each(var u in userList) {
			u.type = "string";
			if(u.nick == "")
				list.push("SERVICE");
			else
				list.push(u.nick);
		}
	}
	
	for(;;) {
		if(!verifyConnection())
			return;

		init();
		i=uifc.list(WIN_MID|WIN_ACT|WIN_ESC, 0, 0, 0, 0, 0, "Module Users", list);
		/* process selection */
		if(i == -1) {
			return;
		}
	}
}

/* choose a data type */
function getDataType() {
	
	var last=0;
	var list;
	var dataTypes=[
		{	
			name:"String"
			,type:"function"
			,val:"string"
		},
		{	
			name:"Number"
			,type:"function"
			,val:"number"
		},
		{	
			name:"Array"
			,type:"function"
			,val:"array"
		},
		{	
			name:"Object"
			,type:"function"
			,val:"object"
		},
		{	
			name:"Boolean"
			,type:"function"
			,val:"boolean"
		}
	];
	
	for(;;) {
		if(!verifyConnection())
			return;

		list=initList(dataTypes);
		i=uifc.list(WIN_MID|WIN_ACT|WIN_ESC, 0, 0, 0, last, last, "Data Types", list);
		
		/* process selection */
		if(i==-1)
			return;
		else if(dataTypes[i]) {
			last=i;
			return dataTypes[i].val;
		}

	}
}

/* insert a new object */
function insObject(module,location,index,dataType) {
	
	var key=undefined;
	var val=undefined;
	
	if(dataType=="object") {
		key=uifc.input(WIN_MID, "New object property name", "", 0, K_EDIT);
		if(key == undefined)
			return;
	}

	/* choose a data type for new element */
	var type=getDataType();
	
	/* prompt for new value based on data type */
	switch(type) {
	case "string":
		val=uifc.input(WIN_MID, "Enter String", "", 0, K_EDIT);
		break;
	case "number":
		val=uifc.input(WIN_MID, "Enter Number", "", 0, K_NUMBER|K_EDIT);
		if(val!=undefined)
			val=Number(val);
		break;
	case "boolean":
		var choice = uifc.list(WIN_MID, 0, 0, 0, 0, 0, "Choose", ["True","False"]);
		if(choice == 0)
			val = true;
		else if(choice == 1)
			val = false;
		break;
	case "array":
		val = [];
		break;
	case "object":
		val = {};
		break;
	}
	
	/* if we did not choose a value, abort insert */
	if(val == undefined) {
		return;
	}
	
	/* if key is not specified, we are in an array */
	if(key == undefined) {
		db.splice(module.name,location,index,0,val,2);
	}
	
	/*otherwise, we're setting a new object property */
	else {
		db.write(module.name,location+"."+key,val,2);
	}
}

/* delete an object */
function delObject(module,location) {
	var choice=uifc.list(WIN_MID, 0, 0, 0, 0, 0, "Are you sure?", ["Yes","No"]);
	if(choice == 1)
		return;
	db.remove(module.name,location,2);
}

/* configure server address/port and user name/password */
uifc.init("JSON Service Control");
configureServer();
uifc.pop();
uifc.bail();
