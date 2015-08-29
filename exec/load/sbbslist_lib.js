// $Id$

// Synchronet BBS List (SBL) v4 Library

load("synchronet-json.js"); // For compatibility with older verisons of Synchronet/JavaScript-C without the JSON object

var list_fname = system.data_dir + 'sbbslist.json';

function read_list()
{
    var f = new File(list_fname);
    print("Opening list file: " + f.name);
    if(!f.open("r")) {
        alert("Error " + f.error + " opening " + f.name);
        return null;
    }

    var buf = f.readAll().join(' ');
    var list = [];
    try {
        list=JSON.parse(buf);
    } catch(e) {
        print("JSON.parse exception: " + e);
    }

    f.close();

    return list;
}

function write_list(list)
{
    var out = new File(list_fname);
    print("Opening / creating list file: " + list_fname);
    if(!out.open("w+")) {
        alert("Error " + out.error + " creating " + out.name);
        return false;
    }

    print("Writing list file: " + out.name + " (" + list.length + " BBS entries)");
    out.write(JSON.stringify(list, null, 4));
    out.close();
    return true;
}

