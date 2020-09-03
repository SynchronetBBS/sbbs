/* $Id: main_nodelist.ssjs,v 1.5 2006/01/31 23:19:29 runemaster Exp $ */

/*  Do not change anything else unless you need to :) */

var start=new Date();
var age='';
var gender='';
var location='';


load("nodedefs.js");

template.title=system.name+" Who's Online";

template.who_online = [];
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
       if(show_gender) {
            if(u.gender=="F")
                gender="Female";
            else
           gender="Male";
       }
       t=time()-u.logontime;
       if(t&0x80000000) t=0;
       timeon=format("%u:%02u:%02u" ,Math.floor(t/(60*60)) ,Math.floor(t/60)%60 ,t%60);
       template.who_online.push({ node: n+1, name: u.alias, email: u.email.replace(/@/g,"&#64;"), action: action, location: u.location, age: u.age, gender: gender, timeon: timeon });
    }
    else {
        action=format(NodeStatus[system.node_list[n].status],system.node_list[n].aux);
        template.who_online.push({ node: n+1, action: action });
    }
}

 
