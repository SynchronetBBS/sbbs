// The user's settings as the server last reported them (a settings.get or
// settings.set result), shared by the views that use them: compose (From
// addresses, display name, signature), the reader (the senders whose
// images are always shown), the settings view and the theme.
//
// prefs.get()            the last result, or null before the first one
// prefs.set(result)      a newer result (settings.get or settings.set):
//                        every listener is called with it
// prefs.load(opts)       settings.get (rpc.call's opts), taken as the newest;
//                        a Promise of the result (rejects as rpc.call does)
// prefs.on(fn)           fn(result) after each set(); returns a function
//                        that removes it
// prefs.reset()          forgets the result (at sign-out), keeps listeners

import { rpc } from "./rpc.js";

let current = null;
const listeners = new Set();

export const prefs = {
	get: () => current,
	set(r) {
		if (!r || typeof r !== "object")
			return;
		current = r;
		for (const f of Array.from(listeners)) {
			try {
				f(r);
			} catch (e) {
				// one view's failure does not keep the others from the change
			}
		}
	},
	load(opts) {
		return rpc.call("settings.get", null, opts).then((r) => {
			prefs.set(r);
			return r;
		});
	},
	on(f) {
		listeners.add(f);
		return () => listeners.delete(f);
	},
	reset() {
		current = null;
	}
};
