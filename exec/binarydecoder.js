// binarydecoder.js

// Synchronet Binary Attachment Decoder 
// for UUE and yEnc encoded binary attachments
// Requires Synchronet v3.10m or later

// $Id$

load("sbbsdefs.js");

const REVISION = "$Revision$".split(' ')[1];

printf("Synchronet Binary Decoder %s session started\r\n", REVISION);

lines_per_yield=10;
yield_length=1;
completed_files=0;
sub = new Array();

var ini_fname = system.ctrl_dir + "binarydecoder.ini";
file = new File(ini_fname);
if(file.open("r")) {
	sub = file.iniGetAllObjects("code","sub:");
	file.close();
}

if(argc) {
	print("Searching for sub-boards matching pattern: " + argv[0]);
	re = new RegExp(argv[0],"i");
	for(i in msg_area.sub)
		if(re.test(i))
			sub.push(msg_area.sub[i]);
}

for(i in sub) {

	msgbase = new MsgBase(sub[i].code);
	if(msgbase.open()==false) {
		printf("!ERROR %s opening msgbase: %s\r\n",msgbase.last_error,sub[i].code);
		delete msgbase;
		continue;
	}

	/* Read pointer file */
	ptr=0;
	ptr_fname = msgbase.file + ".sbd";
	ptr_file = new File(ptr_fname);
	if(ptr_file.open("r")) {
		ptr = parseInt(ptr_file.readln());
		ptr_file.close();
	}
	if(ptr<msgbase.first_msg)
		ptr=msgbase.first_msg;

	/* Default attachment dir */
	if(sub[i].attachment_dir==undefined)
		attachment_dir=msg_area.sub[sub[i].code].data_dir+"attach"
	else
		attachment_dir=sub[i].attachment_dir;

	/* Create attachment dir, if necessary */
	mkdir(attachment_dir);
	if(attachment_dir.substr(-1)!='/')
		attachment_dir+="/";

	/* save for later */
	msg_area.sub[sub[i].code].attachment_dir=attachment_dir;

	/* Read MD5 list */
	md5_fname=attachment_dir + "md5.lst";
	md5_list=new Array();
	md5_file=new File(md5_fname);
	if(md5_file.open("r")) {
		md5_list=md5_file.readAll();
		md5_file.close();
	}
	md5_list_length=md5_list.length;

	/* Read CRC-32 list */
	crc_fname=attachment_dir + "crc32.lst";
	crc_list=new Array();
	crc_file=new File(crc_fname);
	if(crc_file.open("r")) {
		crc_list=crc_file.readAll();
		crc_file.close();
	}
	crc_list_length=crc_list.length;

	/* Read parts database */
	parts_fname=attachment_dir + "parts.db";
	parts_list=new Array();
	parts_file=new File(parts_fname);
	if(parts_file.open("r")) {
		parts_list=parts_file.iniGetAllObjects();
		parts_file.close();
	}
	parts_list_modified=false;

	printf("Scanning %s\r\n",msgbase.cfg.code);

	last_msg=msgbase.last_msg;
	for(;ptr<=last_msg && bbs.online;ptr++) {

		console.inkey();	/* allow ^O toggle */
		printf("%s %lu of %lu\r\n"
			,msgbase.cfg.code, ptr, last_msg);

		hdr = msgbase.get_msg_header(
			/* retrieve by offset? */	false,
			/* message number */		ptr,
			/* regenerate msg-id? */	false
			);
		if(hdr == null)
			continue;
		if(hdr.attr&MSG_DELETE)	/* marked for deletion */
			continue;

		body = msgbase.get_msg_body(
				 false	/* retrieve by offset */
				,ptr	/* message number */
				,false	/* remove ctrl-a codes */
				,false	/* rfc822 formatted text */
				,false	/* include tails */
				);
		if(body == null) {
			printf("!FAILED to read message number %ld\r\n",ptr);
			continue;
		}

		file = new File(system.temp_dir + "binary.tmp");
		if(!file.open("w+b")) {
			printf("!ERROR %d opening/creating %s\r\n",file.error,file.name);
			continue;
		}

		var part=0;

		for(o in parts_list) {
			obj=parts_list[o];
			if(obj.codec=="uue" 
				&& (part=compare_subject(hdr.subject,obj.subject))!=0) {
				fname=obj.name.toString();
				file.uue=true;
			}
		}

		var begin,end;
		var first_line=0;

		lines=body.split("\r\n");

		for(li=0;li<lines.length;li++) {

			if(lines_per_yield && li && (li%lines_per_yield)==0)
				sleep(yield_length);

			line=lines[li];

			if(file.uue && line=="end") {
				if(!part)
					complete_file(file,fname,attachment_dir);
				li--;
				break;
			}

			if(file.yenc && line.substr(0,6)=="=yend "
				&& (part_size=line.indexOf(" size="))>0) {
				printf("yEnc trailer: %s\r\n",line);
				file.flush();
				if(part) {
					if((end_part=line.indexOf(" part="))<0) {
						printf("!yEnd part number (%ld) missing in trailer: %s\r\n"
							,part,line);
						continue;
					}
					end_part=parseInt(line.slice(end_part+6),10);
					if(end_part!=part) {
						printf("!yEnd part number mismatch, %ld in header, trailer: %s\r\n"
							,part,line);
						continue;
					}
				}

				part_size=parseInt(line.slice(part_size+6),10);
				if(part_size!=end-(begin-1)) {
					printf("!yEnc part size mismatch, %ld in header, trailer: %s\r\n"
						,end-(begin-1),line);
					printf("begin=%ld end=%ld\r\n",begin,end);
					continue;
				}
				if(part_size!=file.length) {
					printf("!yEnc part size mismatch, actual: %ld, trailer: %s\r\n"
						,file.length,line);
					continue;
				}
				if(!part && size!=part_size) {
					printf("!yEnc single part size mismatch: %ld in header, trailer: %s\r\n"
						,size,line);
					continue;
				}

				if(part && (crc32=line.indexOf(" pcrc32="))>0)
					crc32=parseInt(line.slice(crc32+8),16);
				else if(!part && (crc32=line.indexOf(" crc32="))>0)
					crc32=parseInt(line.slice(crc32+7),16);
				else
					crc32=undefined;

				file_crc32=file.crc32;
				if(crc32!=undefined && crc32!=file_crc32) {
					printf("!yEnc part CRC-32 mismatch, actual: %08lX, trailer: %s\r\n"
						,file_crc32,line);
					continue;
				}
				part_crc32=crc32;

				/* Get total file CRC-32, if specified */
				if(part) {
					if((crc32=line.indexOf(" crc32="))>0)
						crc32=parseInt(line.slice(crc32+7),16);
					else
						crc32=undefined;
				}

				if(crc32!=undefined) {
					for(ci=0;ci<crc_list.length;ci++)
						if(parseInt(crc_list[ci],16)==crc32)
							break;
					if(ci<crc_list.length) {
						printf("Duplicate CRC-32 found: %s\r\n",crc_list[ci]);
						continue;
					}
				}

				if(part && size!=part_size) {
					file.length=0;	/* truncate temp file */
					file.yenc=false;
					/* add to parts database */
					if(!add_part(parts_list,msgbase.cfg.code,hdr
						,fname,"yenc" /* codec */
						,part,total,first_line,li-1 /* last_line */
						,begin,end,part_crc32,size,crc32))
						console.pause();
					continue;
				}

				complete_file(file,fname,attachment_dir);
				break;
			}

			if(file.yenc || file.uue) {
				file.write(line);
				continue;
			}


			/* UUE */
			if(line.substr(0,6)=="begin ") {
				// Parse uuencode header
				arg=line.split(/\s+/);
				arg.splice(0,2);	// strip "begin 666 "
				fname=file_getname(arg.join(" "));
				file.uue=true;
				first_line=li+1;
				continue;
			}


			/* yEnc */
			if(line.substr(0,8)=="=ybegin " 
				&& line.indexOf(" line=")>0
				&& (size=line.indexOf(" size="))>0
				&& (name=line.indexOf(" name="))>0) {

				printf("yenc header: %s\r\n",line);

				size=parseInt(line.slice(size+6),10);
				fname=file_getname(line.slice(name+6));

				if((total=line.indexOf(" total="))>0)
					total=parseInt(line.slice(total+7),10);
		
				/* part? */
				part=line.indexOf(" part=");
				if(part>0)
					part=parseInt(line.slice(part+6),10);
				else
					part=0;
				if(part) {
					line=lines[++li];
					printf("ypart header: %s\r\n",line);
					if(line.substr(0,7)!="=ypart "
						|| (begin=line.indexOf(" begin="))<1
						|| (end=line.indexOf(" end="))<1) {
						printf("!yEnc MALFORMED ypart line: %s\r\n",line);
						continue;
					}
					begin=parseInt(line.slice(begin+7),10);
					end=parseInt(line.slice(end+5),10);
				} else {
					begin=1;
					end=size;
				}
				file.yenc=true;
				first_line=li+1;
				continue;
			}

		} /* for(li in lines) */

		if(file.is_open && file.uue) {
			/* adds to parts database */
			if(!add_part(parts_list,msgbase.cfg.code,hdr
				,fname,"uue" /* codec */
				,part,undefined
				,first_line,li-1 /* last_line */
				))
				console.pause();
		}

		file.remove();
		delete file;

		yield();
	} /* for(ptr<=msg.last_msg) */

	/* Save the scan pointer */
	if(ptr_file.open("w"))
		ptr_file.writeln(ptr);

	delete ptr_file;
	delete msgbase;

	/* Save Attachment MD5 history */
	if(md5_list.length!=md5_list_length) {
		print("Saving MD5 history");
		if(md5_file.open("w")) {
			md5_file.writeAll(md5_list);
			md5_file.close();
		}
	}

	/* Save Attachment CRC-32 history */
	if(crc_list.length!=crc_list_length) {
		print("Saving CRC-32 History");
		if(crc_file.open("w")) {
			crc_file.writeAll(crc_list);
			crc_file.close();
		}
	}

	if(parts_list_modified==true) {

		/* Combine and decode parts */
		print("Combining partial files");
		combine_parts(parts_list);

		/* Save the Partial file/parts Database */
		print("Saving partial file database");
		parts_file.open("w");
		for(o in parts_list) {
			obj=parts_list[o];
			parts_file.writeln("[" + obj.name + "]");
			for(prop in obj)
				parts_file.printf("%-20s=%s\r\n"
					,prop.toString(), obj[prop].toString());
			parts_file.writeln("; end of file: " + obj.name );
			parts_file.writeln();
		}
		parts_file.printf("; %lu partial files\n", parts_list.length);
		parts_file.close();
	}

} /* for(i in subs) */

