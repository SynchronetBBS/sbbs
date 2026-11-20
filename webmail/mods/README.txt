WebMail mods directory
======================
Put your own changes to the web client here. Your files in this directory
are never replaced or removed when Synchronet is updated (this README is).

Language overlays
-----------------
To change some of the client's wording, create

    mods/locale/<name>.ini

where <name> is the locale to change, such as en_us or es_ar (lowercase
letters and an underscore). The file lists only the keys you want to
change. Anything it does not list keeps the stock wording from
lib/locale/<name>.ini (or from lib/locale/en_us.ini if the stock locale
has no such file). Each key is in a section; the stock files show the
section and key names.

Example, mods/locale/en_us.ini, renaming Trash and changing the
sign-in button:

    [folders]
    trash = Bin

    [main]
    button_sign_in = Log in

Rules for these files:
  - Save them as UTF-8.
  - Lines starting with ; or # are comments. A ; or # inside a value is
    part of the text.
  - Spaces around the text after = are removed.
  - Keep {placeholders} such as {count} exactly as they are written.
  - A string that depends on a number may have a companion key ending
    in _one, used when the number is exactly 1 (see label_trash_notice
    and label_trash_notice_one in lib/locale/en_us.ini). If you change
    one, change the other.
  - A new locale (for example mods/locale/de_de.ini with no stock file)
    is filled in from English for any key it leaves out.
