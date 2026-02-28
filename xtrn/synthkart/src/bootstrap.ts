/**
 * Bootstrap - Load Synchronet definitions before anything else.
 * This file must be concatenated FIRST in the build.
 */

// Load Synchronet standard definitions (color constants, etc.)
load('sbbsdefs.js');

// Type declarations for Synchronet global objects
declare var user: any;  // User object with alias, name, etc.

// Load key definitions for arrow keys, escape, etc.
// These define KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_ESC, etc.
require("key_defs.js", "KEY_UP");
require("key_defs.js", "KEY_DOWN");
require("key_defs.js", "KEY_LEFT");
require("key_defs.js", "KEY_RIGHT");
require("key_defs.js", "KEY_ESC");
require("key_defs.js", "KEY_HOME");
require("key_defs.js", "KEY_END");