printf("Synchronet Binary Decoder %s session complete (%lu files completed)\r\n"
	   ,REVISION, completed_files);

exit();	/* all done */

/****************************************************************************/
/* Compares two message subjects (subj1==new msg, subj2=file in database	*/
/* returning 0 if they differ in anything but decimal digits (or identical)	*/
/* returning the parsed part number otherwise								*/
/****************************************************************************/
function compare_subject(subj1, subj2)
{
	/* first compare lengths */
	length=subj1.length;
	if(!length)
		return(0);
	if(length!=subj2.length)	/* must be same length */
		return(0);
	if(subj1==subj2)			/* but not duplicate part */
		return(0);

	diff="";
	for(i=0;i<length;i++) {
		if(subj1.charAt(i)!=subj2.charAt(i))
			diff+=(subj1.charAt(i)+subj2.charAt(i));
	}
	ascii_0=ascii('0');
	ascii_9=ascii('9');
	length=diff.length;
	for(i=0;i<length;i++) {
		ch=ascii(diff.charAt(i));
		if(ch<ascii_0 || ch>ascii_9)
			break;
	}
	if(i<length)	/* differ in non-digit char */
		return(0);

	/* parse part number */
	part=subj1.lastIndexOf('(');
	if(part>=0)
		part=parseInt(subj1.slice(part+1),10);
	if(part>0)
		return(part);
	return(0);
}

