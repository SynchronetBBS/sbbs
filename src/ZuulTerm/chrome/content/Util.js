
// From http://sejq.blogspot.com/2008/12/two-javascript-synchronous-sleep.html
/**
 * Netscape compatible WaitForDelay function.
 * You can use it as an alternative to Thread.Sleep() in any major programming language
 * that support it while JavaScript it self doesn't have any built-in function to do such a thing.
 * parameters:
 *  (Number) delay in millisecond
*/
function nsWaitForDelay(delay) {
    /**
    * Just uncomment this code if you're building an extention for Firefox.
    * Since FF3, we'll have to ask for user permission to execute XPCOM objects.
    */
    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

    // Get the current thread.
    var thread = Components.classes["@mozilla.org/thread-manager;1"].getService(Components.interfaces.nsIThreadManager).currentThread;

    // Create an inner property to be used later as a notifier.
    this.delayed = true;

    /* Call JavaScript setTimeout function
     * to execute this.delayed = false
     * after it finish.
     */
    setTimeout("this.delayed = false;", delay);

    /**
     * Keep looping until this.delayed = false
    */
    while (this.delayed) {
        /**
         * This code will not freeze your browser as it's documented in here:
         * https://developer.mozilla.org/en/Code_snippets/Threads#Waiting_for_a_background_task_to_complete
        */
        thread.processNextEvent(true);
    }
}

function decode(encoded)
{
	var bytes=0;
	var i;
	var val=0xfffd;
	var firstbyte=encoded.charCodeAt(0);

	if((firstbyte & 0xe0) == 0xc0) {
		bytes=2;
		val=firstbyte & 0x1f;
	}
	else if((firstbyte & 0xf0) == 0xe0) {
		bytes=3;
		val=firstbyte & 0x0f;
	}
	else if((firstbyte & 0xf8) == 0xf0) {
		bytes=4;
		val=firstbyte & 0x07;
	}
	else if((firstbyte & 0xfc) == 0xf8) {
		bytes=5;
		val=firstbyte & 0x03;
	}
	else if((firstbyte & 0xfe) == 0xfc) {
		bytes=6;
		val=firstbyte & 0x01;
	}
	for(i=1; i<bytes; i++) {
		if((encoded.charCodeAt(i) & 0xc0) != 0x80) {
			val = 0xfffd;
			break;
		}
		val <<= 6;
		val |= (encoded.charCodeAt(i) & 0x3f);
	}

	return String.fromCharCode(val);
}

function bytesToUnicode(bytes)
{
	var ret={unicode:'',remainder:''};
	var i;
	var encoded='';

	for(i=0; i<bytes.length; i++) {
		if(bytes.charCodeAt(i) < 128) {
			if(encoded.length > 0) {
				ret.unicode += decode(encoded);
				encoded='';
			}
			ret.unicode += bytes.charAt(i);
		}
		else {
			if(encoded.legnth > 0 && (bytes.charCodeAt(i) & 0x0c) != 0x0c) {
				ret.unicode += decode(encoded);
				encoded='';
			}
			encoded += bytes.charAt(i);
		}
	}
	ret.remainder=encoded;
	return ret;
}
