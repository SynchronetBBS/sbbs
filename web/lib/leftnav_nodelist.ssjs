/* $Id: leftnav_nodelist.ssjs,v 1.4 2006/01/31 23:19:29 runemaster Exp $ */

var start=new Date();

load("nodedefs.js");

var include_statistics=true;
var who_online=false;

template.title=system.name+" Home Page";

template.node_list = [];
var u = new User(0);
var n;
for(n=0;n<system.node_list.length;n++) {
    if(system.node_list[n].status==NODE_INUSE) {
        u.number=system.node_list[n].useron;
        if(system.node_list[n].action==NODE_XTRN && system.node_list[n].aux && xtrn_area.prog[u.curxtrn])
            action=format("running %s",xtrn_area.prog[u.curxtrn].name);
        else
            action=format(NodeAction[system.node_list[n].action]
                ,system.node_list[n].aux);
        template.node_list.push({ name: u.alias, email: u.email.replace(/@/g,"&#64;"), action: action});
    }
}
