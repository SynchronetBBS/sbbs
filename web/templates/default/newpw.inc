<!-- $Id: newpw.inc,v 1.5 2006/01/31 23:13:43 runemaster Exp $ -->

<!-- Main Content --> 
</td>

    <td class="main" valign="top"><br /><br />
    <p>Your password was last changed on <b>@@pwchangedate@@</b><br /><br />
    Please enter your existing password as well as your new password.  Re-enter your new password to confirm.<br /><br />
    Your password is case insensitive for <b>most</b> of the BBS servers, but some authentication protocols
    (e.g. POP3-APOP and certain SMTP authentication schemes) will require you specify the password in the proper case. </p>
    <br /><br />
    <form action="changepw.ssjs" method="post">
      <table border="1" align="center">
      <tr>
        <td>Old Password: </td><td><input type="password" name="oldpass" size="8" maxlength="8" value="" /><br /></td>
      </tr>
      <tr>
        <td>New Password: </td><td><input type="password" name="newpass1" size="8" maxlength="8" value="" />
      Retype New Password: <input type="password" name="newpass2" size="8" maxlength="16" value="" /></td>
      </tr>
      <tr>
        <td colspan="2" align="center"><input type="submit" value="Change Password" /></td>
      </tr>
      </table>   
    </form>
    
