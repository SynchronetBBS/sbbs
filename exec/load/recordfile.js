/* TODO: File Locking */

function GetRecordLength(RecordDef)
{
	var i;
	var len=0;

	for(i=0; i<RecordDef.length; i++) {
		switch(RecordDef[i].type) {
			case "Integer":
				len+=4;
				break;
			case "Date":
				len+=8;
				break;
			case "Boolean":
				len+=1;
				break;
			default:
				var m=RecordDef[i].type.match(/^.*:([0-9]+)$/);
				if(m!=null)
					len+=parseInt(m[1]);
				break;
		}
	}
	return(len);
}

function RecordFile(filename, definition)
{
	this.file=new File(filename);
	this.RecordLength=GetRecordLength(this.fields);
	if(!this.file.open(file_exists(this.file.name)?"rb+":"wb+",true,this.RecordLength))
		return(null);
	this.fields=definition;
	this.length=this.file.length/this.RecordLength;

	/* Read a record by index */
	this.Get=RecordFile_Get;
	/* Create a new record */
	this.New=RecordFile_New;
}

function RecordFile_Get(num)
{
	var rec=0;
	var i;
	var ret=new Object();

	if(num==undefined || num < 0 || parseInt(num)!=num)
		return(null);
	num=parseInt(num);

	if(num>=this.length)
		return(null);

	ret.parent=this;
	ret.Put=RecordFileRecord_Put;
	ret.ReLoad=RecordFileRecord_ReLoad;
	ret.ReInit=RecordFileRecord_ReInit;

	ret.Record=num;

	this.file.position=ret.Record * this.RecordLength;
	for(i=0; i<this.fields.length; i++) {
		switch(this.fields[i].type) {
			case "Integer":
				ret[this.fields[i].prop]=this.file.readBin(4);
				break;
			case "Date":
				ret[this.fields[i].prop]=this.file.read(8);
				ret[this.fields[i].prop].replace(/\x00/g,"");
				break;
			case "Boolean":
				var b=this.file.readBin(1);
				if(b)
					ret[this.fields[i].prop]=true;
				else
					ret[this.fields[i].prop]=false;
				break;
			default:
				var m=this.fields[i].type.match(/^String:([0-9]+)$/);
				if(m!=null) {
					ret[this.fields[i].prop]=this.file.read(parseInt(m[1]));
					ret[this.fields[i].prop].replace(/\x00/g,"");
				}
				break;
		}
	}

	return(ret);
}

function RecordFile_New()
{
	var i;
	var ret=new Object();

	for(i=0; i<this.fields.length; i++)
		ret[this.fields[i].prop]=this.fields[i].def;

	ret.parent=this;
	ret.Put=RecordFileRecord_Put;
	ret.ReLoad=RecordFileRecord_ReLoad;
	ret.ReInit=RecordFileRecord_ReInit;
	ret.Record=this.length;
	this.length++;
	ret.Put();
	return(ret);
}

function RecordFileRecord_ReInit()
{
	for(i=0; i<this.parent.fields.length; i++)
		this[this.parent.fields[i].prop]=this.parent.fields[i].def;
}

function RecordFileRecord_Put()
{
	this.parent.file.position=(this.Record)*this.parent.RecordLength;
	for(i=0; i<this.parent.fields.length; i++) {
writeln("Saving "+this.parent.fields[i].prop+" = "+this[this.parent.fields[i].prop]);
		switch(this.parent.fields[i].type) {
			case "Integer":
				this.parent.file.writeBin(this[this.parent.fields[i].prop],4);
				break;
			case "Date":
				var wr=this[this.parent.fields[i].prop].substr(0,8);
				while(wr.length < 8)
					wr=wr+"\x00";
				this.parent.file.write(this[this.parent.fields[i].prop],8);
				break;
			case "Boolean":
				if(this[this.parent.fields[i].prop])
					this.parent.file.writeBin(255,1);
				else
					this.parent.file.writeBin(0,1);
				break;
			default:
				var m=this.parent.fields[i].type.match(/^String:([0-9]+)$/);
				if(m!=null) {
					var len=parseInt(m[1]);
					var wr=this[this.parent.fields[i].prop].substr(0,len);
						while(wr.length < len)
						wr=wr+"\x00";
					this.parent.file.write(this[this.parent.fields[i].prop],len);
				}
				break;
		}
	}
}

function RecordFileRecord_ReLoad(num)
{
	this.parent.file.position=(this.Record)*this.parent.RecordLength;
	for(i=0; i<this.parent.fields.length; i++) {
		switch(this.parent.fields[i].type) {
			case "Integer":
				this[this.parent.fields[i].prop]=this.parent.file.readBin(4);
				break;
			case "Date":
				this[this.parent.fields[i].prop]=this.parent.file.read(8);
				this[this.parent.fields[i].prop].replace(/\x00/g,"");
				break;
			case "Boolean":
				var b=this.parent.file.readBin(1);
				if(b)
					this[this.parent.fields[i].prop]=true;
				else
					this[this.parent.fields[i].prop]=false;
				break;
			default:
				var m=this.parent.fields[i].type.match(/^String:([0-9]+)$/);
				if(m!=null) {
					this[this.parent.fields[i].prop]=this.parent.file.read(parseInt(m[1]));
					this[this.parent.fields[i].prop].replace(/\x00/g,"");
				}
				break;
		}
	}

	return(ret);
}
