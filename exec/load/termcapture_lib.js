// $Id: termcapture_lib.js,v 1.7 2019/03/22 14:23:53 rswindell Exp $

// Currently, only supports Telnet and RLogin

const ESC = 27

var telnet=load(new Object, "telnet_lib.js");

var protocol = "telnet";
var address = "localhost";
var port = 23;
var rows = 24;
var timeout = 30;
var poll_timeout = 10;
var max_lines = 512;
var max_line_length = 256*1024;
var error="";

function capture()
{ 
	if(!address) {
		error = "No address specified";
		return false;
	}
    if(rows == undefined)
		rows=24;
    if(!port) {
        switch(protocol) {
            case "telnet":  port=23; break;
    //        case "ssh":     port=22; break;
            case "rlogin":  port=513; break;
            default:
                error = "Unsupported protocol: " + protocol;
                return false;
        }
    }

    var socket=new Socket();
    if(!socket.connect(address, port)) {
        error = "No connect: " + socket.error;
        return false;
    }
    /* RLogin handshake: */
    if(protocol=="rlogin") {
        if(!socket.poll(poll_timeout)) {
            error = "RLogin Timeout";
            return false;
        }
        var byte=socket.recvBin(1);
        if(byte != 0) {
            error = "Invalid RLogin handshake: " + byte;
            return false;
        }
        socket.sendBin(0, 3);
    }
    var lastbyte=0;
    var lines=[];
    var curline="";
    var hello=[];
    var stopcause="Disconnected";
    var start=time();

    while(socket.is_connected) {
        if(js.terminated) {
            error = "Terminated locally";
            return false;
        }
        if(lines.length >= this.max_lines) {
            stopcause = "Max lines reached:" + this.max_lines;
            break;
        }
        if(time()-start >= timeout) {
            stopcause = "Capture timeout reached: " + timeout + " seconds";
            break;
        }
        if(!socket.poll(poll_timeout)) {
            stopcause = "Poll timeout reached: " + poll_timeout + " seconds";
            break;
        }
        var byte = socket.recvBin(1);
        var char = ascii(byte);
        if(protocol=="telnet" && byte == telnet.IAC) {
            var cmd  = socket.recvBin(1);
            log(LOG_DEBUG, "Telnet: " + telnet.cmdstr(cmd));
            switch(cmd) {
                case telnet.DO:
                case telnet.DONT:
                    var arg = socket.recvBin(1);
                    if(this.telnet_ack) {
                        socket.sendBin(telnet.IAC, 1);
                        socket.sendBin(telnet.ack(cmd), 1);
                        socket.sendBin(arg, 1);
                    }
                    break;
                case telnet.SB:
                    while(socket.recvBin(1)!=telnet.SE)
                        ;
                    break;
            }
            continue;
        }
        if(char == '[' && lastbyte == ESC) {
            this.ansi = "";
        } else if(this.ansi!=undefined) {
            this.ansi += char;
            //print("ANSI: " + this.ansi);
            if(this.ansi == "6n") {
                log(LOG_DEBUG, "Responding to ANSI cursor position request");
                socket.send(format("\x1b[%u;80R", rows));
                delete this.ansi;
                continue;
            }
            if((char >= 'A' && char <= 'Z') || (char >= 'a' && char <= 'z')) {
                curline += "\x1b[" + this.ansi;
                delete this.ansi;
            }
        } else if(byte==12) { /* FF */
            if(!hello.length)
                hello = lines.slice();
            lines.length = 0;
            curline="";
        } else if(char=='\n') {
            if(curline.length && curline.charAt(curline.length-1)=='\r')
                curline = curline.substring(0,curline.length-1);
            lines.push(curline);
            log(LOG_DEBUG, format("Captured %u lines (last: %u bytes)", lines.length, curline.length));
            curline="";
        } else if(byte != ESC) {
            curline+=char;
        }
        if(curline.length >= this.max_line_length) {
            log(LOG_DEBUG,"Stopping capture on line length of " + curline.length);
            stopcause = "Max line length reached:" + this.max_line_length + " bytes";
            break; 
        }
        lastbyte=byte;
    }
	var ip_address = socket.remote_ip_address;
	socket.close();
    if(curline.length)
        lines.push(curline);
    if(!hello.length)
        hello = lines.slice();

    for(var i=0; i<hello.length;) {
        if(hello[i].length == 0)
            hello.splice(i,1);
        else
            i++;
    }

    return { preview: lines, hello: hello, ip_address: ip_address, stopcause: stopcause };
}

/* Leave as last line for convenient load() usage: */
this;
