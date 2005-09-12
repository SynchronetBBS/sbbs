// cntnodes.js

// Counts number of nodes in use and number of nodes waiting for call
// Sets variables NODES_INUSE and NODES_WFC
// Usage: 	load("cntnodes.js");
// 		cntnodes();

// $Id$

// @format.tab-size 4, @format.use-tabs true

load("nodedefs.js");

var NODES_INUSE=0;
var NODES_WFC=0;

function cntnodes()
{
	var i;

	nodes_inuse=0
	nodes_wfc=0

	for(i=0; i<system.nodes; i++) {
		if(system.node_list[i].status & NODE_WFC)
			nodes_wfc++;
		if(system.node_list[i].status & NODE_INUSE)
			nodes_inuse++;
	}
	writeln("Nodes in use="+nodes_inuse+"  Waiting for Caller="+nodes_wfc);
}

