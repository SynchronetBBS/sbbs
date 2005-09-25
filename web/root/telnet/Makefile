#
# This file is part of "The Java Telnet Applet".
#
# This is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# "The Java Telnet Applet" is distributed in the hope that it will be 
# useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with The JAVA Telnet Applet; see the file COPYING.  If not, write to 
# the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
#
# $Id$

OPT	= 
DEBUG	= -g -deprecation
JAVA	= java
JAVAC	= javac
JAVADOC = javadoc
APPV	= appletviewer

.SUFFIXES: .java .class .html

.java.class:
	$(JAVAC) $(OPT) $(DEBUG) $<

.class.html:	
	$(APPLV) $@

HELP:
	@echo 'To compile "The Java Telnet Applet" do one of the following:'
	@echo '------------------------------------------------------------'
	@echo 'make all          - compile all files to classes'
	@echo 'make test         - compile applet and run appletviewer'
	@echo 'make telnet       - compile standalone and run telnet'
	@echo 'make chartest     - compile character display test and run'
	@echo 'make proxy        - compile proxy server'
	@echo 'make doc          - create documentation'
	@echo 'make clean        - delete backup files'
	@echo 'make realclean    - make clean and delete .class files'

all: appWrapper.class telnet.class modules tools
	@echo All classes created.

test: appWrapper.class telnet.class
	$(APPV) index.test.html

telnet:	appWrapper.class telnet.class
	$(JAVA) telnet localhost	

chartest: CharDisplayTest.class
	$(APPV) CharDisplay.html

doc: 
	$(JAVADOC) -d Documentation/Source -author -version \
		display socket modules \
	        telnet.java appWrapper.java \
                tools/proxy.java CharDisplayTest.java IOtest.java

tar:	
	rm -f ../telnet.tgz ../telnet.zip
	(cd ..; tar cf - Telnet | gzip - > telnet.tgz)
	(cd ..; zip -r telnet.zip Telnet)

bin-tar: all
	rm -f ../classes.tgz ../classes.zip
	(cd ..; tar cf - Telnet/*.class Telnet/*/*.class \
                          Telnet/README Telnet/INSTALL Telnet/COPYING \
		| gzip - > classes.tgz)
	(zip -r ../classes.zip *.class */*.class README INSTALL COPYING)

revision: 
	grep @version *.java */*.java|awk '{split($$6,rev,".");printf("%-26.26s %2.2s.%-2.2s (%s)\n",$$1,rev[1],rev[2],$$7);}' > REVISION
	rcs2log *.java */*.java > CHANGES
	sed -e"s/package:.*$$/package: `date +%c`/" index.html > index.html.x
	mv index.html.x index.html

dist:	realclean all doc tar bin-tar
	@echo DONE.

clean:	
	rm -f *~ */*~

realclean:	clean
	rm -f *.class */*.class
	(cd tools; make clean)

# dependencies

appWrapper.class: \
	appWrapper.java \
	frame.java

telnet.class: \
	telnet.java \
	display \
	socket \
	modules 

# display classes and terminal emulation

display: \
	display/vt320.class

display/vt320.class: \
	display/vt320.java \
	display/CharDisplay.java \
	display/TerminalHost.java \
	display/Terminal.java

display/CharDisplay.java: \
	display/SoftFont.java

# socket io classes
socket: \
	socket/TelnetIO.class \
	socket/TelnetWrapper.class

socket/TelnetIO.class: \
	socket/TelnetIO.java \
	socket/StatusPeer.java

# dynamical loaded modules
modules: modules/ButtonBar.class \
         modules/Script.class \
         modules/TextLabel.class \
         modules/MudConnector.class \
	 modules/BSXModule.class

modules/ButtonBar.class: \
	modules/ButtonBar.java \
	modules/Module.java \
	frame.java

modules/Script.class: \
	modules/Script.java \
	modules/Module.java

modules/MudConnector.class: \
	modules/MudConnector.java \
	modules/Module.java

modules/BSXModule.class: \
	modules/bsx/BSXDisplay.java \
	modules/bsx/BSXGraphic.java \
	modules/bsx/BSXInputStream.java \
	modules/bsx/BSXObject.java \
	modules/bsx/BSXPolygon.java \
	modules/bsx/BSXScene.java

# tools
tools: \
	tools/proxy.class \
	CharDisplayTest.class \
	IOtest.class \
	tools/mrelayd \
	tools/relayd

tools/proxy.class: \
	tools/proxy.java

CharDisplayTest.class: \
	CharDisplayTest.java \
	display/CharDisplay.java

IOtest.class: \
	IOtest.java \
	socket/TelnetIO.java

socket/TelnetWrapper.class: \
	socket/TelnetWrapper.java \
	socket/TelnetIO.java
