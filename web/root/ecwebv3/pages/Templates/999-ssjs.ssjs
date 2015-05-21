//Document Title

/*	The first comment line in an SSJS page will be used as its title in the
	"pages" sidebar module (where the "Home", "Forum" and links to other
	local pages appear.)  Edit "Document Title" above as desired, replacing
	it with "HIDDEN:Title" if you do not want this page to appear in the list. */

load('webInit.ssjs');

print("<span class='title'>SSJS Page</span><br /><br />");
print("This is a Server-Side Javascript page.  It's more cumbersome than an");
print("XJS page, but can be useful in certain situations.<br /><br />");

// SSJS example:
if(!user.number || user.alias == webIni.WebGuest)
	print("You are not logged in.");
else
	print("You are logged in as " + user.alias + " #" + user.number);
