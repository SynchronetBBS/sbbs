load("recordfile.js");
var Struct = [
				{
				 prop:"Check"
				,type:"Boolean"
				,def:false
				}
	];
var rf=new RecordFile("../xtrn/tw2/rectest.dat",Struct);

if(rf.length==0)
	r=rf.New();
else
	r=rf.Get(0);
writeln("At start, Check=="+r.Check);
r.Put();
writeln("After Put, Check=="+r.Check);
r.ReLoad(0);
writeln("Ad end, Check=="+r.Check);
