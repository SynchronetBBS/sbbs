function GetRecordLength(RecordDef)
{
	var i;
	var len=0;

	function GetTypeLength(fieldtype) {
		switch(fieldtype) {
			case "Float":
				return(22);
			case "SignedInteger":
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
	this.__defineGetter__("length", function() {return parseInt(this.file.length/this.RecordLength);});
	this.locks = [];
}

function RecordFileRecord(parent, num)
{
	this.parent=parent;
	this.Record=num;
}

RecordFileRecord.prototype.ReLoad = function()
{
	var i;

	this.parent.file.position=(this.Record)*this.parent.RecordLength;
	for(i=0; i<this.parent.fields.length; i++)
		this[this.parent.fields[i].prop]=this.parent.ReadField(this.parent.fields[i].type);
}

RecordFile.prototype.Lock = function(rec, timeout)
{
	var i;
	var end = new Date();
	var now;
	var ret = false;

	if (rec === undefined)
		return false;
	if (this.locks.indexOf(rec) != -1)
		return false;

	if (timeout === undefined)
		timeout = 1;
	end += timeout*1000;

	do {
		ret = this.file.lock(rec*this.RecordLength, this.RecordLength);
	} while (ret === false && new Date() < end);

	if (ret)
		this.locks.push(rec);

	return ret;
}

RecordFile.prototype.UnLock = function(rec)
{
	var ret;
	var lck;

	if (rec === undefined)
		return false;

	if ((lck = this.locks.indexOf(rec)) == -1)
		return false;

	ret = this.file.unlock(rec * this.RecordLength, this.RecordLength);

	this.locks.splice(lck, 1);

	return ret;
}

RecordFileRecord.prototype.UnLock = function()
{
	return this.parent.UnLock(this.Record);
}

RecordFileRecord.prototype.Lock = function(timeout)
{
	return this.parent.Lock(this.Record, timeout);
}

RecordFileRecord.prototype.Put = function()
{
	var i;

	this.parent.file.position=this.Record * this.parent.RecordLength;
	for(i=0; i<this.parent.fields.length; i++)
		this.parent.WriteField(this[this.parent.fields[i].prop], this.parent.fields[i].type, this.parent.fields[i].def);
}

RecordFileRecord.prototype.ReInit = function()
{
	var i;

	for(i=0; i<this.parent.fields.length; i++)
		this[this.parent.fields[i].prop]=eval(this.parent.fields[i].def.toSource());
}

RecordFile.prototype.Get = function(num)
{
	var rec=0;
	var i;

	if(num==undefined || num < 0 || parseInt(num)!=num)
		return(null);
	num=parseInt(num);

	if(num>=this.length)
		return(null);

	var ret = new RecordFileRecord(this, num);

	this.file.position=ret.Record * this.RecordLength;
	for(i=0; i<this.fields.length; i++)
		ret[this.fields[i].prop]=this.ReadField(this.fields[i].type);

	return(ret);
}

RecordFile.prototype.New = function(timeout)
{
	var i;
	var ret;

	if (!this.Lock(this.length, timeout))
		return undefined;

	ret = new RecordFileRecord(this, this.length);

	for(i=0; i<this.fields.length; i++)
		ret[this.fields[i].prop]=eval(this.fields[i].def.toSource());

	ret.Put();
	ret.UnLock();

	return(ret);
}

RecordFile.prototype.ReadField = function(fieldtype)
{
	var i;
	var m=fieldtype.match(/^Array:([0-9]+):(.*)$/);
	var ret;
	var tmp;

	if(m!=null) {
		ret=new Array();
		for(i=0; i<parseInt(m[1]); i++)
			ret.push(this.ReadField(m[2]));
		return(ret);
	}
	else {
		switch(fieldtype) {
			case "Float":
				tmp=this.file.read(22);
				return(parseFloat(tmp));
			case "SignedInteger":
				ret=this.file.readBin(4);
				if(ret>=2147483648)
					ret-=4294967296;
				return(ret);
			case "Integer":
				return(this.file.readBin(4));
			case "Date":
				tmp=this.file.read(8);
				return(tmp.replace(/\x00/g,""));
			case "Boolean":
				if(this.file.readBin(1) > 0)
					return(true);
				return(false);
			default:
				m=fieldtype.match(/^String:([0-9]+)$/);
				if(m!=null) {
					tmp=this.file.read(parseInt(m[1]));
					return(tmp.replace(/\x00/g,""));
				}
				return(null);
		}
	}
}

RecordFile.prototype.WriteField = function(val, fieldtype, def)
{
	var i;
	var m=fieldtype.match(/^Array:([0-9]+):(.*)$/);
	var wr;
	var len;
	var ret;

	if(m!=null) {
		ret=new Array();
		for(i=0; i<parseInt(m[1]); i++) {
			this.WriteField(val[i], m[2], def[i]);
		}
		return(ret);
	}
	else {
		if(val==undefined)
			val=def;
		switch(fieldtype) {
			case "Float":
				wr=val.toExponential(15);
				while(wr.length < 22)
					wr=wr+"\x00";
				this.file.write(wr,22);
				break;
			case "SignedInteger":
				if(val < -2147483648)
					val = -2147483648;
				if(val > 2147483647)
					val = 2147483647;
				this.file.writeBin(val,4);
				break;
			case "Integer":
				if(val<0)
					val=0;
				if(val>4294967295)
					val=4294967295;
				this.file.writeBin(val,4);
				break;
			case "Date":
				wr=val.substr(0,8);
				while(wr.length < 8)
					wr=wr+"\x00";
				this.file.write(wr,8);
				break;
			case "Boolean":
				if(val.valueOf())
					this.file.writeBin(255,1);
				else
					this.file.writeBin(0,1);
				break;
			default:
				m=fieldtype.match(/^String:([0-9]+)$/);
				if(m!=null) {
					len=parseInt(m[1]);
					wr=val.substr(0,len);
					while(wr.length < len)
						wr=wr+"\x00";
					this.file.write(wr,len);
				}
				break;
		}
	}
}

