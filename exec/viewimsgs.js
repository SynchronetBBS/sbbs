// Redisplay instant messages (telegrams and notifications)

require("sbbsdefs.js", 'P_NOATCODES');
require("key_defs.js", 'KEY_HOME');

const prev_key = bbs.text.Previous[0];
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
    console.printfile(msg, P_NOATCODES | P_AUTO_UTF8);
	++displayed;
    console.mnemonics("\r\n~@Quit@, ~Recent, ~@Previous@ or [~@Next@]: ");
    switch(console.getkeys("\b-+[]\x02\x1e\x0a\x1d\x06RPN\r" + console.quit_key, 0)) {
        case 'R':
        case KEY_HOME:
            num = -1;
            break;
        case prev_key:
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
        case console.quit_key:
            break loop;
        default:
            num++;
            break;
    }
}
if(!displayed)
	writeln("Sorry, no messages.");
displayed;
