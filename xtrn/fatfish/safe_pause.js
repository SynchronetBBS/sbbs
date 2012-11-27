function safe_pause() {
    //console.crlf();

    console.write(ANSI.DEFAULT + ANSI.BG_GREEN + ANSI.FG_GREEN + ANSI.BOLD + "  Press ENTER. " + ANSI.DEFAULT);
    var d = "";
    while (d != "\r") {
        d = console.inkey(K_NOSPIN | K_NOCRLF | K_NOECHO, wait_time);

        /* TODO: cycle what's needed. */
    }
    console.crlf();
    console.line_counter = 0;
}