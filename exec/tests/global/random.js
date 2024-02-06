random();
if (random() == random() == random() == random())
	throw new Error("Four random() calls returned same result");
for (var i = 0; i < 1000; i++) {
	var rval = random(10);
	if (rval < 0 || rval >= 10)
		throw new Error("random() result out of range");
}
