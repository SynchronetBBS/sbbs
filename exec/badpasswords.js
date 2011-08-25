load("sbbsdefs.js");
var badpasswords=0;
user = new User(1);
for(u=1; u<=system.lastuser; u++) {
	user.number=u;
	if(user==null)
		break;
    if(user.settings&(USER_DELETED|USER_INACTIVE))
	    continue;
	if(!system.trashcan("password",user.security.password))
		continue;
	printf("%-25s: %s\r\n", user.alias, user.security.password);
	badpasswords++;
}

printf("%u bad passwords found\n", badpasswords);