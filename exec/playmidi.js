// playmidi.js

// Synchronet Module to play a single track from a MIDI file

const STATUS_META_EVENT			=0xff

/* Meta-Event types */
const EVENT_TYPE_SEQNUM			=0x00
const EVENT_TYPE_TEXT			=0x01
const EVENT_TYPE_COPYRIGHT		=0x02
const EVENT_TYPE_TRACK_NAME		=0x03
const EVENT_TYPE_INSTRUMENT		=0x04
const EVENT_TYPE_LYRIC			=0x05
const EVENT_TYPE_MARKER			=0x05
const EVENT_TYPE_QUE_POINT		=0x06
const EVENT_TYPE_PROG_NAME		=0x07
const EVENT_TYPE_DEV_NAME		=0x08
const EVENT_TYPE_END_OF_TRACK	=0x2f
const EVENT_TYPE_TEMPO			=0x51
const EVENT_TYPE_SMPTE_OFFSET	=0x54
const EVENT_TYPE_TIME_SIGNATURE =0x58
const EVENT_TYPE_KEY_SIGNATURE	=0x59
const EVENT_TYPE_PROPRIETARY	=0x7f

/* MIDI Channel Voice Messages (upper nibble) */
const EVENT_TYPE_NOTE_OFF		=0x80
const EVENT_TYPE_NOTE_ON		=0x90

var filename;
var debug=false;
var channel=0;
var note_start=0;
var note_on;
var note;
var last_delta=0;
var tempo=500;		/* msec/quarter-note (120 bbp) */
var ticks_per_qnote=500;

function readVar(file)
{
	var val = 0;
	var b = 0;

	do {
		val<<=7;
		b = file.readBin(1);
		if(b&0x80)
			val|=b^0x80;
		else
			val|=b;
	} while(!file.eof && !js.terminated && b&0x80);

	return val;
}

function ticks_per_msec()
{
	return(tempo/ticks_per_qnote);
}

for(i=0;i<argc;i++) {
	switch(argv[i]) {
		case "-d":
			debug=true;
			break;
		case "-c": /* channel */
			channel=parseInt(argv[++i]);
			break;
		default:
			filename=argv[i];
			break;
	}
}

if(!filename) {
	alert("filename not specified");
	exit();
}

if(!file_exists(filename))
	filename=system.exec_dir + filename;

file = new File(filename, true /* shareable */);

if(!file.open("rb")) {
	alert("error " + file.error + " opening " + filename);
	exit();
}

file.network_byte_order = true;

while(!file.eof && !js.terminated) {
	/* Read chunk */
	type = file.read(4);
	if(file.eof)
		break;
	length = file.readBin();

	if(debug) {
		writeln("type = " + type);
		writeln("length = " + length);
	}

	if(length<0) {
		alert("invalid length at " + file.position);
		break;
	}

	switch(type) {
		case 'MThd':
			fmat = file.readBin(2);
			ntrks = file.readBin(2);
			division = file.readBin(2);
			if(!(division&0x8000))
				ticks_per_qnote=division;
			if(debug) {
				writeln("\tformat: " + fmat);
				writeln("\tntrks:  " + ntrks);
				writeln("\tdivis:  " + division);
			}
			break;
		case 'MTrk':
			end_of_track = file.position + length;

			/* Read events */
			do {
				delta = readVar(file);
				status = file.readBin(1);

				if(debug) {
					writeln("\t\tdelta:  " + delta);
					writeln("\t\tstatus: " + format("%02X",status));
				}

				if(status == STATUS_META_EVENT) /* non-MIDI (Meta) event */
				{
					event_type=file.readBin(1);
					event_len=file.readBin(1);
					if(debug) {
						writeln("\t\t* Meta-Event");
						writeln("\t\t* type: " + format("%02X",event_type));
						writeln("\t\t* len:  " + format("%02X",event_len));
					}

					end_of_event = file.position + event_len;

					switch(event_type) {
						case EVENT_TYPE_TEXT:
							writeln("Text: " + file.read(event_len));
							break;
						case EVENT_TYPE_COPYRIGHT:
							writeln("Copyright: " + file.read(event_len));
							break;
						case EVENT_TYPE_TRACK_NAME:
							writeln("Track Name: " + file.read(event_len));
							break;
						case EVENT_TYPE_INSTRUMENT:
							writeln("Instrument: " + file.read(event_len));
							break;
						case EVENT_TYPE_LYRIC:
							writeln(file.read(event_len));
							break;
						case EVENT_TYPE_MARKER:
							writeln("Marker: " + file.read(event_len));
							break;
						case EVENT_TYPE_QUE_POINT:
							writeln("Que Point: " + file.read(event_len));
							break;
						case EVENT_TYPE_PROG_NAME:
							writeln("Program Name: " + file.read(event_len));
							break;
						case EVENT_TYPE_DEV_NAME:
							writeln("Device Name: " + file.read(event_len));
							break;
						case EVENT_TYPE_END_OF_TRACK:
							writeln("End of track");
							break;
						case EVENT_TYPE_TEMPO:
							track_tempo  = file.readBin(1)<<16;
							track_tempo |= file.readBin(1)<<8;
							track_tempo |= file.readBin(1);
							writeln("Tempo: " + track_tempo);
							temp = track_tempo/1000;	/* Convert from usec to msec */
							break;
						case EVENT_TYPE_TIME_SIGNATURE:
							nn=file.readBin(1);
							dd=file.readBin(1);
							cc=file.readBin(1);
							bb=file.readBin(1);
							writeln("Time Sigature: " + format("%u/%u",nn,Math.pow(2,dd)));
							break;
						default:
							while(file.position < end_of_event) {
								event_byte = file.readBin(1);
								if(debug)
									writeln("\t\t* event byte: ", format("%02X",event_byte));
							}
							break;
					}
					continue;
				}
				if(status&0x80 && (status&0x0f)==channel) {	/* Channel Voice Message */
					switch(status&0xf0) {
						case EVENT_TYPE_NOTE_ON:
							note_start=delta;
							note=file.readBin(1);
							velocity=file.readBin(1);
							if(note_on)
								break;
							writeln("\t\tnote on:  " + note);
							if(delta)
								sleep(delta*ticks_per_msec());
							note_on=note;
							break;
						case EVENT_TYPE_NOTE_OFF:
							note=file.readBin(1);
							velocity=file.readBin(1);
							writeln("\t\tnote off: " + note);
							if(note!=note_on)
								break;

							if(delta) {
								freq=523.50/32.0; /* Low 'C' */
								freq*=Math.pow(2,note/12);
								writeln("\t\tPlaying " + Math.floor(freq) + " for " + delta);
								beep(freq,delta*ticks_per_msec());
							}
							note_on=0;
							break;
					}
				} 
				/*
				else if(status&0x80)
					writeln("Ignoring message for channel: " + format("%02X",status&0x0f));
				*/

			} while(!file.eof && !js.terminated && file.position < end_of_track);

			break;
		default:
			file.position += length;
			break;

	}
}

file.close();
