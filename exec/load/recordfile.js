function getRecordLength(RecordDef)
{
	'use strict';
	var i;
	var len=0;
	var m;

	function getTypeLength(fieldtype) {
		switch(fieldtype) {
			case "Float":
				return(22);
			case "SignedInteger8":
			case "Integer8":
				return(1);
			case "SignedInteger16":
			case "Integer16":
				return(2);
			case "SignedInteger":
			case "Integer":
				return(4);
			case "Date":
				return(8);
			case "Boolean":
				return(1);
			default:
				m=fieldtype.match(/^String:([0-9]+)$/);
				if(m !== null) {
					return(parseInt(m[1], 10));
				}
				m=fieldtype.match(/^PString:([0-9]+)$/);
				if(m !== null) {
					return(parseInt(m[1], 10)+1);
				}
				return(0);
		}
	}

	for(i=0; i<RecordDef.length; i += 1) {
		m=RecordDef[i].type.match(/^Array:([0-9]+):(.*)$/);
		if(m !== null) {
			len += getTypeLength(m[2])*parseInt(m[1], 10);
		}
		else {
			len += getTypeLength(RecordDef[i].type);
		}
	}
	return(len);
}
function GetRecordLength(RecordDef)
{
	'use strict';
	return getRecordLength(RecordDef);
}

function RecordFile(filename, definition)
{
	'use strict';
	this.file=new File(filename);
	this.fields=definition;
	this.RecordLength=getRecordLength(this.fields);
	// A vbuf of a record length prevents more than one record from being in the buffer.
	if(!this.file.open(file_exists(this.file.name)?"rb+":"wb+",true,this.RecordLength)) {
		return(null);
	}
	Object.defineProperty(this, 'length', {
		enumerable: true,
		get: function() {
			return parseInt(this.file.length/this.RecordLength, 10);
		}
	});
	this.locks = [];
}

function RecordFileRecord(parent, num)
{
	'use strict';
	this.parent=parent;
	this.Record=num;
}

RecordFileRecord.prototype.flushRead = function(keeplocked)
{
	'use strict';
	var i;
	var locked;
	var flushed = false;
	var tmp;

	locked = (this.parent.locks.indexOf(this.Record) !== -1);
	if (keeplocked === undefined) {
		if (locked) {
			keeplocked = true;
		}
		else {
			keeplocked = false;
		}
	}

	// If there's only one record, the only way to flush the buffer is
	// to close and re-open...
	if (this.parent.length === 1) {
		// Which means we need to give up the lock...
		if (locked) {
			this.unLock();
		}
		this.parent.file.close();
		if (!this.parent.file.open('rb+', true, this.parent.RecordLength)) {
			throw('Unable to re-open '+this.parent.file.name);
		}
		if (locked) {
			this.lock();
		}
		flushed = true;
	}

	// Try to force a read cache flush by reading a different record...
	// First, try to read one we have a lock on already...
	if (!flushed) {
		for (i = 0; i < this.parent.locks.length; i += 1) {
			tmp = this.parent.locks[i];
			if (tmp !== this.Record) {
				this.parent.file.position = tmp * this.parent.RecordLength;
				this.parent.file.read(this.parent.RecordLength);
				flushed = true;
				return;
			}
		}
	}

	// Otherwise, use the first one we can get an immediate lock on...
	if (!flushed) {
		if (locked) {
			this.unLock();
		}
		for (i = 0; i < this.parent.length; i += 1) {
			if (i !== this.Record) {
				if (this.parent.lock(i, 0)) {
					this.parent.file.position = i * this.parent.RecordLength;
					this.parent.file.read(this.parent.RecordLength);
					this.parent.unLock(i);
					flushed = true;
					break;
				}
			}
		}
		if (locked) {
			this.lock();
		}
	}
	// Finally, just wait until we can read some random record...
	if (!flushed) {
		i = random(this.parent.length - 1);
		if (i >= this.Record) {
			i += 1;
		}
		if (locked) {
			this.unLock();
		}
		while(!this.parent.lock(i))
			{}
		this.parent.file.position = i * this.parent.RecordLength;
		this.parent.file.read(this.parent.RecordLength);
		this.parent.unLock(i);
		flushed = true;
		if (locked) {
			this.lock();
		}
	}

	if (keeplocked) {
		if (locked) {
			return;
		}
		this.lock();
	}
	else {
		if (locked) {
			this.unLock();
		}
	}
};
RecordFileRecord.prototype.FlushRead = function(keeplocked)
{
	'use strict';
	return this.flushRead(keeplocked);
};

