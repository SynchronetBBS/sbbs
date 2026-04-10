// Test that user object properties survive enumeration (JSON.stringify, for..in)
// This catches non-configurable property redefinition errors during resolve/enumerate.

var u = new User;

// First enumeration via JSON.stringify
var json = JSON.stringify(u);
if (typeof json !== "string" || json.length < 2)
	throw new Error("JSON.stringify(user) failed: " + json);

// Second enumeration on the same object (triggers re-resolve)
var json2 = JSON.stringify(u);
if (json2 !== json)
	throw new Error("Second JSON.stringify(user) produced different result");

// for..in enumeration
var props = [];
for (var p in u)
	props.push(p);
if (props.length < 10)
	throw new Error("for..in enumeration returned only " + props.length + " properties");

// Verify key properties are present and accessible after enumeration
var required = ["alias", "name", "birthdate", "number", "security", "stats", "limits"];
for (var i = 0; i < required.length; i++) {
	if (u[required[i]] === undefined)
		throw new Error("user." + required[i] + " is undefined after enumeration");
}

// Verify birthdate is still writable after enumeration
var saved = u.birthdate;
u.birthdate = "01/01/2000";
if (u.birthdate !== "20000101")
	throw new Error("birthdate not writable after enumeration: " + u.birthdate);
u.birthdate = saved;
