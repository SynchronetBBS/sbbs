const arg1 = random(256);
// First, test that let and var go into the current global
load("load_define.sjs", arg1);
if (load_var !== arg1)
	throw new Error('Var failed '+load_var+' != '+arg1);
//if (load_let !== arg1)
//	throw new Error('Let failed '+load_let+' != '+arg1);

// Next, test the return value
if (load("load_return.sjs", arg1) !== arg1)
	throw new Error('load() return value bad.');

// Now test what happens when a scope is passed...
// Disabled for 1.8.5
//var scope = {};
//load(scope, "load_define.sjs", arg1);
//if (scope.load_var !== arg1)
//	throw new Error('xVar failed '+scope.load_var+' != '+arg1);

// NOTE: When a scope is passed, it's absolutely entered then left.
// This requires let support, var won't cut it.
//if (scope.load_let === arg1)
//	throw new Error('Let still in scope '+scope.load_let.toSource()+' == '+arg1.toSource());
//if (scope.get_let() !== arg1)
//	throw new Error("Scope closures don't work!");

const arg2 = random(256);
const arg3 = random(256);

// Finally, test background threads...
var q = load(true, "load_background.sjs", arg1, arg2, arg3);
if (q.read(-1) !== arg1)
	throw new Error('First result from queue incorrect');
if (q.read(-1) !== arg2)
	throw new Error('Second result from queue incorrect');
if (q.read(-1) !== arg3)
	throw new Error('Exit result from queue incorrect');
if (q.orphan)
	throw new Error('q orphaned when background terminates');
