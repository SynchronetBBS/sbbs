<!doctype HTML public "-//W30//DTD W3 HTML 3.0//EN">
<HTML>
<HEAD><TITLE>@@system:name@@</TITLE></HEAD>
<BODY BGCOLOR="#ffffff">
<HR>
<CENTER>
<TABLE>
<TR ALIGN=CENTER><TD COLSPAN=2>
<!-- Here begins the applet code -->
<APPLET ARCHIVE="classes.zip" CODE="appWrapper.class" WIDTH=800 HEIGHT=600>
  <PARAM NAME=applet      VALUE="telnet">

  <!-- optional (WIDTH and HEIGHT should be changed!) -->
  <!-- PARAM NAME="startButton" VALUE="Connect to www.first.gmd.de!" -->
  <!-- PARAM NAME="stopButton" VALUE="Shutdown telnet!" -->
  <!-- PARAM NAME="frameTitle" VALUE="The Java Telnet Applet: WWW" -->

  <!-- applet initialization: address and port -->
  <PARAM NAME=address     VALUE="^^system:inet_addr^^">
  <PARAM NAME=port        VALUE="23">
  <PARAM NAME=emulation   VALUE="vt320">

  <!-- terminal emulation -->
  <PARAM NAME=VTscrollbar VALUE="right">
  <PARAM NAME=VTresize    VALUE="font">
  <PARAM NAME=VTfont      VALUE="Courier">
  <PARAM NAME=VTfontsize  VALUE="13">
  <PARAM NAME=VTid        VALUE="vt220">
  <PARAM NAME=VTcharset   VALUE="ibm">

  <!-- modules: #1 is a buttonbar -->
  <PARAM NAME=module#1    VALUE="ButtonBar@North">
  <PARAM NAME=1#Button    VALUE="connect|\$connect()">
  <PARAM NAME=2#Button    VALUE="disconnect|\$disconnect()">
  <PARAM NAME=3#Button    VALUE="Detach/Delete Window|\$detach()">

  <!-- modules: #2 is a scripting module (disabled!) -->
  <!-- PARAM NAME=module#2    VALUE="Script" -->
  <!-- PARAM NAME=script      VALUE="login:|leo" -->


  <!-- make sure, non-java-capable browser get a message: -->
  <B>
  Your Browser seems to have no <A HREF="http://java.sun.com/">Java</A>
  support. Please get a new browser or enable Java to see this applet!
  </B>
</APPLET>
<!-- End of applet code -->
</TABLE>
</CENTER>
<HR>
</BODY>
</HTML>
