/* Callback connection lifetime tests require their own event loop. */
load(js.exec_dir + "standalone.sjs")(js.exec_dir + "connect-callbacks.sjs");
