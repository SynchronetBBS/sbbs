if(js.global.scramble==undefined)
	load(script_dir+'utilfuncs.js');

function Order()
{
	this.unit=null;
	this.qty=0;
	this.command=0;
	this.args=new Array();
	this.suborders=new Array();
}

function expandorders(r,orders)
{
	var i,o,prop,neworder,neworders,u;

	for (u in r.units)
		r.units[u].n = -1;

	neworders=new Array();

	for (o in orders) {
		for(i=0; i<orders[o].qty; i++) {
			neworder=new Object();

			neworder.unit=orders[o].unit;
			neworder.unit.n=0;
			for(prop in orders[o]) {
				if(prop != 'qty')
					neworder[prop]=orders[o][prop];
			}
			neworders.push(neworder);
		}
	}

	scramble(neworders);

	orders.splice(0,orders.length);
	while(neworders.length)
		orders.push(neworders.shift());
}
