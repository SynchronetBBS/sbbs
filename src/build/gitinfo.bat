@rem gitinfo.bat -- generate git_hash.h / git_branch.h for the MSVC build.
@rem
@rem A build from a DIRTY tree is marked, because otherwise the stamp describes a
@rem commit the binary isn't:
@rem
@rem   clean:  GIT_HASH "a1009085"    GIT_DATE = that COMMIT's date
@rem   dirty:  GIT_HASH "~a1009085"   GIT_DATE = the BUILD time
@rem
@rem The leading '~' is Vanilla Conquer's convention ("based on this commit, plus
@rem uncommitted changes"). The DATE moves with it on purpose: the commit's
@rem timestamp says nothing about a binary carrying newer, uncommitted code, and
@rem reporting it invites the mistake of thinking a fresh build is an old one.
@rem
@rem Dirtiness is judged from TRACKED changes under the trees that FEED the binary
@rem -- src/ and 3rdp/. 3rdp holds no loose sources, but it tracks the build patches
@rem (3rdp/build), the tarballs they patch (3rdp/dist) and the prebuilt libs, all of
@rem which end up in the binary, so a local change there must mark the build too. Untracked files are ignored (a working
@rem install has hundreds), as is tracked churn elsewhere -- LORD2 rewrites its own
@rem xtrn/lord2/*.dat as people play it, and those bytes are in no binary.
@rem Mirrors ../build/gitinfo.cmake (the doors) and sbbs3's targets.mk. The
@rem dirty-build timestamp comes from PowerShell's Get-Date: a bare `date` here
@rem runs cmd.exe's built-in DATE command (which shadows Git's usr/bin\date.exe,
@rem even when spelled `date.exe`), and it would try to *set* the clock from the
@rem format string, fail, and write its interactive prompt into git_hash.h.
@set GIT_DIRTY=
@for /f "delims=" %%d in ('git status --porcelain -uno -- :/src :/3rdp') do @set GIT_DIRTY=1
@if defined GIT_DIRTY (
	@git log -1 HEAD --format="#define GIT_HASH \"~%%h\"" > git_hash.h
	@for /f "usebackq delims=" %%d in (`powershell -NoProfile -Command "Get-Date -Format 'MMM dd yyyy HH:mm'"`) do @echo #define GIT_DATE "%%d" >> git_hash.h
	@for /f "usebackq delims=" %%t in (`powershell -NoProfile -Command "[DateTimeOffset]::Now.ToUnixTimeSeconds()"`) do @echo #define GIT_TIME %%t >> git_hash.h
) else (
	@git log -1 HEAD --format="#define GIT_HASH \"%%h\"" > git_hash.h
	@git log -1 HEAD --format="#define GIT_DATE \"%%cd\"" "--date=format-local:%%b %%d %%Y %%H:%%M" >> git_hash.h
	@git log -1 HEAD --format="#define GIT_TIME %%cd" --date=unix >> git_hash.h
)
@echo #define GIT_BRANCH ^"| tr -d "\r\n" > git_branch.h
@git rev-parse --abbrev-ref HEAD | tr -d "\n" >> git_branch.h
@echo ^" >> git_branch.h
