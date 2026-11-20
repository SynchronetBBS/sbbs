state.check(js.options === state.options, "options inside scoped script");
var scopeValue = state.value;
function scopeSymbol() { return scopeValue; }
eval("var scopeEval = scopeValue + 1;");
require(state.nestedPath, "scopeNested");
state.check(scopeNested() === scopeValue + 2, "implicit nested require");
require("missing.sjs", "scopeNested");
function deferredCallback() {
	state.check(js.options === state.options, "options inside callback");
	state.check(scopeSymbol() === state.value && scopeEval === state.value + 1,
		"callback declaration scope");
	require(scopeObject, "missing.sjs", "scopeNested");
	state.check(scopeObject.scopeNested() === state.value + 2, "callback require");
	state.callback();
}
if (state.onExit)
	js.on_exit("var scopeExit = scopeSymbol(); state.exitSeen = scopeExit;");
scopeSymbol();
