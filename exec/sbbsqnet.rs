; Synchronet QWK Network Robocom Script
;
; User account must have 'Q' restriction on host system
;
; If path to Synchronet data dir is incorrect, change it

TITLE "Synchronet QWK Network Packet Exchange"

WHEN "Hit a key" SEND "^M"                      ; if pause, hit enter
WHEN "No new messages" GOTO END			; if no new messages

WAITFOR "NN:" FAILURE GOTO END                  ; wait for NN: prompt
SEND "%BBS13%^M"                                ; send login name
WAITFOR "PW:" FAILURE GOTO END                  ; wait for PW: prompt
SEND "%BBS7%^M"                                 ; send password
IF NOT EXIST "%REPDIR%%ID%.REP" GOTO DOWNLOAD
WAITFOR "QWK: " FAILURE GOTO END                ; QWK prompt
SEND "U"                                        ; Upload
WAITFOR "Protocol" FAILURE GOTO END
SEND "ZN"					; Zmodem, don't hang up
UPLOAD "%REPDIR%%ID%.REP"
ERASE "%REPDIR%%ID%.REP"
:DOWNLOAD
WAITFOR "QWK: " FAILURE GOTO END                ; QWK prompt
SEND "DL"                                      	; Download, leave codes in
TIMEOUT 300
WAITFOR "Protocol" FAILURE GOTO END
SEND "ZN"					; Zmodem, don't hang up
DOWNLOAD "%QWKDIR%%ID%.QWK"

:END
SEND "Q"                                        ; Logoff
DELAY 5
HANGUP
EXIT

