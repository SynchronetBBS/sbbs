<!-- $Id$ -->

<!-- Main Content -->  
    
    <td class="main" valign="top"><br />
<center>
<h1>Edit Profile</h1>
    <form action="/members/updateprofile.ssjs" method="post">
	<h3>Telnet Profile</h3>
	<p>Changes here mainly affect the Telnet side of the BBS</p>

		<<REPEAT profile>>
			@@profile:html@@
		<<END REPEAT profile>>

	<h3>Web Profile</h3>
	<p>Changes here are only visible in the Web Interface.</p>
        <table class="newuser" cellpadding="0" cellspacing="2">
            <tr>
                <td class="newuser" align="right">ICQ:</td>
                <td class="newuser" align="left"><input type="text" name="icq" size="10" maxlength="10" value="%%user_icq%%" /></td>
            </tr>
            <tr>
                <td class="newuser" align="right">MSN:</td>
                <td class="newuser" align="left"><input type="text" name="msn" size="50" maxlength="50" value="%%user_msn%%" /></td>
            </tr>
            <tr>
                <td class="newuser" align="right">Yahoo:</td>
                <td class="newuser" align="left"><input type="text" name="yahoo" size="50" maxlength="50" value="%%user_yahoo%%" /></td>
            </tr>
                <td class="newuser" align="right">AIM:</td>
                <td class="newuser" align="left"><input type="text" name="aim" size="50" maxlength="50" value="%%user_aim%%" /></td>
            </tr>			
            <tr>
                <td class="newuser" align="right">Homepage:</td>
                <td class="newuser" align="left"><input type="text" name="homepage" size="50" maxlength="50" value="%%user_homepage%%" /></td>
            </tr>
            <tr>
                <td class="newuser" align="right">Hobbies:</td>
                <td class="newuser" align="left"><input type="textarea" size="50" name="hobbies" value="%%user_hobbies%%" /></td>
            </tr>
			<tr>
                <td class="newuser" align="right">Picture (Link):</td>
                <td class="newuser" align="left"><input type="textarea" size="50" name="picture" value="%%user_picture%%" /></td>
            </tr>
			<tr>
                <td class="newuser" align="right">Avatar (Link):</td>
                <td class="newuser" align="left"><input type="textarea" size="50" name="avatar" value="%%user_avatar%%" /></td>
            </tr>
            <tr>
            <td class="newuser">&nbsp;</td>
            <td class="newuser">
                <br />
                <input type="submit" value="Update Profile" />
            </td>
            </tr>
        </table>
</centre>
    </form>
	<br />

    <!-- end Main Content -->
    
