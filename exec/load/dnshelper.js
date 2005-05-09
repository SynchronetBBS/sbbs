// $Id$

/*
   dnshelper.js -- quick script to do a DNS resolution and display the result.

   This is intended to be used from inside another JS script by using the
   Synchronet load() function. Example:

   q = load(true,"dnshelper.js",some_ip_address);
   var dns_result;
   do {
		dns_result = q.read();
   while (!dns_result);

   log("Got result: " + dns_result);

   This is intended for utilization in a situation where blocking is not
   acceptable.  Otherwise simply use resolve_host() as per normal.
*/

if(resolve_host(argv[0])==null)
	false;	// Work-around for bug in v3.12a (segfault when decoding "null")