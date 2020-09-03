/* $Id: privchat.js,v 1.2 2013/09/15 08:46:47 rswindell Exp $ */

/* JS implementation of sbbs_t::privchat() from src/sbbs3/chat.cpp */

/* Local/sysop and private node-to-node chat using sbbs v2 file I/O scheme */

/* ToDo: private node-to-node chat */
/* ToDo: split-screen mode */
/* ToDo: performance optimizations */

load("sbbsdefs.js");
load("text.js");

const PCHAT_LEN = 1000;
const clr_chatlocal = 13;
const clr_chatremote = 14;
var debug = false;
var local = false;
var node = 1;
var error_log_level = LOG_WARNING;

// Parse arguments
for(i = 0; i < argc; i++) {
    if(argv[i].toLowerCase() == "-d")			// debug
        debug = true;
    else if(argv[i].toLowerCase() == "-l")
        local = true;
    else if(argv[i].toLowerCase() == "-n") {
        node = parseInt(argv[i + 1]);
        i++;
    }
}

if(!local) {
    if (user.security.restrictions & UFLAG_C) {
        console.print(bbs.text(R_Chat));
        exit();
    }

    // lots to do here for private node-to-node chat
}

if(local)
    console.print(format(bbs.text(SysopIsHere), system.operator));
else
    console.print(bbs.text(WelcomeToPrivateChat));

var outfile = new File(system.node_dir + "chat.dab");
if(!outfile.open("wb+", /* shareable: */true)) {
    alert("Error " + outfile.error + " opening " + outfile.name);
    exit();
}

var infile = new File(local ? (system.node_dir + "lchat.dab") : (system.node_list[node - 1].dir + "chat.dab"));

if(!file_exists(infile.name))   /* Wait while it's created for the first time */
    mswait(2000);
if(!infile.open("wb+", /* shareable: */true, /* bufsize: */system.platform=="Win32" ? 1:0)) {
    alert("Error " + infile.error + " opening " + infile.name);
    exit();
}
log(LOG_DEBUG,"infile error: " + infile.error);

var buf=new Array(PCHAT_LEN);
infile.writeBin(buf, 1);
outfile.writeBin(buf, 1);
infile.position = 0;
outfile.position = 0;

system.node_list[bbs.node_num-1].misc &= ~NODE_RPCHT;
if(!local) {
    system.node_list[node-1].misc |= NODE_RPCHT;    /* Set "reset pchat flag" on other node */
    while(true) {
        /* Wait for other node to acknowledge and reset */
        if(!(system.node_list[node-1].misc&NODE_RPCHT))
            break;
        bbs.get_time_left();
        bbs.nodesync();
        sleep(500);
    }
}

var ch,key;

while(true) {
    console.line_counter = 0;
    if(!local && console.aborted)
        break;

    if((key=console.inkey(K_NONE, 100)) != '') {
        console.attributes = console.color_list[clr_chatlocal];
        if(key=='\b' || key==ascii(127) /* DELETE */)
            console.backspace();
        else if(key=='\t')
            console.print(' ');
        else if(key=='\r')
            console.crlf();
        else if(ascii(key)>=ascii(' '))
            console.print(key);
        else
            continue;

        ch=outfile.readBin(1);
        outfile.position--;
        if(ch==0) { /* hasn't wrapped */
            if(!outfile.writeBin(ascii(key), 1))
                log(error_log_level, outfile.name + " write error " + outfile.error);
            else
                outfile.flush();
        } else {
            log(error_log_level,"wrapped!?!?");
        }
    }

    if(infile.position >= PCHAT_LEN)
        infile.position = 0;

    file_utime(infile.name);
    ch=infile.readBin(1);
    if(ch < 0)
        log(error_log_level, infile.name + " read error: " + infile.error);
    else {
        infile.position--;
        if(ch != 0) {
            if(debug)
                log(LOG_DEBUG,"received " + ch);
            console.attributes = console.color_list[clr_chatremote];
            if(ch==ascii('\b') || ch==127 /* DELETE */)
                console.backspace();
            else if(ch==ascii('\t'))
                console.print(' ');
            else if(ch==ascii('\r'))
                console.crlf();
            else
                console.print(ascii(ch));
            /* Acknowledge receipt/display of character: */
            infile.writeBin(0,1);
            infile.flush();
        }
    }

    if(system.node_list[bbs.node_num-1].action != NODE_PCHT) {
        console.print(bbs.text(EndOfChat));
        break;
    }
    if(system.node_list[bbs.node_num-1].misc & NODE_RPCHT) {
        /* pchat has been reset so seek to beginning */
        infile.posiion = 0;
        outfile.position = 0;
        system.node_list[bbs.node_num-1].misc &= ~NODE_RPCHT;
    }
}

console.aborted=false;   // clear the aborted state in case user hit Ctrl-C
