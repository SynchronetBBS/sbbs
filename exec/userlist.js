load("sbbsdefs.js");

var lastuser=system.stats.total_users;
var u = new User(1);

for(i=1;i<=lastuser;i++) {
	u.number=i;
	if(u.settings&USER_DELETED)
		continue;
	printf("%d/%d ",i,lastuser);
	printf("%-30s %-30s %s\r\n"
		,u.alias
		,u.location
		,u.connection
		);
	if(bbs.sys_status&SS_ABORT)
		break;
}	