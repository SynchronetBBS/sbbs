// $Id$

// Example Node Listing (display) script

load("nodedefs.js");

function xtrn_name(code)
{
	if(this.xtrn_area==undefined)
		return(code);

	if(xtrn_area.prog!=undefined)
		if(xtrn_area.prog[code]!=undefined)
			return(xtrn_area.prog[code].name);
	else {	/* old way */
		for(s in xtrn_area.sec_list)
			for(p in xtrn_area.sec_list[s].prog_list)
				if(xtrn_area.sec_list[s].prog_list[p].code.toLowerCase()==code.toLowerCase())
					return(xtrn_area.sec_list[s].prog_list[p].name);
	}
	return(code);
}

var u = new User(1);
for(n=0;n<system.node_list.length;n++) {
	printf("Node %2d ",n+1);
	if(system.node_list[n].status==NODE_INUSE) {
		u.number=system.node_list[n].useron;
		printf("%s (%u %s) ", u.alias, u.age, u.gender);
		if(system.node_list[n].action==NODE_XTRN && system.node_list[n].aux)
			print(format("running %s",xtrn_name(u.curxtrn)));
		else
			print(format(NodeAction[system.node_list[n].action],system.node_list[n].aux));

	} else
		print(format(NodeStatus[system.node_list[n].status],system.node_list[n].aux));
}
