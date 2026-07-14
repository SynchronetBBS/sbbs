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
@rem Dirtiness is judged from TRACKED changes under src/ only: untracked files
@rem would make every tree dirty (a working install has hundreds), and tracked
@rem churn outside src/ -- game data a door rewrites at runtime -- has nothing to
@rem do with what got compiled. Mirrors ../build/gitinfo.cmake (the doors) and
@rem sbbs3's targets.mk. `date` comes from the same Git-for-Windows usr/bin that
@rem already supplies the `tr` below.
@set GIT_DIRTY=
@for /f "delims=" %%d in ('git status --porcelain -uno -- :/src') do @set GIT_DIRTY=1
@if defined GIT_DIRTY (
	@git log -1 HEAD --format="#define GIT_HASH \"~%%h\"" > git_hash.h
	@date "+#define GIT_DATE \"%%b %%d %%Y %%H:%%M\"" >> git_hash.h
	@date "+#define GIT_TIME %%s" >> git_hash.h
) else (
	@git log -1 HEAD --format="#define GIT_HASH \"%%h\"" > git_hash.h
	@git log -1 HEAD --format="#define GIT_DATE \"%%cd\"" "--date=format-local:%%b %%d %%Y %%H:%%M" >> git_hash.h
	@git log -1 HEAD --format="#define GIT_TIME %%cd" --date=unix >> git_hash.h
)
@echo #define GIT_BRANCH ^"| tr -d "\r\n" > git_branch.h
@git rev-parse --abbrev-ref HEAD | tr -d "\n" >> git_branch.h
@echo ^" >> git_branch.h
