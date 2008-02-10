function GetRecordLength(RecordDef)
{
	var i;
	var len=0;

	function GetTypeLength(fieldtype) {
		switch(RecordDef[i].fieldtype) {
			case "Integer":
				return(4);
			case "Date":
				return(8);
			case "Boolean":
				return(1);
			default:
				var m=fieldtype.match(/^String:([0-9]+)$/);
				if(m!=null)
					return(parseInt(m[1]));
				return(0);
		}
	}

	for(i=0; i<RecordDef.length; i++) {
		var m=RecordDef[i].type.match(/^Array:([0-9]+):(.*)$/);
		if(m!=null)
			len += GetTypeLength(m[2])*parseInt(m[1]);
		else
			len += GetTypeLength(RecordDef[i].type);
	}
	return(len);
}

function RecordFile(filename, definition)
{
	this.file=new File(filename);
	this.fields=definition;
	this.RecordLength=GetRecordLength(this.fields);
	if(!this.file.open(file_exists(this.file.name)?"rb+":"wb+",true,this.RecordLength))
		return(null);
	this.length=this.file.length/this.RecordLength;

	/* Read a record by index */
	this.Get=RecordFile_Get;
	/* Create a new record */
	this.New=RecordFile_New;
	/* Read a single field from the current position */
	this.ReadField=RecordFile_ReadField;
	/* Write a single field from the current position */
	this.WriteField=RecordFile_WriteField;
}

function RecordFile_ReadField(fieldtype)
{
	var i;
	var m=fieldtype.match(/^Array:([0-9]+):(.*)$/);

	if(m!=null) {
		var ret=new Array();
		for(i=0; i<parseInt(m[1]); i++)
			ret.push(this.ReadField(m[2]));
		return(ret);
	}
	else {
		switch(fieldtype) {
			case "Integer":
				return(this.file.readBin(4));
			case "Date":
				var tmp=this.file.read(8);
				return(tmp.replace(/\x00/g,""));
			case "Boolean":
				if(file.readBin(1))
					return(true);
				return(false);
			default:
				var m=fieldtype.match(/^String:([0-9]+)$/);
				if(m!=null) {
					var tmp=this.file.read(parseInt(m[1]));
					return(tmp.replace(/\x00/g,""));
				}
				return(null);
		}
	}
}

function RecordFile_WriteField(val, fieldtype)
{
	var i;
	var m=fieldtype.match(/^Array:([0-9]+):(.*)$/);

	if(m!=null) {
		var ret=new Array();
		for(i=0; i<parseInt(m[1]); i++)
			this.WriteField(val[i], m[2]);
		return(ret);
	}
	else {
		switch(fieldtype) {
			case "Integer":
				this.file.writeBin(val,4);
				break;
			case "Date":
				var wr=val.substr(0,8);
				while(wr.length < 8)
					wr=wr+"\x00";
				this.file.write(wr,8);
				break;
			case "Boolean":
				if(val)
					this.file.writeBin(255,1);
				else
					this.file.writeBin(0,1);
				break;
			default:
				var m=fieldtype.match(/^String:([0-9]+)$/);
				if(m!=null) {
					var len=parseInt(m[1]);
					var wr=val.substr(0,len);
					while(wr.length < len)
						wr=wr+"\x00";
					this.file.write(wr,len);
				}
				break;
		}
	}
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
	for(i=0; i<this.fields.length; i++)
		ret[this.fields[i].prop]=this.ReadField(this.fields[i].type);

	return(ret);
}

function RecordFile_New()
{
	var i;
	var ret=new Object();

	for(i=0; i<this.fields.length; i++)
		ret[this.fields[i].prop]=eval(this.fields[i].def.toSource());

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
		this[this.parent.fields[i].prop]=eval(this.parent.fields[i].def.toSource);
}

function RecordFileRecord_Put()
{
	this.parent.file.position=(this.Record)*this.parent.RecordLength;
	for(i=0; i<this.parent.fields.length; i++)
		this.parent.WriteField(this[this.parent.fields[i].prop], this.parent.fields[i].type);
}

function RecordFileRecord_ReLoad(num)
{
	this.parent.file.position=(this.Record)*this.parent.RecordLength;
	for(i=0; i<this.parent.fields.length; i++)
		this[this.parent.fields[i].prop]=this.parent.ReadField(this.parent.fields[i].type);
}
