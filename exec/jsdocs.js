// jsdocs.js

// This script will (eventually be used to) generated HTML documentation of the
// Synchronet JavaScript object model

function document_methods(name,obj)
{
	if(obj._method_list == undefined)
		return;

	for(method in obj._method_list)
		printf("%-8s %s.%s()\r\n","",name,obj._method_list[method].name);
}

function document_object(name,obj)
{
	var prop_name;

	for(prop in obj) {
		prop_name=name + "." + prop;
		printf("%-8s %s\r\n",typeof(obj[prop]),prop_name);
		if(typeof(obj[prop])=="object") {
			if(obj[prop].length != undefined)	// array ?
				document_object(prop_name + "[]",obj[prop][0]);
			else
				document_object(prop_name,obj[prop]);
		}
	}

	document_methods(name,obj);
}

document_object("client"	,client);
document_object("system"	,system);
document_object("bbs"		,bbs);
document_object("user"		,user);
document_object("server"	,server);
document_object("console"	,console);
document_object("msg_area"	,msg_area);
document_object("file_area"	,file_area);
document_object("File"		,new File("bogusfile"));