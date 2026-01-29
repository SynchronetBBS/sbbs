// Display the contents of a viewable (e.g. plain text) file

"use strict";

require("sbbsdefs.js", "P_CPM_EOF");

console.printfile(argv[0], P_CPM_EOF | P_SEEK);

