// Redisplay instant messages (telegrams and notifications)

require("sbbsdefs.js", 'P_NOATCODES');
require("key_defs.js", 'KEY_HOME');

var num = -1;
var displayed = 0;
loop:
while (bbs.online && !console.aborted) {
    var msg = system.data_dir + "msgs/" + format("%04u", user.number) + ".last.msg";
    if(num >= 0)
        msg = system.data_dir + "msgs/" + format("%04u", user.number) + ".last." + num + ".msg";
    console.clear();
    if(!file_exists(msg))
        break;
    var timestamp = system.timestr(file_date(msg));
    print("\1n\1cInstant messages displayed \1h" + timestamp);
    console.printfile(msg, P_NOATCODES);
	++displayed;
    console.mnemonics("\r\n~Quit, ~Recent, ~Prev, or [~Next]: ");
    prmpt:
    switch(console.getkeys("\b-+[]\x02\x1e\x0a\x1d\x06RPN\rQ")) {
        case 'R':
        case KEY_HOME:
            num = -1;
            break;
        case 'P':
        case '\b':
        case '-':
        case '[':
        case KEY_UP:
        case KEY_LEFT:
            if(num >=0)
                num--;
            else
                console.beep();
            break;
        case 'Q':
            break loop;
        default:
            num++;
            break;
    }
}
if(!displayed)
	writeln("Sorry, no messages.");
exit(displayed);