RecordFileRecord.prototype.reLoad = function(keeplocked)
{
	'use strict';
	var i;
	var lock;

	lock = (this.parent.locks.indexOf(this.Record) === -1);
	if (keeplocked === undefined) {
		if (lock) {
			keeplocked = false;
		}
		else {
			keeplocked = true;
		}
	}

	this.flushRead(!lock);

	// Locks don't work because threads hate them. :(
	this.parent.file.position=(this.Record)*this.parent.RecordLength;
	if (lock) {
		while(!this.lock())
			{}		// Forever
	}

	for(i=0; i<this.parent.fields.length; i += 1) {
		this[this.parent.fields[i].prop]=this.parent.readField(this.parent.fields[i].type);
	}

	if (!keeplocked) {
		this.unLock();
	}
};
RecordFileRecord.prototype.ReLoad = function(keeplocked)
{
	'use strict';
	return this.reLoad(keeplocked);
};

RecordFile.prototype.lock = function(rec, timeout)
{
	'use strict';
	var end = new Date();
	var ret = false;

	if (rec === undefined) {
		return false;
	}
	if (this.locks.indexOf(rec) !== -1) {
		return false;
	}

	if (timeout === undefined) {
		timeout = 1;
	}
	end.setTime(end.getTime() + timeout*1000);

	do {
		ret = this.file.lock(rec*this.RecordLength, this.RecordLength);
		if (ret === false && timeout > 0) {
			mswait(1);
		}
	} while (ret === false && new Date() < end);

	if (ret) {
		this.locks.push(rec);
	}

	return ret;
};
RecordFile.prototype.Lock = function(rec, timeout)
{
	'use strict';
	return this.lock(rec, timeout);
};

RecordFile.prototype.unLock = function(rec)
{
	'use strict';
	var ret;
	var lck;

	if (rec === undefined) {
		return false;
	}

	lck = this.locks.indexOf(rec);
	if (lck === -1) {
		return false;
	}

	ret = this.file.unlock(rec * this.RecordLength, this.RecordLength);

	this.locks.splice(lck, 1);

	return ret;
};
RecordFile.prototype.UnLock = function(rec)
{
	'use strict';
	return this.unLock(rec);
};

RecordFileRecord.prototype.unLock = function()
{
	'use strict';
	return this.parent.unLock(this.Record);
};
RecordFileRecord.prototype.UnLock = function()
{
	'use strict';
	return this.unLock();
};

RecordFileRecord.prototype.lock = function(timeout)
{
	'use strict';
	return this.parent.lock(this.Record, timeout);
};
RecordFileRecord.prototype.Lock = function(timeout)
{
	'use strict';
	return this.lock(timeout);
};

RecordFileRecord.prototype.put = function(keeplocked)
{
	'use strict';
	var i;
	var lock;

	lock = (this.parent.locks.indexOf(this.Record) === -1);
	if (keeplocked === undefined) {
		if (lock) {
			keeplocked = false;
		}
		else {
			keeplocked = true;
		}
	}

	this.parent.file.position=this.Record * this.parent.RecordLength;

	if (lock) {
		while(!this.lock())
			{}		// Forever
	}

	for(i=0; i<this.parent.fields.length; i += 1) {
		this.parent.writeField(this[this.parent.fields[i].prop], this.parent.fields[i].type, eval(this.parent.fields[i].def.toSource()).valueOf());
	}
	this.parent.file.flush();

	if (!keeplocked) {
		this.unLock();
	}
};
RecordFileRecord.prototype.Put = function(keeplocked)
{
	'use strict';
	return this.put(keeplocked);
};