/***********************************************************************************/
/* Adds a part of a binary file to the parts database for later combine and decode */
/***********************************************************************************/
function add_part(list,sub_code,hdr
				  ,fname,codec,part,total
				  ,first_line,last_line
				  ,begin,end,pcrc32,size,crc32)
{
	if(part<1) {		/* must parse from subject (yuck) */
		part=hdr.subject.lastIndexOf('(');
		if(part>=0)
			part=parseInt(hdr.subject.slice(part+1),10);
		if(part<1) {
			printf("!Failed to parse part number from: %s\r\n",hdr.subject);
			return(false);
		}
		printf("Parsed part number: %u\r\n",part);
	}
	if(total==undefined || total<part) {	/* must parse from subject (yuck) */
		total=hdr.subject.lastIndexOf('(');
		if(total>=0) {
			subject=hdr.subject.slice(total+1);
			total=subject.indexOf('/');
		}
		if(total>=0)
			total=parseInt(subject.slice(total+1),10);
		if(total<part) {
			printf("!Failed to parse total parts from: %s\r\n",hdr.subject);
			return(false);
		}
		printf("Parsed total parts: %u\r\n",total);
	}

	/* Search database for existing file object */
	var li;
	for(li=0; li<list.length; li++)
		if(list[li].name==fname && list[li].total==total && list[li].codec==codec
			&& (size==undefined || list[li].size==size)
			&& (crc32==undefined || parseInt(list[li].crc32,16)==crc32)
			) {
			printf("%s found in database\r\n",fname.toString());
			break;
		}

	obj=list[li];

	var time_str = system.timestr();
	var time_hex = format("%08lxh",time());

	if(obj==undefined)	{	/* new entry */

		printf("New parts database entry for: %s\r\n",fname.toString());
		printf("total=%u, size=%u, crc32=%08lx\r\n",total,size,crc32);

		obj = { name: fname, codec: codec, parts: 0, total: total };

		obj.subject=hdr.subject;	/* save first subject for additional UUE parts */
		obj.from=hdr.from;

		/* yEnc file fields */
		if(size!=undefined)
			obj.size=size;
		if(crc32!=undefined)
			obj.crc32=format("%08lxh",crc32);

		/* Timestamp */
		obj.created=time_str;
		obj.created_time=time_hex
	}

	/* Timestamp */
	obj.updated=time_str;
	obj.updated_time=time_hex;

	/* part message info */
	prop=format("part%u.id",part);
	if(obj[prop]!=undefined) {
		printf("Part %u of %s already in database\r\n",part,fname.toString());
		return(true);	// pretend we added it
	}

	printf("Adding part %u of %s-encoded file to database:\r\n",part,codec.toUpperCase());
	printf("File: %s\r\n",fname);

	obj[format("part%u.id",part)]=hdr.id;
	obj[format("part%u.sub",part)]=sub_code;
	obj[format("part%u.msg",part)]=hdr.number;
	obj[format("part%u.date",part)]=hdr.date;
	obj[format("part%u.added",part)]=time_str;
	obj[format("part%u.added_time",part)]=time_hex;
//	obj[format("part%u.from",part)]=hdr.from;
//	obj[format("part%u.subject",part)]=hdr.subject;

	/* These save us the hassle of parsing again later */
	obj[format("part%u.first_line",part)]=first_line;
	obj[format("part%u.last_line",part)]=last_line;

	/* yEnc part fields */
	if(begin!=undefined)
		obj[format("part%u.begin",part)]=begin;
	if(end!=undefined)
		obj[format("part%u.end",part)]=end;
	if(pcrc32!=undefined)
		obj[format("part%u.crc32",part)]=format("%08lxh",pcrc32);

	obj.parts++;

	list[li]=obj;

	parts_list_modified=true;

	printf("Part %u added (%u/%u parts in database)\r\n"
		,part,obj.parts,obj.total);

	return(true);
}

