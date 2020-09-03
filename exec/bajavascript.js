/* bajavascript.c */

/* Synchronet command shell/module compiler */

/* $Id: bajavascript.js,v 1.2 2014/02/04 09:23:17 deuce Exp $ */

load("bajalib.js");

function BAJAVA_cmd(cmd_name)
{
	if(baja[cmd_name.toUpperCase()]==undefined)
		throw("Undefined BAJA command "+cmd_name.toUpperCase());
	this.cmd=baja[cmd_name.toUpperCase()];
	this.args=[];
}

function BAJAVA_var(name, type, value)
{
	this.name=name;
	this.value=value;
	this.type=type;
}

function BAJAVA_program()
{
	this.cmds=[];
	this.next_cmd=0;
	this.stack=[];
	this.defines={};
	this.globals={};
	this.labels={};
	this.vars={};
}

function BAJAVA_compile(path, program)
{
	var cmds=[];
	var infile;
	var str;
	var defines={};
	var m;
	var tmp;
	var lnum=0;
	var i;

	function expdefs(str) {
		var ret='';
		var inner_i;
		var quoting=false;
		var dname;

		for(inner_i=0; inner_i<str.length; inner_i++) {
			if(quoting) {
				ret += str.charAt(inner_i);
				if(str.charAt(inner_i) == '"')
					quoting=false;
				continue;
			}
			if(str.charAt(inner_i)=='"') {
				ret += '"';
				continue;
			}
			if(str.charAt(inner_i)==' ') {
				ret += ' ';
				continue;
			}

			dname='';
			for(;inner_i<str.length;inner_i++) {
				if(str.charAt(inner_i).search(/[A-Za-z0-9_]/)==0) {
					dname += str.charAt(inner_i);
				}
				else {
					break;
				}
			}
			if(dname.length > 0) {
				if(program.defines[dname] != undefined) {
					ret += program.defines[dname];
				}
				else {
					ret += dname;
				}
			}
			ret += str.charAt(inner_i);
		}
		return ret;
	}

	function getcrc(arg) {
		var name = arg.replace(/ .*$/,'').toUpperCase();
		if(name=='STR')
			return 0;
		if(name.substr(0,4)=='VAR_')
			return parseInt(name.substr(4, 16));
		name=crc32_calc(name);
		return name;
	}

	function get_varcrc(arg) {
		var name=getcrc(arg);
		if(program.vars[name]==undefined)
			throw("!SYNTAX ERROR (expecting variable name): "+path+":"+lnum+" "+str);
	}

	function newvar(arg, global, type) {
		if(arg.search(/^[0-9]/)==0)
			throw("!SYNTAX ERROR (illegal variable name): "+path+":"+lnum+" "+str);
		var name=getcrc(arg);
		if(program.vars[name]==undefined) {
			if(global) {
				if(bbs.mods.BAJAVAScript_vars==undefined)
					bbs.mods.BAJAVAScript_vars = {};
				if(bbs.mods.BAJAVAScript_vars[name]==undefined)
					bbs.mods.BAJAVAScript_vars[name] = new BAJAVA_var(name, type);
				program.vars[name]=bbs.mods.BAJAVAScript_vars[name];
			}
			else
				program.vars[name]=new BAJAVA_var(name, type)
		}
	}

	if(path.search(/^(?:[A-Z]:)?[\/\\]/)!=0)
		path=system.exec_dir+path;
	infile=new File(path);

	if(program==undefined)
		program=new BAJAVA_program();

	if(!infile.open("rb")) {
		print(format("error %d opening %s for read\n",infile.error,path));
		exit(1);
	}
	while(!(infile.eof || infile.error)) {
		str=infile.readln(1000);
		lnum++;
		if(str==undefined)
			break;
		str=truncsp(str);
		str=str.replace(/\t/g,' ');
		str=str.replace(/^[\x00- ]*/g,'');
		if(str.length==0)
			continue;
		if(str.charAt(0)=='#')
			continue;
		str=expdefs(str);
		// TODO: This may be easier to use a standard parser...
		m=str.match(/([^ ]+)(?: [\x00- ]*([^ ]+ [\x00- ]*([^ ]+ [\x00- ]*([^ ]+ [\x00- ]*([^ ]+.*?)?)?)?)?)$/);
		if(m==null)
			throw("Unhandled command "+str);
		m[1]=m[1].toUpperCase();
		switch(m[1].charAt(0)) {
			case '!':
				switch(m[1].substr(1)) {
					case 'INCLUDE':
						BAJAVA_compile(m[2], program);
						break;
					case 'DEFINE':
						throw("!DEFINE NOT YET IMPLEMENTED!");
						break;
					case 'GLOBAL':
						if(m[2] == undefined)
							break;
						tmp='';
						for(i=0; i<m[2].length; i++) {
							switch(m[2].charAt(i)) {
								case ' ':
									if(tmp.length)
										newvar(tmp, true);
									newvar(tmp, true);
									tmp='';
									break;
								case '#':
									if(tmp.length)
										newvar(tmp, true);
									i=m[2].length;
									tmp='';
									break;
								default:
									tmp += m[2].charAt(i);
									break;
							}
						}
						if(tmp.length)
							newvar(tmp, true);
						break;
				}
				break;
			case ':':
				tmp=m[1].substr(1);
				if(program.labels[tmp]!=undefined)
					throw("Duplicate label "+tmp+" defined at "+path+":"+lnum);
				program.labels[tmp]=program.cmds.length;
				break;
			default:
				program.cmds.push(new BAJAVA_cmd(m[1], m.slice(3)));
		}
	}
	return program;
}
