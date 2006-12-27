// jsdocs.js

// This script generates HTML documentation of the Synchronet JavaScript object model
// Requires a Debug build of the Synchronet executable(s)

// $Id$

const table_tag = "<table border=1 width=100%>";

const li_tag =	"<li onclick = 'this.className = (this.className == \"showList\") ? \"defaultStyles\" : \"showList\";'\n" +
				"\tonselectstart = 'event.returnValue = false;'" +
				">";

min_ver=0;
max_ver=999999;
total_methods=0;
total_properties=0;
table_depth=0;
object_depth=0;

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

function verstr(ver)
{
	var str=format("%u.%u",ver/10000,(ver%10000)/100);
	if(ver%100)
		str+=format('%c',ascii('a')+(ver%100));
	return str;
}

function document_methods(name,obj)
{
	if(obj._method_list == undefined)
		return;

	f.writeln(li_tag);
	f.writeln("<a href=#" + name +"_methods>methods</a>");

	table_close();
	table_open(name);

	
	writeln("<caption align=left><b><tt>" + name + "</tt>");
	writeln("<a name=" + name + "_methods> methods</a>"); 
	writeln("</b></caption>");
	writeln("<tr bgcolor=gray>");
	writeln("<th align=left width=100>");
	writeln("Name".fontcolor("white"));
	writeln("<th align=left width=100>");
	writeln("Returns".fontcolor("white"));
	writeln("<th align=left width=200>");
	writeln("Usage".fontcolor("white"));
	if(!min_ver && obj._method_list[0].ver) {
		writeln("<th align=left width=50>");
		writeln("Ver".fontcolor("white"));
	}
	writeln("<th align=left>");
	writeln("Description".fontcolor("white"));

	for(method in obj._method_list) {
		if(obj._method_list[method].ver < min_ver
			|| obj._method_list[method].ver > max_ver)
			continue;

		write("<tr valign=top>");

		if(obj==js.global)
			func=obj._method_list[method].name;
		else
			func=name + '.' + obj._method_list[method].name;

		write(format("<td>%s<td>%s<td><tt>%s(%s)\n"
			,obj._method_list[method].name.bold()
			,obj._method_list[method].type
			,func
			,obj._method_list[method].args
			));
		if(!min_ver && obj._method_list[method].ver)
			writeln("<td>" + verstr(obj._method_list[method].ver));
		writeln("<td>" + obj._method_list[method].desc);
		total_methods++;
	}
}

function object_header(name, obj, type)
{
	if(type==undefined)
		type="object";

	f.writeln(li_tag);
	if(!object_depth)
		f.write("[+] &nbsp");
	f.writeln(name.bold().link("#"+name) + " " + type);

	if(table_depth)
		table_close();
	writeln("<h2><a name=" + name + ">" + name + " " + type + "</a>");
	if(obj._description!=undefined)
		writeln("<br><font size=-1>"+obj._description+"</font>");
	if(!min_ver && obj._ver>310)
		writeln("<font size=-1> - introduced in v"+verstr(obj._ver)+"</font>");
	writeln("</h2>");
	if(obj._constructor!=undefined)
		writeln("<p>" + obj._constructor + "</p>");
}

function properties_header(name, obj)
{

	f.writeln(li_tag);
	f.writeln("<a href=#" + name +"_properties>properties</a>");

	table_close();
	if(obj._method_list != undefined)
		writeln("<br>");

	table_open(name);
	writeln("<caption align=left><b><tt>" + name + "</tt>");
	writeln("<a name=" + name + "_properties> properties</a>"); 
	writeln("</b></caption>");
	writeln("<tr bgcolor=gray>");
	writeln("<th align=left width=100>");
	writeln("Name".fontcolor("white"));
	writeln("<th align=left width=100>");
	writeln("Type".fontcolor("white"));
	if(!min_ver && obj._property_ver_list && obj._property_ver_list.length) {
		writeln("<th align=left width=50>");
		writeln("Ver".fontcolor("white"));
	}
	writeln("<th align=left>");
	writeln("Description".fontcolor("white"));
}