/****************************************************************************/
/* Combine parts for complete files											*/
/****************************************************************************/
function combine_parts(list,attachment_dir)
{
	var li;
	for(li=0; li<list.length; li++) {
		if(list[li].parts!=list[li].total)
			continue;
		var obj=list[li];
		var sub_code;
		printf("File complete: %s (%u parts)\r\n", obj.name, obj.parts);

		file = new File(system.temp_dir + "binary.tmp");
		if(!file.open("w+b")) {
			printf("!ERROR %d opening/creating %s\r\n", file.error, file.name);
			continue;
		}
		file[obj.codec]=true;	/* yenc or uue */

		var pi;
		for(pi=1;pi<=obj.total;pi++) {
			printf("Processing part %u of %u\r\n",pi,obj.total);
			var prefix=format("part%u.",pi);
			sub_code=obj[prefix + "sub"];
			msgbase=new MsgBase(sub_code);
			if(msgbase.open()==false) {
				printf("!ERROR %s opening msgbase: %s\r\n",msgbase.last_error,sub_code);
				delete msgbase;
				break;
			}
			ptr=obj[prefix + "msg"];
			body = msgbase.get_msg_body(
					 false	/* retrieve by offset */
					,ptr	/* message number */
					,false	/* remove ctrl-a codes */
					,false	/* rfc822 formatted text */
					,false	/* include tails */
					);
			if(body == null) {
				printf("!FAILED to read message number %ld\r\n",ptr);
				break;
			}

			lines=body.split("\r\n");

			first_line=obj[prefix + "first_line"];
			last_line=obj[prefix + "last_line"];
			for(var l=first_line;l<=last_line;l++)
				file.write(lines[l]);

			delete msgbase;
		}
		if(pi<=obj.total) {
			file.remove();
			printf("!Combine failure, removing part %u\r\n",pi);
			list[li][format("part%u.id",pi)]=undefined;
			continue;
		}

		if(obj.size!=undefined && obj.size!=file.length) {
			file.remove();
			printf("!File length mismatch, actual: %lu, expected: %lu\r\n"
				,file.length, obj.size);
			continue;
		}

		/* Verify final CRC-32, if available */
		if(obj.crc32!=undefined) {
			file_crc32=file.crc32;
			crc32=parseInt(obj.crc32,16);
			if(crc32!=file_crc32) {
				file.remove();
				printf("!CRC-32 failure, actual: %08lx, expected: %08lx\r\n"
					,file_crc32, crc32);
				continue;
			}
		}

		complete_file(file
			,obj.name.toString()
			,msg_area.sub[sub_code].attachment_dir);

		// Remove file entry from database
		list.splice(li--,1);
		parts_list_modified=true;
	}
}

