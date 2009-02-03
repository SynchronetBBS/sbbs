
function scramble(array)
{
	var i;
	var sw;
	var tmp;

	for(i=array.length-1; i>=0; i--) {
		sw=random(i);
		tmp=array[sw];
		array[sw]=array[i];
		array[i]=tmp;
	}
}
