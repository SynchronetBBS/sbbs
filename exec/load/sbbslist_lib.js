// $Id$

// Synchronet BBS List (SBL) v4 Library

load("synchronet-json.js"); // For compatibility with older verisons of Synchronet/JavaScript-C without the JSON object

var list_fname = system.data_dir + 'sbbslist.json';

var sort_property = 'name';

function compare(a, b)
{
	if(a[sort_property].toLowerCase()>b[sort_property].toLowerCase()) return 1; 
	if(a[sort_property].toLowerCase()<b[sort_property].toLowerCase()) return -1;
	return 0;
}

function read_list()
{
    var f = new File(list_fname);
    log(LOG_INFO, "Opening list file: " + f.name);
    if(!f.open("r")) {
        log(LOG_ERR, "SBBSLIST: Error " + f.error + " opening " + f.name);
        return null;
    }

    var buf = f.read();
    f.close();
	truncsp(buf);
    var list = [];
	if(buf.length) {
		try {
			list=JSON.parse(buf);
		} catch(e) {
			log(LOG_ERR, "SBBSLIST: JSON.parse exception: " + e);
		}
	}
    return list;
}

function write_list(list)
{
    var out = new File(list_fname);
    log(LOG_INFO, "SBBSLIST: Opening / creating list file: " + list_fname);
    if(!out.open("w+")) {
        log(LOG_ERR, "SBBSLIST: Error " + out.error + " creating " + out.name);
        return false;
    }

    log(LOG_INFO, "SBBSLIST: Writing list file: " + out.name + " (" + list.length + " BBS entries)");
    out.write(JSON.stringify(list, null, 4));
    out.close();
    return true;
}

function remove_dupes(list)
{
    var new_list=[];
    var names=[];
    var address=[];
    var i;

    for(i in list)
        if(names.indexOf(list[i].name.toLowerCase()) < 0 && address.indexOf(list[i].service[0].address.toLowerCase()) < 0) {
            names.push(list[i].name.toLowerCase());
            address.push(list[i].service[0].address.toLowerCase());
            new_list.push(list[i]);
        }

    return new_list;
}

function imsg_capable_system(bbs)
{
	if(!bbs.entry.autoverify || !bbs.entry.autoverify.last_success)
		return false;
	var services = bbs.entry.autoverify.last_success.other_services;
	if(services.udp.indexOf("finger")<0	&& services.udp.indexOf("systat")<0)
		return false;
	if(services.tcp.indexOf("msp")<0 && services.tcp.indexOf("smtp")<0 && services.tcp.indexOf("submission")<0)
		return false;
	return true;
}

function system_index(list, name)
{
    name = name.toLowerCase();
    for(var i in list)
        if(list[i].name.toLowerCase() == name)
            return i;

    return -1;
}


function system_exists(list, name)
{
    return system_index(list, name) >= 0;
}

function new_system(name, nodes, stats)
{
	return {
		name: name, 
		entry:{}, 
		sysop:[], 
		service:[], 
		terminal:{ nodes: nodes, support: [] }, 
		network:[], 
		description:[], 
		total: stats
	};
}

function system_stats()
{
	return {
		users: system.stats.total_users, 
		subs: Object.keys(msg_area.sub).length, 
		dirs: Object.keys(file_area.dir).length, 
		xtrns: Object.keys(xtrn_area.prog).length, 
		storage:disk_size(system.data_dir, 1024*1024)*1024*1024, 
		msgs: system.stats.total_messages, 
		files: system.stats.total_files
	};
}

function add_system(list, bbs, by)
{
	bbs.entry.created = { by: by, on: new Date() };
	list.push(bbs);
}

function update_system(bbs, by)
{
	bbs.entry.updated = { by: by, on: new Date() };
}

/* Leave as last line for convenient load() usage: */
this;
