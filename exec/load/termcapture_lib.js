// $Id$

// Currently, only supports Telnet and RLogin

const ESC = 27

var telnet=load(new Object, "telnet_lib.js");

var protocol = "telnet";
var address = "localhost";
var port = 23;
var rows = 24;
var timeout = 30;

function capture()
{
    if(rows == undefined)
            rows=24;
    if(port == undefined) {
        switch(protocol) {
            case "telnet":  port=23; break;
    //        case "ssh":     port=22; break;
            case "rlogin":  port=513; break;
            default:
                alert("Unknown protocol: " + protocol);
                return false;
        }
    }

    var socket=new Socket();
    if(!socket.connect(address, port)) {
        alert("Error " + socket.error + " connecting to: " + address);
        return false;
    }
    /* RLogin handshake: */
    if(protocol=="rlogin") {
        if(!socket.poll(timeout)) {
            alert("Timeout wiating for initial byte from from rlogin server");
            return false;
        }
        var byte=socket.recvBin(1);
        if(byte != 0) {
            alert("Expected 0 from rlogin server, received: " + byte);
            return false;
        }
        socket.sendBin(0, 3);
    }
    var lastbyte=0;
    var lines=[];
    var curline="";
    delete this.hello;

    while(socket.is_connected) {
        if(!socket.poll(timeout))
            break;
        var byte = socket.recvBin(1);
        var char = ascii(byte);
        if(protocol=="telnet" && byte == telnet.IAC) {
            var cmd  = socket.recvBin(1);
            log(LOG_DEBUG, "Telnet: " + telnet.cmdstr(cmd));
            switch(cmd) {
                case telnet.WILL:
                case telnet.WONT:
                case telnet.DO:
                case telnet.DONT:
                    var arg = socket.recvBin(1);
                    socket.sendBin(telnet.IAC, 1);
                    socket.sendBin(telnet.ack(cmd), 1);
                    socket.sendBin(arg, 1);
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
            if(!this.hello)
                this.hello = lines.slice();
            lines.length = 0;
            curline="";
        } else if(char=='\n') {
            lines.push(curline);
            curline="";
        } else if(byte != ESC)
            curline+=char;
        lastbyte=byte;
    }
    if(curline.length)
        lines.push(curline);
    if(!this.hello)
        this.hello = lines.slice();
    return lines;
}

/* Leave as last line for convenient load() usage: */
this;