Sound files for the @SOUND:, @MUSIC: and @CACHE_AUDIO: @-codes
--------------------------------------------------------------

Files placed here can be played in the user's terminal from any display
file or text string, e.g.:

    @SOUND:beep.wav@            play once
    @SOUND:beep.wav:50@         play once at 50 percent
    @SOUND:beep.wav:-6dB@       play once at -6 dB
    @MUSIC:theme.ogg@           play looped
    @MUSICVOL:30@               change the looping volume
    @MUSICOFF:500@              stop it, fading out over 500 ms
    @SOUNDOFF@                  stop everything
    @CACHE_AUDIO:theme.ogg@     send it ahead of time, without playing

All of these take a bare filename in this directory, so @CACHE_AUDIO:
sends exactly the file that a later @SOUND: or @MUSIC: of the same name
will play.

Formats
-------
The user's terminal decodes these, not the BBS. WAV, VOC, OGG and FLAC
work wherever the terminal has libsndfile; MP3 needs libsndfile 1.1.0 or
later. The format is detected from the file contents, so the extension
does not matter.

Size
----
Each file is sent to the user's terminal, so size is bandwidth. It is
sent once per client machine and cached there, not once per session, so
the cost falls on a user's first visit only.

Two limits in ctrl/main.ini bound what a display file can send:

    max_sound_file_size     default 256K, for @SOUND: and @MUSIC:
    max_cache_file_size     default 4M, for @CACHE_AUDIO:

They differ because @SOUND: and @MUSIC: upload while a display file is
drawing, where a delay is noticeable, and @CACHE_AUDIO: is placed
deliberately where one is not. Put @CACHE_AUDIO: for a large piece of
music somewhere a pause is expected, such as the logon sequence, and the
matching @MUSIC: will start instantly. A file larger than
max_sound_file_size plays only once it has been preloaded this way.

Who hears it
------------
Nothing is sent to a terminal that cannot play it: support is detected
when the user connects. Users can also turn sound off for themselves,
under Terminal settings in the user defaults menu.
