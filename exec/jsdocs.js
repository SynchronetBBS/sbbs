// jsdocs.js

// This script will (eventually be used to) generate HTML documentation of the
// Synchronet JavaScript object model

const table_tag = "<table border=1 width=100%>";

table_depth=0;

body = "";

function write(str)
{
	body+=str;
}

function writeln(str)
{
	write(str + "\n");
}

function table_open(name)
{
	writeln(table_tag);
	table_depth++;
}

function table_close()
{
	if(table_depth) {
		writeln("</table>");
		table_depth--;
	}
}

function document_methods(name,obj)
{
	if(obj._method_list == undefined)
		return;

	//f.writeln(" [<a href=#" + name +"_methods>methods</a>]");

	table_close();
	writeln("<br>");
	table_open(name);

	
	writeln("<caption align=left><b>" + name.italics());
	writeln("<a name=" + name + "_methods> methods</a>"); 
	writeln("</b></caption>");
	writeln("<tr bgcolor=gray>");
	writeln("<th align=left width=100>");
	writeln("Name".fontcolor("white"));
	writeln("<th align=left width=100>");
	writeln("Returns".fontcolor("white"));
	writeln("<th align=left width=200>");
	writeln("Usage".fontcolor("white"));
	writeln("<th align=left>");
	writeln("Description".fontcolor("white"));

	for(method in obj._method_list) {
		write("<tr valign=top>");

		if(obj==_global)
			func=obj._method_list[method].name;
		else
			func=name + '.' + obj._method_list[method].name;

		write(format("<td>%s<td>%s<td><tt>%s(%s)\n"
			,obj._method_list[method].name.bold()
			,obj._method_list[method].type
			,func
			,obj._method_list[method].args
			));
		writeln("<td>" + obj._method_list[method].desc);
		/**
		write("<td>");
		if(obj._method_list[method].alias != undefined)
			writeln(obj._method_list[method].alias)
		**/
	}
}

function object_header(name, obj)
{
	f.writeln("<li><a href=#" + name +">" + name + "</a>");

	if(table_depth)
		table_close();
	writeln("<h2><a name=" + name + ">" + name + " object</a>");
	if(obj._description!=undefined)
		writeln("<br><font size=-1>"+obj._description+"</font>");
	writeln("</h2>");
	if(obj._constructor!=undefined)
		writeln("<p>" + obj._constructor + "</p>");
}

function properties_header(name)
{
	table_open(name);
	writeln("<caption align=left><b>" + name.italics() + " properties" + "</b></caption>");
	writeln("<tr bgcolor=gray>");
	writeln("<th align=left width=100>");
	writeln("Name".fontcolor("white"));
	writeln("<th align=left width=100>");
	writeln("Type".fontcolor("white"));
	writeln("<th align=left>");
	writeln("Description".fontcolor("white"));
}

function document_properties(name, obj)
{
	var prop_name;

	properties_header(name);

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
		write("<tr valign=top>");
		writeln("<td>" + prop.bold() + "<td>" + typeof(obj[prop]) );
		if(obj._property_desc_list!=undefined)
			writeln("<td>" + obj._property_desc_list[p++] + "</td>");
	}
}

function document_object(name, obj)
{
	
	object_header(name,obj);
	document_properties(name,obj);
	document_methods(name,obj);
	table_close();
}

// open HTML output file
f=new File("jsdocs.html");
if(!f.open("w")) {
	printf("!Error %d opening output file\n",errno);
	exit();
}

f.writeln("<html>");
f.writeln("<head>");
f.writeln("<title>Synchronet JavaScript Object Model Reference</title>");
f.writeln("</head>");

f.writeln("<body>");
f.writeln("<font face=arial,helvetica size=-1>");

f.writeln("<h1>Synchronet JavaScript Object Model Reference</h1>");
f.printf("Generated for <b>Synchronet v%s</b>, compiled %s\n"
		 ,system.full_version,system.compiled_when);

f.writeln("<ul>");

object_header("global"		,_global);
properties_header("global");
writeln("<tr><td>" + "argc".bold() + "<td>number<td>number of arguments passed to the script</td>");
writeln("<tr><td>" + "argv".bold() + "<td>array<td>array of argument strings (argv.length == argc)</td>");
writeln("<tr><td>" + "errno".bold() + "<td>number<td>last system error number</td>");
writeln("<tr><td>" + "errno_str".bold() + "<td>string<td>description of last system error</td>");
document_methods("global"	,_global);

document_object("system"	,system);
document_object("server"	,server);
document_object("client"	,client);
document_object("user"		,user);
document_object("bbs"		,bbs);
document_object("console"	,console);
document_object("msg_area"	,msg_area);
document_object("file_area"	,file_area);
document_object("MsgBase"	,new MsgBase("mail"));
document_object("File"		,new File("bogusfile"));

f.writeln("</ul>");

f.write(body);

f.close();