
var start=new Date();

load("nodedefs.js");

var include_statistics=true;
var who_online=false;

template.title=system.name+" Home Page";

function xtrn_name(code)
{
    if(this.xtrn_area==undefined)
        return(code);

    for(s in xtrn_area.sec_list)
        for(p in xtrn_area.sec_list[s].prog_list)
            if(xtrn_area.sec_list[s].prog_list[p].code.toLowerCase()==code.toLowerCase())
                return(xtrn_area.sec_list[s].prog_list[p].name);
    return(code);
}

for(n=0;n<system.node_list.length;n++) {
    if(system.node_list[n].status==NODE_INUSE) {
            template.show_nodelist=true;
    }
}

var u = new User(0);
    for(n=0;n<system.node_list.length;n++) {
    if(system.node_list[n].status==NODE_INUSE) {    
    if(system.node_list[n].status==NODE_INUSE) {
        u.number=system.node_list[n].useron;
        if(system.node_list[n].action==NODE_XTRN && system.node_list[n].aux)
            action=format("running %s",xtrn_name(u.curxtrn));
        else
            action=format(NodeAction[system.node_list[n].action]
                ,system.node_list[n].aux);
        if(system.node_list[n].status==NODE_INUSE && system.node_list[n].useron) {
        template.user_email=u.email;
        template.user_name=u.alias;
        }
        if(system.node_list[n].status==NODE_INUSE && system.node_list[n].useron) {
            template.node_action=action;
        }
     }
   }
}
