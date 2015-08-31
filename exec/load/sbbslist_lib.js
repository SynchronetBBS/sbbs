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

    var buf = f.readAll(8192).join(' ');
    var list = [];
    try {
        list=JSON.parse(buf);
    } catch(e) {
        log(LOG_ERR, "SBBSLIST: JSON.parse exception: " + e);
    }

    f.close();

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

/* Leave as last line for convenient load() usage: */
this;