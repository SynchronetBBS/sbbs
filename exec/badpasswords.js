load("sbbsdefs.js");
var badpasswords=0;
usr = new User(1);
for(u=1; u<=system.lastuser; u++) {
	usr.number=u;
	if(usr==null)
		break;
	if(usr.settings&(USER_DELETED|USER_INACTIVE))
	    continue;
	if(!system.trashcan("password",usr.security.password))
		continue;
	printf("%-25s: %s\r\n", usr.alias, usr.security.password);
	badpasswords++;
}

printf("%u bad passwords found\n", badpasswords);