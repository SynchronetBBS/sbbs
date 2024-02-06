var x = random(256);
var scope = new function(){};
var ret = js.exec("exit_subproc.sjs", scope, x);
if (ret != x) {
	print('ret: '+ret.toSource());
	print('x: '+x.toSource());
	throw new Error("ret is wrong");
}
