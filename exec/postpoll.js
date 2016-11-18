var i;
var basecode;
var option = [];

for(i in argv)
        if(argv[i].charAt(0) != '-') {
                if(!basecode)
                        basecode = argv[i];
        } else
                option[argv[i].slice(1)] = true;

if(this.bbs)
	basecode = bbs.cursub_code;

while(!basecode)
        basecode = prompt("message base");

var msgbase = MsgBase(basecode);
if(!msgbase.open()) {
        alert("Error " + msgbase.last_error + " opening " + basecode);
        exit();
}

var poll = { field_list: [] };
while(!poll.subject)
	poll.subject = prompt("Poll question");

var comment;
while(comment = prompt("Comment [done]")) poll.field_list.push({ type: 0x62, data: comment});

var count=0;
var answer;
while(answer = prompt("Answer "+ (++count) + " [done]")) poll.field_list.push({ type: 0xe0, data: answer});

print();
print("Results Visibility:");
print("0 = voters only (and pollster)");
print("1 = everyone always");
print("2 = everyone once poll is closed (and pollster)");
print("3 = pollster only");
var results = parseInt(prompt("Results"));
poll.auxattr = results << 30;
if(js.global.bbs) {
	poll.from = user.alias;
	poll.from_ext = user.number;
} else {
	poll.from = system.operator;
	poll.from_ext = 1;
}
print("posted from: " + poll.from);
if(!msgbase.add_poll(poll))
	alert("Error " + msgbase.status + " " + msgbase.last_error);
