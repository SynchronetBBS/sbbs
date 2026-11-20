/* Standalone: jsdoor -S -f tests/scoped_execution/run.js */
js.global.exit_code = 1;
if (!(js.options & 4))
	throw new Error("Run this regression with VAROBJFIX enabled");

var initialOptions = js.options;
var fixtureDir = js.exec_dir;
var callbackCount = 0;
function check(condition, message) {
	if (!condition)
		throw new Error(message);
	if (js.options !== initialOptions)
		throw new Error("Context options changed: " + message);
}
function makeScope(value, onExit) {
	var scope = {state: {
		value: value,
		options: initialOptions,
		check: check,
		nestedPath: fixtureDir + "nested.sjs",
		onExit: onExit,
		callback: function() { callbackCount++; }
	}};
	scope.scopeObject = scope;
	return scope;
}
var first = makeScope(17, false);
var second = makeScope(29, true);
var third = makeScope(41, false);
require(third, fixtureDir + "declarations.sjs", "scopeSymbol");
check(third.scopeSymbol() === 41, "require declaration destination");
load(first, fixtureDir + "declarations.sjs");
check(first.scopeSymbol() === 17, "load declaration destination");
require(first, fixtureDir + "missing.sjs", "scopeSymbol");
check(first.scopeValue === 17, "require must reuse an existing symbol");
check(js.exec(fixtureDir + "declarations.sjs", fixtureDir, second) === 29,
	"js.exec result");
check(second.scopeExit === 29 && second.state.exitSeen === 29,
	"on_exit declaration destination");
check(first.scopeSymbol() === 17 && second.scopeSymbol() === 29,
	"independent declaration scopes");
check(!first.hasOwnProperty("scopeExit"), "on_exit must not affect another scope");
check(!js.global.hasOwnProperty("scopeValue"), "scoped variable leaked globally");
check(!js.global.hasOwnProperty("scopeSymbol"), "scoped function leaked globally");
check(!js.global.hasOwnProperty("scopeNested"), "nested require leaked globally");
check(!js.global.hasOwnProperty("scopeEval"), "direct eval leaked globally");
check(!js.global.hasOwnProperty("scopeExit"), "on_exit leaked globally");
try {
	load(first, fixtureDir + "throws.sjs");
	throw new Error("Expected fixture exception");
} catch (error) {
	check(error.message === "expected scoped exception", "load exception propagation");
}
check(js.options === initialOptions, "options after exception");
first.deferredCallback();
js.setImmediate(first.deferredCallback);
js.setTimeout(second.deferredCallback, 1);
js.gc(true);
js.setTimeout(function() {
	check(callbackCount === 3, "deferred callbacks did not complete");
	check(first.scopeSymbol() === 17 && second.scopeSymbol() === 29,
		"callback scopes survived GC");
	js.global.exit_code = 0;
	print("PASS: scoped execution with VAROBJFIX continuously enabled (#1372)");
}, 50);
js.do_callbacks = true;
