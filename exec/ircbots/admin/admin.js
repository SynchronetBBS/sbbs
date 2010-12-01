var tail_history=[];
var last_update=0;
var update_interval=2;

function main(srv,target)
{	
	if((time() - last_update) < update_interval) return;
	
	last_update=time();
	for each(var h in tail_history) {
		if(file_date(h.file.name) > h.last_update) {
			h.file.open('r',true);
			var contents=h.file.readAll();
			while(h.index < contents.length) {
				srv.o(h.target,contents[h.index]);
				h.index++;
			}
			
			h.file.close();
			h.last_update=file_date(h.file.name);
		}
	}
	return;
}

function History(filename,target) 
{
	this.target=target;
	this.last_update=0;
	this.file=new File(filename);
	this.file.open('r',true);
	this.index=this.file.readAll().length;
	this.file.close();
}
