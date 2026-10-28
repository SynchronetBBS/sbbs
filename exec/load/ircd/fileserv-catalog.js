/* FileServ's virtual catalog and native BBS authorization. GPL-2.0-or-later. */
"use strict";
function IRC_files_masks(value, code) {
	return String(value || "").split(/[ ,]+/).some(function(mask) {
		if (!mask) return false;
		return IRC_files_match(mask,code);
	});
}
function IRC_files_match(mask,name) {
	var pattern=String(mask).replace(/[.+^${}()|[\]\\]/g,"\\$&").replace(/\*/g,".*").replace(/\?/g,".");
	return new RegExp("^"+pattern+"$","i").test(name);
}
/* Virtual components preserve short names, escaping separators and quotes so
   every displayed name can be supplied back to the command parser. */
function IRC_files_component(name) {
	name=String(name);
	if (name=="." || name=="..") return name.replace(/\./g,"%2E");
	return name.replace(/[%\/\\"\x00-\x1f\x7f]/g,function(c) { return "%"+("0"+c.charCodeAt(0).toString(16).toUpperCase()).slice(-2); });
}
function IRC_files_label(dir) { return IRC_files_component(dir.lib_name)+"/"+IRC_files_component(dir.name || dir.code); }
function IRC_files_argument(name) { return /\s/.test(name) ? '"'+name+'"' : name; }
function IRC_files_clean(value) { return String(value || "").replace(/[\x00-\x1f\x7f]/g, " "); }
function IRC_files_name(name) { return typeof name == "string" && name.length > 0 && name.length <= 255 && !/[\x00-\x1f\x7f/\\:"*?]/.test(name) && name != "." && name != ".."; }
function IRC_files_args(text) {
	var args=[],pattern=/"([^"\r\n]*)"|([^\s"]+)/g,m,last=0;
	while ((m=pattern.exec(text))) {
		if (text.substr(last,m.index-last).trim()) throw new Error("Unclosed quote in FileServ command.");
		args.push(m[1]===undefined ? m[2] : m[1]);last=pattern.lastIndex;
	}
	if (text.substr(last).trim()) throw new Error("Unclosed quote in FileServ command.");
	return args;
}
function IRC_files_active(user) { return user && user.number && !(user.settings & (USER_DELETED | USER_INACTIVE)); }
function IRC_files_guest(config) {
	var n = Number(config.guest), u;
	if (n) { u = new User(n); return IRC_files_active(u) && (u.security.restrictions & UFLAG_G) ? u : null; }
	for (n = 1; n <= system.last_user; n++) {
		u = new User(n);
		if (IRC_files_active(u) && (u.security.restrictions & UFLAG_G)) return u;
	}
	return null;
}
function IRC_files_user(identity, config) {
	var u = identity ? new User(identity.number) : IRC_files_guest(config);
	if (!IRC_files_active(u) || (identity && (u.stats.firston_date != identity.created || (u.security.restrictions & UFLAG_G))))
		throw new Error("No eligible BBS account. Identify with NickServ on this BBS, or ask its sysop to configure guest access.");
	return u;
}
var IRC_files_user_directory;
function IRC_files_special(dir) {
	if (dir.is_upload || dir.is_sysop || dir.is_offline) return true;
	/* An absent special-directory property makes the native resolver rebuild
	   file_area. Its configuration is fixed for this runtime: resolve once,
	   including the absent case. The other special directories have flags. */
	if (IRC_files_user_directory===undefined) {
		var d=file_area.user_dir;
		IRC_files_user_directory=d ? (typeof d=="object" ? d.code : d) : null;
	}
	return IRC_files_user_directory===dir.code;
}
function IRC_files_allowed(dir, user, config, anonymous, download) {
	var lib = dir && file_area.lib[dir.lib_name];
	return !!(lib && !IRC_files_special(dir)
		&& IRC_files_masks(config.include_libs, lib.code_prefix) && !IRC_files_masks(config.exclude_libs, lib.code_prefix)
		&& IRC_files_masks(config.include_dirs, dir.code) && !IRC_files_masks(config.exclude_dirs, dir.code)
		&& (!anonymous || IRC_files_masks(config.public_libs, lib.code_prefix) || IRC_files_masks(config.public_dirs, dir.code))
		&& user.can_access_dir(dir.number) && (!download || user.can_access_dir(dir.number, "download")));
}
function IRC_files_directory(code, user, config, anonymous, download, library) {
	var target=String(code).replace(/^\//,""),key=target.toLowerCase();
	var dir=Object.prototype.hasOwnProperty.call(file_area.dir,key) ? file_area.dir[key] : null;
	if (!dir) {
		var matches=Object.keys(file_area.dir).map(function(key) { return file_area.dir[key]; }).filter(function(d) {
			return (IRC_files_label(d).toLowerCase()==target.toLowerCase() || ((!library || d.lib_name==library) && IRC_files_component(d.name || d.code).toLowerCase()==target.toLowerCase()))
				&& IRC_files_allowed(d,user,config,anonymous,download);
		});
		if (matches.length>1) throw new Error("Directory name is ambiguous. Use LIBRARY/DIRECTORY or a full internal directory code.");
		dir=matches[0];
	}
	if (!dir || !IRC_files_allowed(dir, user, config, anonymous, download)) throw new Error("Directory unavailable or access denied.");
	return dir;
}
function IRC_files_library(name,user,config,anonymous) {
	var dirs=file_area.dir,keys=Object.keys(file_area.lib).filter(function(key) {
		return IRC_files_component(key).toLowerCase()==String(name).toLowerCase() && Object.keys(dirs).some(function(code) {
			return dirs[code].lib_name==key && IRC_files_allowed(dirs[code],user,config,anonymous,false);
		});
	});
	if (keys.length>1) throw new Error("Library name is ambiguous.");
	return keys[0];
}
function IRC_files_cd(target,user,config,anonymous,library) {
	var absolute=String(target)[0]=="/",value=String(target).replace(/^\//,""),lib=IRC_files_library(value,user,config,anonymous);
	if (lib) return {library:lib,dir:""};
	var dir=IRC_files_directory(value,user,config,anonymous,false,absolute ? null : library);
	return {library:dir.lib_name,dir:dir.code};
}
function IRC_files_free(user, dir) {
	if ((dir.settings & DIR_FREE) || (user.security.exemptions & UFLAG_D) || (dir.exempt_ars && user.compare_ars(dir.exempt_ars))) return true;
	/* The library exemption ARS is not exposed by file_area. Read only that
	   native setting, preserving the same native ARS evaluation. */
	var cfg = new File(system.ctrl_dir + "file.ini");
	if (!cfg.open("r", true)) throw new Error("Cannot read native library download exemptions.");
	try {
		var ars = cfg.iniGetValue("lib:" + dir.lib_name, "exempt_ars", "");
		return !!(ars && user.compare_ars(ars));
	} finally { cfg.close(); }
}
function IRC_files_file(code, name, user, config, anonymous, download, library) {
	if (!IRC_files_name(name)) throw new Error("Use a single catalog filename without paths or control characters.");
	var dir = IRC_files_directory(code, user, config, anonymous, download, library), base = new FileBase(dir.code);
	if (!base.open()) throw new Error("File catalog unavailable.");
	try {
		var meta = base.get(name);
		if (!meta || !IRC_files_name(meta.name) || meta.to_list) throw new Error("File unavailable.");
		var path = base.get_path(meta.name), size = file_size(path);
		/* Resolve symlinks in this worker, never in the IRC callback. Both
		   paths are trusted/native catalog values and shell-quoted. */
		var root = backslash(IRC_files_realpath(dir.path)), actual = IRC_files_realpath(path);
		if (!actual || actual.indexOf(root) != 0 || file_isdir(actual) || size < 0 || size > 4294967295) throw new Error("File unavailable or exceeds the 4 GiB DCC limit.");
		var cost = IRC_files_free(user, dir) ? 0 : Number(meta.cost || 0);
		if (download && cost > Number(user.security.credits) + Number(user.security.free_credits)) throw new Error("Insufficient BBS download credits.");
		return {dir:dir.code, label:IRC_files_label(dir), name:meta.name, size:size, cost:cost, desc:IRC_files_clean(meta.desc), path:actual};
	} finally { base.close(); }
}
function IRC_files_catalog(config, identity, command, args, context) {
	var user = IRC_files_user(identity, config), anon = !identity, rows = [], page = 1, dirs = file_area.dir;
	context=context || {};
	function paging() { if (args.length>((command=="LIST" || command=="SEARCH") ? 1 : 0) && /^\d+$/.test(args[args.length-1])) page = Math.max(1, Math.min(100000, Number(args.pop()))); }
	function result() {
		var start = (page-1)*config.page_size, out = rows.slice(start, start+config.page_size);
		out.push("Page " + page + "; " + rows.length + " result(s)." + (start+config.page_size < rows.length ? " Request the next page number for more." : ""));
		return {lines:out};
	}
	if (command == "GET" || command == "INFO") {
		if (args.length != 2) throw new Error("Syntax: " + command + " <library/directory> <filename>");
		var file = IRC_files_file(args[0], args[1], user, config, anon, command == "GET",context.library);
		file.account={number:user.number,created:user.stats.firston_date};
		return command == "GET" ? {file:file} : {lines:[file.label + "/" + file.name + " " + file.size + " bytes; " + file.cost + " credits. " + file.desc]};
	}
	paging();
	if (command == "LIBS" || command == "DIRS") {
		if (args.length>1 || (command=="LIBS" && args.length && !context.browse)) throw new Error(command=="LIBS" ? "Syntax: LIBS [page]" : "Syntax: DIRS [library-name-mask] [page]");
		var seen=Object.create(null),entries=[];
		Object.keys(dirs).forEach(function(code) {
			var d = dirs[code], l = file_area.lib[d.lib_name];
			if (command=="LIBS") {
				if (seen[d.lib_name] || (args.length && !IRC_files_match(args[0],IRC_files_component(d.lib_name)))) return;
			} else if (context.browse && context.library) {
				if (d.lib_name!=context.library || (args.length && !IRC_files_match(args[0],IRC_files_component(d.name || d.code)))) return;
			} else if (args.length && !IRC_files_match(args[0],IRC_files_component(d.lib_name))) return;
			if (!IRC_files_allowed(d,user,config,anon,false)) return;
			if (command == "LIBS") {
				entries.push({name:IRC_files_component(d.lib_name),desc:l.description});seen[d.lib_name]=true;
			} else if (context.browse && context.library) {
				entries.push({name:IRC_files_component(d.name || d.code),desc:d.description});
			} else entries.push({name:IRC_files_label(d),desc:d.description});
		});
		entries.sort(function(a,b) { return a.name.toLowerCase().localeCompare(b.name.toLowerCase()); });
		rows=entries.map(function(e) { return IRC_files_argument(e.name)+" : "+IRC_files_clean(e.desc); });return result();
	}
	if (command != "LIST" && command != "SEARCH") throw new Error("Unknown catalog command.");
	if (!args.length || args.length > 2) throw new Error(command == "LIST" ? "Syntax: LIST <library/directory> [filename-mask] [page]" : "Syntax: SEARCH <text> [page]");
	var search = args[0].toLowerCase(), selected = command == "LIST" ? [IRC_files_directory(args[0],user,config,anon,false,context.library).code] : Object.keys(dirs).sort();
	selected.forEach(function(code) {
		var dir = dirs[code]; if (!IRC_files_allowed(dir,user,config,anon,false)) return;
		var base = new FileBase(code); if (!base.open()) return;
		try { (base.get_list(command == "LIST" ? (args[1] || "*") : "*") || []).forEach(function(f) {
			if (!IRC_files_name(f.name) || f.to_list || (command == "SEARCH" && (f.name + " " + f.desc).toLowerCase().indexOf(search) < 0)) return;
			rows.push((context.directory==code && command=="LIST" ? "" : IRC_files_label(dir)+"/") + f.name + " " + Number(f.size || 0) + " bytes : " + IRC_files_clean(f.desc));
		}); } finally { base.close(); }
	}); return result();
}

function IRC_files_realpath(path) {
	var quoted="'"+String(path).replace(/'/g,"'\"'\"'")+"'";
	var lines=system.popen("realpath "+quoted+" 2>/dev/null");
	if (!lines || lines.length!=1 || !lines[0]) throw new Error("Cannot resolve catalog path.");
	return lines[0].replace(/[\r\n]+$/, "");
}

function IRC_files_mac(key,text) {
	var c=new CryptContext(CryptContext.ALGO["HMAC-SHA2"]);c.set_key(key);c.encrypt(text);
	return c.hashvalue.replace(/[\s\S]/g,function(b) { return ("0"+b.charCodeAt(0).toString(16)).slice(-2); });
}

/* Normalize literal addresses without invoking DNS, including expanded IPv6
			and IPv4-mapped peers. Unspecified addresses cannot identify a DCC peer. */
function IRC_files_ip(address) {
	address=String(address || "").replace(/^::ffff:/i,"");
	if (/^[0-9]+(?:\.[0-9]+){3}$/.test(address)) {
		var a=address.split(".").map(Number);
		return a.some(function(n) { return n>255; }) || a.every(function(n) { return n==0; }) ? null : a.join(".");
	}
	if (!/^[0-9a-f:]+$/i.test(address) || address.indexOf(":")<0) return null;
	var halves=address.toLowerCase().split("::"),parts;
	if (halves.length>2) return null;
	if (halves.length==1) { parts=address.split(":");if (parts.length!=8) return null; }
	else {
		var left=halves[0] ? halves[0].split(":") : [],right=halves[1] ? halves[1].split(":") : [];
		if (left.length+right.length>=8) return null;
		parts=left.concat(new Array(9-left.length-right.length).join("0").split("")).concat(right);
	}
	if (parts.length!=8 || parts.some(function(p) { return !/^[0-9a-f]{1,4}$/i.test(p); })) return null;
	parts=parts.map(function(p) { return parseInt(p,16); });
	if (parts.every(function(n) { return n==0; })) return null;
	if (parts.slice(0,5).every(function(n) { return n==0; }) && parts[5]==65535)
		return [parts[6]>>8,parts[6]&255,parts[7]>>8,parts[7]&255].join(".");
	return parts.map(function(n) { return n.toString(16); }).join(":");
}
