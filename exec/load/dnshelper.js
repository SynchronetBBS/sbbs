/*
   dnshelper.js -- quick script to do a DNS resolution and display the result.

   This is intended to be used from inside another JS script by using the
   Synchronet load() function. Example:

   q = load(true,"dnshelper.js",some_ip_address);
   var dns_result = 0;
   while (!dns_result) {
      dns_result = q.read();
   }
   log("Got result: " + dns_result);

   This is intended for utilization in a situation where blocking is not
   acceptable.  Otherwise simply use resolve_host() as per normal.
*/

if (!argv[0]) {
	exit();
}

var resolved_hostname = resolve_host(argv[0]);

if (resolved_hostname)
	resolved_hostname;
else
	false;

