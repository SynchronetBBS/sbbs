
<!-- $Id$ -->

<!-- Main Content --> 
</td>

    <td class="main" valign="top"><br /><br />
    <p>Your password was last changed on <b>@@pwchangedate@@</b><br /><br />
    Please Enter your existing password and well as your new password.  Re-Enter your new password to confirm.<br /><br />
    Your password is currently coverted to UPPERCASE for Legacy compatibility.  Remember this when checking your E-Mail.</p>
    <br /><br />
    <form action="changepw.ssjs" method="post">
      <table border="1" align="center">
      <tr>
        <td>Old Password: </td><td><input type="password" name="oldpass" size="8" maxlength="8" value="" /><br /></td>
      </tr>
      <tr>
        <td>New Password: </td><td><input type="password" name="newpass1" size="8" maxlength="8" value="" />
      Retype New Password: <input type="password" name="newpass2" size="8" maxlength="8" value="" /></td>
      </tr>
      <tr>
        <td colspan="2" align="center"><input type="submit" value="Change Password" /></td>
      </tr>
      </table>   
    </form>
    
