/* CNF data structure definitions (see scfglib2.c, scfgdefs.h) 
	$Id: cnflib.js,v 1.15 2020/03/15 08:47:52 rswindell Exp $
*/

js.global.load(js.global,"cnfdefs.js");

var CNF = new (function() {

	/* read an int from a cnf file */
	function getInt(file,bytes) {
		var i = file.readBin(bytes);
		return i;
	}

	/* read a string from a cnf file (optional string length record) */
	function getStr(file,bytes,length) {
		if(typeof length !== 'undefined' && length) {
			var i = file.readBin(length);
			var s = file.read(bytes);
			return s.substr(0,i);
		}
		else {
			var s = file.read(bytes);
			return s.replace(/\0/g,'');
		}
	}

	/* write an int to a cnf file */
	function setInt(file,bytes,val) {
		file.writeBin(val,bytes);
	}

	/* write a null-padded string to a cnf file (optional string length record) */
	function setStr(file,bytes,str,length) {
		if(typeof length !== 'undefined' && length) {
			setInt(file,length,str.length);
		}
		if(str === undefined)
			str = '';
		file.write(str,bytes);
	}

	/* get record padding */
	function getPadding(file,bytes) {
		file.position += bytes;
		return false;
	}

	/* set record padding */
	function setPadding(file,bytes) {
		setStr(file,bytes,"");
	}

	/* read a set of records from *.cnf */
	function readArray(file,struct,length) {
		var list = [];
		if(length == undefined)
			length = getInt(file,UINT16_T);
		for(var i=0;i<length;i++) {
			var data = readRecord(file,struct);
			list[i] = data;
		}
		return list;
	}

	/* read an individual record */
	function readRecord(file,struct) {
		var data = {};
		for(var p in struct) {
			if(file.eof)
				break;
			if(p.match(/__PADDING\d*__/))
				getPadding(file,struct[p]);
			else {
				switch(struct[p].type) {
				case "int":
					data[p] = getInt(file,struct[p].bytes);
					break;
				case "str":
					data[p] = getStr(file,struct[p].bytes,struct[p].length);
					break;
				case "obj":
					data[p] = readRecord(file,struct[p].bytes);
					break;
				case "lst":
					data[p] = readArray(file,struct[p].bytes,struct[p].length);
					break;
				case "ntv":
					data[p] = readNative(file,struct[p].bytes);
					break;
				}
			}
		}
		return data;
	}

	/* read native programs list from xtrn.cnf (because digitalman broke the pattern) */
	function readNative(file,struct) {
		var list = [];
		var records = getInt(file,UINT16_T);
		
		for(var i=0;i<records;i++) {
			if(file.eof) 
				break;
			list[i] = {
				name:getStr(file,struct.name.bytes),
				misc:0
			};
		}
		records = i;
		for(var i=0;i<records;i++) {
			if(file.eof)
				break;
			list[i].misc = getInt(file,struct.misc.bytes);
		}
		return list;
	}

	/* write a set of records to *.cnf */
	function writeArray(file,struct,length,records) {
		if(length == undefined) {
			setInt(file,UINT16_T,records.length);
		}
		for(var i=0;i<records.length;i++) {
			writeRecord(file,struct,records[i]);
		}
	}

	/* write an individual record */
	function writeRecord(file,struct,record) {
		for(var p in struct) {
			if(p.match(/__PADDING\d*__/))
				setPadding(file,struct[p]);
			else {
				switch(struct[p].type) {
				case "int":
					setInt(file,struct[p].bytes,record[p]);
					break;
				case "str":
					setStr(file,struct[p].bytes,record[p],struct[p].length);
					break;
				case "obj":
					writeRecord(file,struct[p].bytes,record[p]);
					break;
				case "lst":
					writeArray(file,struct[p].bytes,struct[p].length,record[p]);
					break;
				case "ntv":
					writeNative(file,struct[p].bytes,record[p]);
					break;
				}
			}
		}
	}

	/* write native programs list to xtrn.cnf */
	function writeNative(file,struct,records) {
		setInt(file,UINT16_T,records.length);
		for(var i=0;i<records.length;i++) {
			setStr(file,struct.name.bytes,records[i].name);
		}
		for(var i=0;i<records.length;i++) {
			setInt(file,struct.misc.bytes,records[i].misc);
		}
	}

	/* determine the size of a record (in bytes) */
	this.getBytes = function(struct) {
		var bytes = 0;
		for each(var p in struct) {
			bytes += p.bytes;
			if(p.type == "str" && p.length > 0)
				bytes += p.length;
		}
		return bytes;
	}

	this.fullpath = function(fileName) {
		if(file_name(fileName) == fileName)
			return system.ctrl_dir + fileName;
		return fileName;
	}
	
	/* read records from .cnf file */
	this.read = function(fileName,struct) {
		if(!struct) {
			switch(file_getname(fileName).toLowerCase()) {
				case "main.cnf":
					struct = js.global.struct.main;
					break;
				case "xtrn.cnf":
					struct = js.global.struct.xtrn;
					break;
				case "msgs.cnf":
					struct = js.global.struct.msg;
					break;
				case "file.cnf":
					struct = js.global.struct.file;
					break;
				default:
					return false;
			}
		}
		var f = new File(fullpath(fileName));
		if(!f.open('rb'))
			return false;

		var data = readRecord(f,struct);

		f.close();
		return data;
	}

	/* write records to .cnf file */
	this.write = function(fileName,struct,data) {
		if(!struct) {
			switch(file_getname(fileName).toLowerCase()) {
				case "main.cnf":
					struct = js.global.struct.main;
					break;
				case "xtrn.cnf":
					struct = js.global.struct.xtrn;
					break;
				case "msgs.cnf":
					struct = js.global.struct.msg;
					break;
				case "file.cnf":
					struct = js.global.struct.file;
					break;
				default:
					return false;
			}
		}
		if(js.terminated)
			return false;
		var f = new File(fullpath(fileName));
		if(!f.open('wb'))
			return false;
		
		writeRecord(f,struct,data);
		f.close();
		return true;
	}

})();

CNF;
