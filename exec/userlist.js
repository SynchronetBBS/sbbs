load("sbbsdefs.js");

var lastuser=system.stats.total_users;
var user = new User(1);

for(i=1;i<=lastuser;i++) {
	user.number=i;
	if(user.settings&USER_DELETED)
		continue;
	printf("%d/%d ",i,lastuser);
	printf("%-30s %-30s %s\r\n"
		,user.alias
		,user.location
		,user.connection
		);
	if(bbs.sys_status&SS_ABORT)
		break;
}	