function document_properties(name, obj)
{
	var prop_name;
	var count=0;
	var prop_num;

	prop_hdr=false;

	p=0;
	for(prop in obj) {
		prop_num=count++;

		if(min_ver && (!obj._property_ver_list || !obj._property_ver_list[prop_num])) {
			p++;
			continue;
		}
		if(obj._property_ver_list 
			&& (obj._property_ver_list[prop_num] < min_ver 
			||  obj._property_ver_list[prop_num] > max_ver)) {
			p++;
			continue;
		}

		prop_name=name + "." + prop;

		if(typeof(obj[prop])=="object" 
			&& prop!="socket" 
			&& prop!="global"
			&& prop!="dir"
			&& prop!="sub") {
			if(obj[prop].length!=undefined) {
				if(typeof(obj[prop][0])=="object") {	// array ?
					document_object(prop_name /*+ "[]"*/,obj[prop][0], "array");
					continue;
				}
			}
			else {
				document_object(prop_name,obj[prop]);
				continue;
			}
		} 
		if(!prop_hdr) {
			properties_header(name, obj);
			prop_hdr=true;
		}
		write("<tr valign=top>");
		writeln("<td>" + prop.bold() + "<td>" + typeof(obj[prop]) );
		if(!min_ver && obj._property_ver_list)
			writeln("<td>" 
				+ (obj._property_ver_list[p] ? verstr(obj._property_ver_list[p]) : "N/A"));
		if(obj._property_desc_list!=undefined)
			writeln("<td>" + obj._property_desc_list[p]);
		p++;
		total_properties++;
	}
}

function document_object(name, obj, type)
{
	if(obj._ver > max_ver)
		return;

	printf("Documenting: %s\r\n",name);
	object_header(name,obj,type);
	if(obj._dont_document==undefined) {
		if(obj._assoc_array!=undefined)
			for(i in obj) {
				obj=obj[i];
				break;
			}
		f.writeln("<ul>");
		document_methods(name,obj);
		object_depth++;
		document_properties(name,obj);
		object_depth--;
		f.writeln("</ul>");
		table_close();
	}
}

// open HTML output file
f=new File("../docs/jsobjs.html");
if(!f.open("w")) {
	printf("!Error %d opening output file\n",errno);
	exit();
}

f.writeln("<html>");
f.writeln("<head>");
f.writeln("<title>Synchronet JavaScript Object Model Reference</title>");

if(1) {	/* Style sheet */
	f.writeln("<STYLE>");
	f.writeln("\tOL LI                { cursor: hand; }");
	f.writeln("\tUL LI                { display: none;list-style: square; }");
	f.writeln("\t.showList LI         { display: list-item; }");
	f.writeln("</STYLE>");
}

f.writeln("</head>");

f.writeln("<body>");
f.writeln("<font face=arial,helvetica>");

f.writeln("<h1>Synchronet JavaScript Object Model Reference</h1>");
f.printf("Generated for <b>Synchronet v%s</b>, compiled %s\n"
		 ,system.full_version.replace(/ Debug/,""),system.compiled_when);
f.writeln("<br><font size=-1>");
if(min_ver)
	f.writeln("Includes Properties and Methods added or substantially modified in Synchronet v" + verstr(min_ver) + " only.");
else
	f.writeln("Property and Method version numbers (when available) indicate the Synchronet version when the " +
		  "item was added or modified.");
f.writeln("</font>");

f.writeln("<ol type=square>");

object_header("global"		,js.global);
f.writeln("<ul>");
document_methods("global"	,js.global);
properties_header("global"	,js.global);
writeln("<tr><td>" + "argc".bold() + "<td>number<td>number of arguments passed to the script</td>");
writeln("<tr><td>" + "argv".bold() + "<td>array<td>array of argument strings (argv.length == argc)</td>");
writeln("<tr><td>" + "errno".bold() + "<td>number<td>last system error number</td>");
writeln("<tr><td>" + "errno_str".bold() + "<td>string<td>description of last system error</td>");
writeln("<tr><td>" + "socket_errno".bold() + "<td>number<td>last socket-related error number (same as <i>errno</i> on Unix platforms)</td>");
f.writeln("</ul>");

document_object("js"		,js);
document_object("system"	,system);
document_object("server"	,server);
document_object("client"	,client);
document_object("user"		,user);
document_object("bbs"		,bbs);
document_object("console"	,console);
document_object("msg_area"	,msg_area);
document_object("file_area"	,file_area);
document_object("xtrn_area"	,xtrn_area);
document_object("MsgBase"	,new MsgBase(msg_area.grp_list[0].sub_list[0].code), "class");
document_object("File"		,new File(system.devnull), "class");
document_object("Queue"		,new Queue(), "class");
sock=new Socket();
sock.close();
sock.descriptor=client.socket.descriptor;
document_object("Socket"	,sock, "class");

f.writeln("</ol>");

f.write(body);

f.writeln("<p><small>");
f.writeln("Totals: " + total_properties + " properties, " + total_methods + " methods");

f.close();