/***********************************************************************************/
/* When a file is completely decoded, verifies uniqueness and copies to output dir */
/***********************************************************************************/
function complete_file(file, fname, attachment_dir)
{
	fname=fname.replace(/ /g,'_');	/* no spaces in filenames */

	printf("Completing attachment: %s\r\n",fname);

	md5=file.md5_hex;
	crc32=file.crc32;
	file.close();

	for(mi=0;mi<md5_list.length;mi++)
		if(md5_list[mi].substr(0,32)==md5)
			break;
	if(mi<md5_list.length) {
		printf("Duplicate MD5 digest found: %s\r\n",md5_list[mi]);
		return(false);
	}
	md5_list.push(format("%s %s",md5,fname));

	for(ci=0;ci<crc_list.length;ci++)
		if(parseInt(crc_list[ci],16)==crc32)
			break;
	if(ci<crc_list.length) {
		printf("Duplicate CRC-32 found: %s\r\n",crc_list[ci]);
		return(false);
	}
	crc_list.push(format("%08lx %s",crc32,fname));

	new_fname=fname;
	file_num=0;
	while(file_exists(attachment_dir + new_fname) && file_num<1000) { 
		// generate unique name, if necessary
		ext=fname.lastIndexOf('.');
		if(ext<0)
			ext="";
		else
			ext=fname.slice(ext);
		// Convert filename.ext to filename.<article>.ext
		new_fname=format("%.*s.%lu%s",fname.length-ext.length,fname,file_num++,ext);
	}
	fname=attachment_dir + new_fname;

	if(!file_rename(file.name,fname)) {
		printf("Error %d renaming %s to %s\r\n",errno,file.name,fname);
		return(false);
	}

	printf("Attachment saved as: %s\r\n",fname);
	completed_files++;
	return(true);
}