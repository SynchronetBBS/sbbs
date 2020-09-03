// jsdocs.js

// This script generates HTML documentation of the Synchronet JavaScript object model
// Requires a Debug build of the Synchronet executable(s)

// $Id: jsdocs.js,v 1.40 2020/04/20 06:31:15 rswindell Exp $

const table_tag = "<table border=1 width=100%>";

const li_tag =	"<li onclick = 'this.className = (this.className == \"showList\") ? \"defaultStyles\" : \"showList\";'\n" +
				"\tonselectstart = 'event.returnValue = false;'" +
				">";

var min_ver=0;
var max_ver=999999;
var total_methods=0;
var total_properties=0;
var table_depth=0;
var object_depth=0;

var body = "";
var f;

function docwrite(str)
{
	body+=str;
}

function docwriteln(str)
{
	docwrite(str + "\n");
}

function table_open(name)
{
	docwriteln(table_tag);
	table_depth++;
}

function table_close()
{
	if(table_depth) {
		docwriteln("</table>");
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
	var method;
	var func;

	if(obj._method_list == undefined)
		return;

	f.writeln(li_tag);
	f.writeln("<a href=#" + name +"_methods>methods</a>");

	table_close();
	table_open(name);

	
	docwriteln("<caption align=left><b><tt>" + name + "</tt>");
	docwriteln("<a name=" + name + "_methods> methods</a>"); 
	docwriteln("</b></caption>");
	docwriteln("<tr bgcolor=gray>");
	docwriteln("<th align=left width=100>");
	docwriteln("Name".fontcolor("white"));
	docwriteln("<th align=left width=100>");
	docwriteln("Returns".fontcolor("white"));
	docwriteln("<th align=left width=200>");
	docwriteln("Usage".fontcolor("white"));
	if(!min_ver && obj._method_list[0].ver) {
		docwriteln("<th align=left width=50>");
		docwriteln("Ver".fontcolor("white"));
	}
	docwriteln("<th align=left>");
	docwriteln("Description".fontcolor("white"));

	for(method in obj._method_list) {
		if(obj._method_list[method].ver < min_ver
			|| obj._method_list[method].ver > max_ver)
			continue;

		docwrite("<tr valign=top>");

		if(obj==js.global)
			func=obj._method_list[method].name;
		else
			func=name + '.' + obj._method_list[method].name;

		docwrite(format("<td>%s<td>%s<td><tt>%s(%s)\n"
			,obj._method_list[method].name.bold()
			,obj._method_list[method].type
			,func
			,obj._method_list[method].args
			));
		if(!min_ver && obj._method_list[method].ver)
			docwriteln("<td>" + verstr(obj._method_list[method].ver));
		docwriteln("<td>" + obj._method_list[method].desc);
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
	docwriteln("<h2><a name=" + name + ">" + name + " " + type + "</a>");
	if(obj._description!=undefined)
		docwriteln("<br><font size=-1>"+obj._description+"</font>");
	if(!min_ver && obj._ver>310)
		docwriteln("<font size=-1> - introduced in v"+verstr(obj._ver)+"</font>");
	docwriteln("</h2>");
	if(obj._constructor!=undefined)
		docwriteln("<p>" + obj._constructor + "</p>");
}

function properties_header(name, obj)
{

	f.writeln(li_tag);
	f.writeln("<a href=#" + name +"_properties>properties</a>");

	table_close();
	if(obj._method_list != undefined)
		docwriteln("<br>");

	table_open(name);
	docwriteln("<caption align=left><b><tt>" + name + "</tt>");
	docwriteln("<a name=" + name + "_properties> properties</a>"); 
	docwriteln("</b></caption>");
	docwriteln("<tr bgcolor=gray>");
	docwriteln("<th align=left width=100>");
	docwriteln("Name".fontcolor("white"));
	docwriteln("<th align=left width=100>");
	docwriteln("Type".fontcolor("white"));
	if(!min_ver && obj._property_ver_list && obj._property_ver_list.length) {
		docwriteln("<th align=left width=50>");
		docwriteln("Ver".fontcolor("white"));
	}
	docwriteln("<th align=left>");
	docwriteln("Description".fontcolor("white"));
}

function document_properties(name, obj)
{
	var prop_name;
	var count=0;
	var prop;
	var prop_num;
	var prop_hdr=false;
	var p;

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
			&& prop!="scope"
            ) {
			if(obj[prop]===null)
				continue;
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
		docwrite("<tr valign=top>");
		docwriteln("<td>" + prop.bold() + "<td>" + typeof(obj[prop]) );
		if(!min_ver && obj._property_ver_list)
			docwriteln("<td>" 
				+ (obj._property_ver_list[p] ? verstr(obj._property_ver_list[p]) : "N/A"));
		if(obj._property_desc_list!=undefined)
			docwriteln("<td>" + obj._property_desc_list[p]);
		p++;
		total_properties++;
	}
}

function document_object(name, obj, type)
{
	var i;

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
    f.writeln("\tcaption              { display: table-caption; text-align: left; caption-side: top; }");
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
docwriteln("<tr><td>" + "argc".bold() + "<td>number<td>N/A<td>count of arguments passed to the script</td>");
docwriteln("<tr><td>" + "argv".bold() + "<td>array<td>N/A<td>array of argument strings (argv.length == argc)</td>");
docwriteln("<tr><td>" + "errno".bold() + "<td>number<td>3.10h<td>last system error number</td>");
docwriteln("<tr><td>" + "errno_str".bold() + "<td>string<td>3.10h<td>description of last system error</td>");
docwriteln("<tr><td>" + "socket_errno".bold() + "<td>number<td>3.13a<td>last socket-related error number (same as <i>errno</i> on Unix platforms)</td>");
docwriteln("<tr><td>" + "socket_errno_str".bold() + "<td>string<td>3.18a<td>description of last socket-related error (same as <i>errno_str</i> on Unix platforms)</td>");
f.writeln("</ul>");

document_object("js"		,js);
if(js.global.system != undefined)		document_object("system"	,system);
if(js.global.server != undefined) 		document_object("server"	,server);
if(js.global.client != undefined)		document_object("client"	,client);
if(js.global.user != undefined)			document_object("user"		,user);
if(js.global.bbs != undefined)			document_object("bbs"		,bbs);
if(js.global.console != undefined)		document_object("console"	,console);
if(js.global.msg_area != undefined)		document_object("msg_area"	,msg_area);
if(js.global.file_area != undefined)	document_object("file_area"	,file_area);
if(js.global.xtrn_area != undefined)	document_object("xtrn_area"	,xtrn_area);
if(js.global.uifc != undefined)			document_object("uifc"		,uifc);
if(js.global.MsgBase != undefined)		document_object("MsgBase"	,new MsgBase(msg_area.grp_list[0].sub_list[0].code), "class");
if(js.global.File != undefined)			document_object("File"		,new File(system.devnull), "class");
if(js.global.Queue != undefined)		document_object("Queue"		,new Queue(), "class");
if(js.global.ConnectedSocket != undefined) {
	var sock=new ConnectedSocket("www.google.com", 80);
	sock.close();
	if(sock != undefined)		document_object("ConnectedSocket"	,sock, "class");
}
if(js.global.ListeningSocket != undefined) {
	var sock=new ListeningSocket("localhost", 0, "jsdocs");
	sock.close();
	if(sock != undefined)		document_object("ListeningSocket"	,sock, "class");
}
if(js.global.Socket != undefined) {
	var sock=new Socket();
	sock.close();
	if(js.global.client != undefined)
		sock.descriptor=client.socket.descriptor;
	if(sock != undefined)		document_object("Socket"	,sock, "class");
}
if(js.global.COM != undefined) {
	var com;
	if(system.platform=="Win32")
		com=new COM('COM1');
	else
		com=new COM('/dev/tty');
	com.close();
	if(com != undefined)		document_object("COM"	,com, "class");
}
if(js.global.CryptContext != undefined) {
	var cc = new CryptContext(CryptContext.ALGO.AES);
	if(cc != undefined)			document_object("CryptContext",cc, "class");
}
if(js.global.CryptKeyset != undefined) {
	var cks = new CryptKeyset("/tmp/tmpkeyset", CryptKeyset.KEYOPT.CREATE);
	if(cks != undefined)			document_object("CryptKeyset",cks, "class");
}
if(js.global.conio != undefined) {
	document_object("conio",js.global.conio);
}


f.writeln("</ol>");

f.write(body);

f.writeln("<p><small>");
f.writeln("Totals: " + total_properties + " properties, " + total_methods + " methods");

f.close();