RecordFileRecord.prototype.reInit = function()
{
	'use strict';
	var i;

	for(i=0; i<this.parent.fields.length; i += 1) {
		this[this.parent.fields[i].prop]=eval(this.parent.fields[i].def.toSource()).valueOf();
	}
};
RecordFileRecord.prototype.ReInit = function()
{
	'use strict';
	return this.reInit();
};

RecordFile.prototype.get = function(num, keeplocked)
{
	'use strict';
	var lock;
	var i;
	var ret;

	if(num === undefined || num < 0 || parseInt(num, 10) !== num) {
		return(null);
	}
	num = parseInt(num, 10);

	if(num>=this.length) {
		return(null);
	}

	lock = (this.locks.indexOf(num) === -1);
	if (keeplocked === undefined) {
		if (lock) {
			keeplocked = false;
		}
		else {
			keeplocked = true;
		}
	}

	ret = new RecordFileRecord(this, num);

	ret.flushRead(!lock);

	this.file.position=ret.Record * this.RecordLength;

	if (lock) {
		while(!ret.lock())
			{}		// Forever
	}

	for(i=0; i<this.fields.length; i += 1) {
		ret[this.fields[i].prop]=this.readField(this.fields[i].type);
	}

	if (!keeplocked) {
		ret.unLock();
	}

	return(ret);
};
RecordFile.prototype.Get = function(num, keeplocked)
{
	'use strict';
	return this.get(num, keeplocked);
};

RecordFile.prototype.new = function(timeout, keeplocked)
{
	'use strict';
	var i;
	var ret;
	var lock;

	lock = (this.locks.indexOf(this.Record) === -1);
	if (keeplocked === undefined) {
		if (lock) {
			keeplocked = false;
		}
		else {
			keeplocked = true;
		}
	}


	ret = new RecordFileRecord(this, this.length);

	if (lock) {
		if (!ret.lock(timeout)) {
			return undefined;
		}
	}

	for(i=0; i<this.fields.length; i += 1) {
		ret[this.fields[i].prop]=eval(this.fields[i].def.toSource()).valueOf();
	}

	ret.put();
	if (!keeplocked) {
		ret.unLock();
	}

	return(ret);
};
RecordFile.prototype.New = function(timeout, keeplocked)
{
	'use strict';
	return this.new(timeout, keeplocked);
};

RecordFile.prototype.readField = function(fieldtype)
{
	'use strict';
	var i;
	var m=fieldtype.match(/^Array:([0-9]+):(.*)$/);
	var ret;
	var tmp;
	var tmp2;

	if(m !== null) {
		ret = [];
		for(i=0; i<parseInt(m[1], 10); i += 1) {
			ret.push(this.readField(m[2]));
		}
		return(ret);
	}
	else {
		switch(fieldtype) {
			case "Float":
				tmp=this.file.read(22);
				return(parseFloat(tmp));
			case "SignedInteger":
				ret=this.file.readBin(4);
				if(ret>=2147483648) {
					ret-=4294967296;
				}
				return(ret);
			case "Integer":
				return(this.file.readBin(4));
			case "SignedInteger16":
				ret=this.file.readBin(2);
				if(ret>=32768) {
					ret-=65536;
				}
				return(ret);
			case "Integer16":
				return(this.file.readBin(2));
			case "SignedInteger8":
				ret=this.file.readBin(1);
				if(ret>=127) {
					ret-=256;
				}
				return(ret);
			case "Integer8":
				return(this.file.readBin(1));
			case "Date":
				tmp=this.file.read(8);
				return(tmp.replace(/\x00/g,""));
			case "Boolean":
				if(this.file.readBin(1) > 0) {
					return(true);
				}
				return(false);
			default:
				m=fieldtype.match(/^String:([0-9]+)$/);
				if(m !== null) {
					tmp=this.file.read(parseInt(m[1], 10));
					return(tmp.replace(/\x00/g,""));
				}
				m=fieldtype.match(/^PString:([0-9]+)$/);
				if(m !== null) {
					tmp=this.file.readBin(1);
					tmp2=this.file.read(parseInt(m[1], 10));
					return(tmp2.substr(0, tmp));
				}
				return(null);
		}
	}
};
RecordFile.prototype.ReadField = function(fieldtype)
{
	'use strict';
	return this.readField(fieldtype);
};

