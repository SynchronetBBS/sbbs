TODO: we want to implement a standalone version of an existing chat subprogram in our shell so other sysops can run the software to either run their own standalone versions or connect to servers.

For reference, this is the subprogram's location we want to try to port over, /sbbs/mods/future_shell/lib/subprograms/chat.js - we do not have to do a 1:1 copy but we do want compatability.

I think there are other considerations to be taken into account when looking at this from a multiple BBS perspective.  Such as, if a user is not on our BBS, how do we fetch an avatar for them?  Maybe it's already possible as we do similar queries in dovenet in our message board code, but maybe we need to think about maybe embedding an avatar string in a "WHO" query, almost add avatars as part of the subscription process and stored data.

I am just thinking outside the box here a bit, but it would be cool to have like a server registry where we could connect to different json-chat instances.  maybe even multiple at a time.  this is probably a v2 or later feature.

I also think our chat is woefully underpowered when it comes to things like changing channels, sending a private message, etc.  This might be a good place to explore those features.

For now, perhaps it makes sense to add an .ini files that will connect to my BBS.  We should maybe use the install-xtrn.ini paradigm for simpler install for sysops, see various xtrn subdirectories for examples.

I'd like us to use typescript for this project, we have some guidelines around using typescript for synchronet in the /sbbs/xtrn/avatar_chat/.github/copilot-instructions.md ... Using typescript makes things infinitely more maintainable and cuts back on errors.  Let's concatenate all the transpiled output to a monolithic .js file for synchronet to run as an xtrn at /sbbs/xtrn/avatar_chat/avatar_chat.js
