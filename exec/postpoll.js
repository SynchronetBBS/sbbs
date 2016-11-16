var i;
var basecode;
var option = [];

for(i in argv)
        if(argv[i].charAt(0) != '-') {
                if(!basecode)
                        basecode = argv[i];
        } else
                option[argv[i].slice(1)] = true;

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

var answer;
while(answer = prompt("Answer "+ (poll.field_list.length+1) + " [done]")) poll.field_list.push({ type: 0xe0, data: answer});

if(this.bbs) {
	poll.from = user.alias;
	poll.from_ext = user.number;
} else {
	poll.from = system.operator;
	poll.from_ext = 1;
}
print("from: " + poll.from);
if(!msgbase.add_poll(poll))
	alert("Error " + msgbase.status + " " + msgbase.last_error);
