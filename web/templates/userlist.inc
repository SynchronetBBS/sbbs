<CENTER>
<TABLE>
<TR bgcolor=white><TH><a href="userlist.ssjs?sort=alias"><font color=black>Alias</font></a></TH><TH><a href="userlist.ssjs?sort=location"><font color=black>Location</font></a></TH><TH><a href="userlist.ssjs?sort=connection"><font color=black>Connection</font></a></TH><TH><a href="userlist.ssjs?sort=laston"><font color=black>Last On</font></a></TH></TR>
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
