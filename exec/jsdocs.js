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
		write("<tr>");

		write(format("<td>%s<td>%s<td><tt>%s.%s(%s)\n"
			,obj._method_list[method].name.bold()
			,obj._method_list[method].type
			,name
			,obj._method_list[method].name
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

function document_object(name, obj)
{
	var prop_name;

	f.writeln("<li><a href=#" + name +">" + name + "</a>");

	if(table_depth)
		table_close();
	writeln("<h2><a name=" + name + ">" + name + " object</a>");
	if(obj._description!=undefined)
		writeln("<br><font size=-1>"+obj._description+"</font>");
	writeln("</h2>");
	if(obj._constructor!=undefined)
		writeln("<p>" + obj._constructor + "</p>");
	table_open(name);
	writeln("<caption align=left><b>" + name.italics() + " properties" + "</b></caption>");
	writeln("<tr bgcolor=gray>");
	writeln("<th align=left width=100>");
	writeln("Name".fontcolor("white"));
	writeln("<th align=left width=100>");
	writeln("Type".fontcolor("white"));
	writeln("<th align=left>");
	writeln("Description".fontcolor("white"));

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
		write("<tr>");
		writeln("<td>" + prop.bold() + "<td>" + typeof(obj[prop]) );
		if(obj._property_desc_list!=undefined)
			writeln("<td>" + obj._property_desc_list[p++] + "</td>");
	}

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
f.writeln("<font face=arial,helvetica>");

f.writeln("<h1>Synchronet JavaScript Object Model Reference</h1>");
f.printf("Generated with Synchronet v%s compiled %s\n"
		 ,system.full_version,system.compiled_when);

f.writeln("<ul>");

document_object("system"	,system);
document_object("server"	,server);
document_object("client"	,client);
document_object("user"		,user);
document_object("bbs"		,bbs);
document_object("console"	,console);
document_object("msg_area"	,msg_area);
document_object("file_area"	,file_area);
document_object("File"		,new File("bogusfile"));

f.writeln("</ul>");

f.write(body);

f.close();