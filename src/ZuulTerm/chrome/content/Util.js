
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
