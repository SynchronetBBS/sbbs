/* Event-loop tests need a standalone runtime, not deferred suite callbacks. */
load(js.exec_dir+"standalone.sjs")(js.exec_dir+"callback-fairness.sjs");
