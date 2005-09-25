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

OPT	= -O
DEBUG	= -g

.SUFFIXES: .java .class .html

.java.class:
	javac -classpath ../:$(CLASSPATH) $(OPT) $(DEBUG) $<

.class.html:	
	(env CLASSPATH=../:$(CLASSPATH); appletviewer $@)

all: proxy.class 

relay: mrelayd relayd

clean:	
	rm -f *~ 

realclean:	clean
	rm -f *.class relayd mrelayd

# dependencies

proxy.class: proxy.java
redirector.class: proxy.java
