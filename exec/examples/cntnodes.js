// cntnodes.js

// Counts number of nodes in use and number of nodes waiting for call
// Sets variables NODES_INUSE and NODES_WFC
// Usage: 	load("cntnodes.js");
// 		cntnodes();

// $Id: cntnodes.js,v 1.3 2005/09/12 19:29:44 deuce Exp $

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
		switch(system.node_list[i].status) {
			case NODE_WFC:
				NODES_WFC++;
				break;
			case NODE_LOGON:
			case NODE_NEWUSER:
			case NODE_INUSE:
			case NODE_QUIET:
				NODES_INUSE++;
				break;
		}
	}
	writeln("Nodes in use="+NODES_INUSE+"  Waiting for Caller="+NODES_WFC);
}

