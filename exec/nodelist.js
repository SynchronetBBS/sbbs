// nodelist.js

load("nodedefs.js");

for(n=0;n<system.node_list.length;n++) {
	printf("Node %2d ",n+1);
	if(system.node_list[n].status==NODE_INUSE) {
		printf("%s ",system.node_list[n].useron_name);
		printf(NodeAction[system.node_list[n].action],system.node_list[n].aux);
	} else
		printf(NodeStatus[system.node_list[n].status],system.node_list[n].aux);

	printf("\r\n");
}