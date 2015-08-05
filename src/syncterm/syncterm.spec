Summary: An ANSI-BBS terminal which supports telnet, rlogin, and SSH
Name: SyncTERM
Version: 1.1b
Release: 20071001
Copyright: GPL
Group: Applications/Communications
Source: http://prdownloads.sourceforge.net/syncterm/syncterm-1.1/syncterm-src-1.1.tgz

%description
An ANSI-BBS terminal designed to connect to remote BBSs via telnet, rlogin, or
SSH. Supports ANSI music and the IBM charset when possible. Will run from a
console, under X11 using XLib, or using SDL.

%prep
%setup -n syncterm-20070722

%build
cd syncterm
gmake PREFIX=/usr RELEASE=1

%install
cd syncterm
gmake install PREFIX=/usr RELEASE=1

%files
/usr/bin/syncterm
/usr/share/icons/hicolor/64x64/apps/syncterm.png
/usr/share/applications/syncterm.desktop
/usr/share/man/man1/syncterm.1.gz

%changelog
* Mon Oct 01 2007 Stephen Hurd <shurd@sasktel.net>
- Initial spec file creation.
