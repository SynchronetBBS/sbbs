js.global.load(js.global,"cnfdefs.js");

var CNF = new (function() {

	/* read an int from a cnf file */
	function getInt(file,bytes) {
		var i = file.readBin(bytes);
		return i;
	}

	/* read a string from a cnf file */
	function getStr(file,bytes) {
		var s = file.read(bytes);
		return s.replace(/\0/g,'');
	}

	/* write an int to a cnf file */
	function setInt(file,bytes,val) {
		file.writeBin(val,bytes);
	}

	/* write a null-padded string to a cnf file */
	function setStr(file,bytes,str) {
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

	/* read a set of records from xtrn.cnf */
	function readArray(file,struct) {
		var list = [];
		var records = getInt(file,UINT16_T);
		for(var i=0;i<records;i++) {
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
					data[p] = getStr(file,struct[p].bytes);
					break;
				case "obj":
					data[p] = readRecord(file,struct[p].bytes);
					break;
				case "lst":
					data[p] = readArray(file,struct[p].bytes);
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
			var data = {};
			if(file.eof) 
				break;
			data.name = getStr(file,struct.name.bytes);
			list[i] = data;
		}
		records = i;
		for(var i=0;i<records;i++) {
			if(file.eof)
				break;
			list[i].misc = getInt(file,struct.misc.bytes);
		}
		return list;
	}

	/* write a set of records to xtrn.cnf */
	function writeArray(file,struct,records) {
		setInt(file,UINT16_T,records.length);
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
					setStr(file,struct[p].bytes,record[p]);
					break;
				case "obj":
					writeRecord(file,struct[p].bytes,record[p]);
					break;
				case "lst":
					writeArray(file,struct[p].bytes,record[p]);
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
		for(var i=0;i<records;i++) {
			setInt(file,struct.misc.bytes,records[i].misc);
		}
	}

	/* read records from .cnf file */
	this.read = function(fileName,struct) {
		var f = new File(fileName);
		f.open('rb',true);
		f.etx=3;
		
		var data = readRecord(f,struct);

		f.close();
		return data;
	}

	/* write records to .cnf file */
	this.write = function(fileName,struct,data) {
		var f = new File(fileName);
		if(!f.open('wb',true))
			return false;
		
		writeRecord(f,struct,data);
		f.close();
		return true;
	}

})();
