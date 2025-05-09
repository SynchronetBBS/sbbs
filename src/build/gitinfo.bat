@git log -1 HEAD --format="#define GIT_HASH \"%%h\"" > git_hash.h
@git log -1 HEAD --format="#define GIT_DATE \"%%cd\"" "--date=format-local:%%b %%d %%Y %%H:%%M" >> git_hash.h
@git log -1 HEAD --format="#define GIT_TIME %%cd" --date=unix >> git_hash.h
@echo #define GIT_BRANCH ^"| tr -d "\r\n" > git_branch.h
@git rev-parse --abbrev-ref HEAD | tr -d "\n" >> git_branch.h
@echo ^" >> git_branch.h