RecordFile.prototype.writeField = function(val, fieldtype, def)
{
	'use strict';
	var i;
	var m=fieldtype.match(/^Array:([0-9]+):(.*)$/);
	var wr;
	var len;
	var ret;

	if(m !== null) {
		ret = [];
		for(i=0; i<parseInt(m[1], 10); i += 1) {
			this.writeField(val[i], m[2], def[i]);
		}
		return(ret);
	}
	if(val === undefined) {
		val=def;
	}
	switch(fieldtype) {
		case "Float":
			if (isNaN(val)) {
				val = 0;
			}
			wr=val.toExponential(15);
			while(wr.length < 22) {
				wr=wr+"\x00";
			}
			this.file.write(wr,22);
			break;
		case "SignedInteger":
			if (isNaN(val)) {
				val = 0;
			}
			if(val < -2147483648) {
				val = -2147483648;
			}
			if(val > 2147483647) {
				val = 2147483647;
			}
			this.file.writeBin(val,4);
			break;
		case "Integer":
			if (isNaN(val)) {
				val = 0;
			}
			if(val<0) {
				val=0;
			}
			if(val>4294967295) {
				val=4294967295;
			}
			this.file.writeBin(val,4);
			break;
		case "SignedInteger16":
			if (isNaN(val)) {
				val = 0;
			}
			if(val < -32768) {
				val = -32768;
			}
			if(val > 32767) {
				val = 32767;
			}
			this.file.writeBin(val,2);
			break;
		case "Integer16":
			if (isNaN(val)) {
				val = 0;
			}
			if(val<0) {
				val=0;
			}
			if(val>65535) {
				val=65535;
			}
			this.file.writeBin(val,2);
			break;
		case "SignedInteger8":
			if (isNaN(val)) {
				val = 0;
			}
			if(val < -128) {
				val = -128;
			}
			if(val > 127) {
				val = 127;
			}
			this.file.writeBin(val,1);
			break;
		case "Integer8":
			if (isNaN(val)) {
				val = 0;
			}
			if(val<0) {
				val=0;
			}
			if(val>255) {
				val=255;
			}
			this.file.writeBin(val,2);
			break;
		case "Date":
			wr=val.substr(0,8);
			while(wr.length < 8) {
				wr=wr+"\x00";
			}
			this.file.write(wr,8);
			break;
		case "Boolean":
			if(val.valueOf()) {
				this.file.writeBin(255,1);
			}
			else {
				this.file.writeBin(0,1);
			}
			break;
		default:
			m=fieldtype.match(/^String:([0-9]+)$/);
			if(m !== null) {
				len=parseInt(m[1], 10);
				wr=val.substr(0,len);
				while(wr.length < len) {
					wr=wr+"\x00";
				}
				this.file.write(wr,len);
			}
			else {
				m=fieldtype.match(/^PString:([0-9]+)$/);
				if(m !== null) {
					this.file.writeBin(wr.length, 1);
					len=parseInt(m[1], 10);
					wr=val.substr(0,len);
					while(wr.length < len) {
						wr=wr+"\x00";
					}
					this.file.write(wr,len);
				}
			}
	}
};

RecordFile.prototype.WriteField = function(val, fieldtype, def)
{
	'use strict';
	return this.writeField(val, fieldtype, def);
};
