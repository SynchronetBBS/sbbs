// jsdocs.js

// This script will (eventually be used to) generate HTML documentation of the
// Synchronet JavaScript object model

const table_tag = "<table border=1 width=100%>";

table_depth=0;

function table_open(name)
{
	f.writeln(table_tag);
	table_depth++;
}

function table_close()
{
	if(table_depth) {
		f.writeln("</table>");
		table_depth--;
	}
}

function document_methods(name,obj)
{
	if(obj._method_list == undefined)
		return;

	table_close();
	f.writeln("<br>");
	table_open(name);

	f.writeln("<caption align=left><b>" + name.italics() + " methods" + "</b></caption>");
	f.writeln("<tr bgcolor=gray>");
	f.writeln("<th align=left width=100>");
	f.writeln("Name".fontcolor("white"));
	f.writeln("<th align=left width=100>");
	f.writeln("Returns".fontcolor("white"));
	f.writeln("<th align=left width=200>");
	f.writeln("Usage".fontcolor("white"));
	f.writeln("<th align=left>");
	f.writeln("Description".fontcolor("white"));

	for(method in obj._method_list) {
		f.write("<tr>");

		f.printf("<td>%s<td>%s<td><tt>%s.%s(%s)\r\n"
			,obj._method_list[method].name.bold()
			,obj._method_list[method].type
			,name
			,obj._method_list[method].name
			,obj._method_list[method].args
			);
		f.writeln("<td>" + obj._method_list[method].desc);
		/**
		f.write("<td>");
		if(obj._method_list[method].alias != undefined)
			f.writeln(obj._method_list[method].alias)
		**/
	}
}

function document_object(name, obj)
{
	var prop_name;

	if(table_depth)
		table_close();
	f.writeln("<h2>"+name+" object</h2>");
	table_open(name);
	f.writeln("<caption align=left><b>" + name.italics() + " properties" + "</b></caption>");
	f.writeln("<tr bgcolor=gray>");
	f.writeln("<th align=left width=100>");
	f.writeln("Name".fontcolor("white"));
	f.writeln("<th align=left width=100>");
	f.writeln("Type".fontcolor("white"));
	f.writeln("<th align=left>");
	f.writeln("Description".fontcolor("white"));

	p=0;
	for(prop in obj) {
		prop_name=name + "." + prop;

		if(typeof(obj[prop])=="object") {
			if(obj[prop].length != undefined)	// array ?
				document_object(prop_name + "[]",obj[prop][0]);
			else
				document_object(prop_name,obj[prop]);
			continue;
		} 
		f.write("<tr>");
		f.writeln("<td>" + prop.bold() + "<td>" + typeof(obj[prop]) );
		if(obj._property_desc_list!=undefined)
			f.writeln("<td>" + obj._property_desc_list[p++] + "</td>");
	}

	document_methods(name,obj);
	table_close();
}

// open HTML output file
f=new File("jsdocs.html");
if(!f.open("w")) {
	print("!Can't open output file");
	exit();
}

f.writeln("<html>");
f.writeln("<head>");
f.writeln("<title>Synchronet JavaScript Object Model Reference</title>");
f.writeln("</head>");

f.writeln("<body>");
f.writeln("<font face=arial,helvetica>");

f.writeln("<h1>Synchronet JavaScript Object Model Reference</h1>");
f.printf("Generated with Synchronet v%s%s compiled %s\r\n"
		 ,system.version,system.revision,system.compiled_when);

document_object("client"	,client);
document_object("system"	,system);
document_object("bbs"		,bbs);
document_object("user"		,user);
document_object("server"	,server);
document_object("console"	,console);
document_object("msg_area"	,msg_area);
document_object("file_area"	,file_area);
document_object("File"		,new File("bogusfile"));

f.close();