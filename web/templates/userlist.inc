<CENTER>
<TABLE>
<TR bgcolor=white><TH><font color=black>Alias</font></TH><TH><font color=black>Location</font></TH><TH><font color=black>Connection</font></TH><TH><font color=black>Last On</font></TH></TR>
<<REPEAT users>>
<TR>
  <TD>
    @@users:alias@@
  </TD>
  <TD>
    @@users:location@@
  </TD>
  <TD>
    @@users:connection@@
  </TD>
  <TD>
    @@users:logon@@
  </TD>
</TR>
<<END REPEAT users>>
</TABLE>
</CENTER>
