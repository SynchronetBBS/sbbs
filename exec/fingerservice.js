// fingerservice.js

load("nodedefs.js");

function write(str)
{
	client.socket.send(str);
}

var user = new User(1);
for(n=0;n<system.node_list.length;n++) {
	write(format("Node %2d ",n+1));
	if(system.node_list[n].status==NODE_INUSE) {
		user.number=system.node_list[n].useron;
		write(format("%s (%u %s) ", user.alias, user.age, user.gender));
		write(format(NodeAction[system.node_list[n].action],system.node_list[n].aux));
	} else
		write(format(NodeStatus[system.node_list[n].status],system.node_list[n].aux));

	write("\r\n");
}