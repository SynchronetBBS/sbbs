<!-- $Id: newuser.inc,v 1.14 2007/09/22 08:43:30 rswindell Exp $ -->

<!-- Main Content -->

<td class="main" valign="top" >    
<center>
    (<b>%%required%%</b> indicates required fields)<br /><br />
    <font size="+1" color="red"><b>%%err_message%%</b></font>
</center>
    <form action="newuser.ssjs" method="post">
        <table class="newuser" cellpadding="0" cellspacing="2">
            @@nupstart@@
            <tr>
                <td class="newuser" align="right">%%errs:nupass%% %%nupass_required%% New User Password:</td>
                <td class="newuser" align="left"><input type="password" name="nupass" size="25" maxlength="25" value="" /></td>
            </tr>
            @@nupend@@
            <tr>
                <td class="newuser" align="right">%%errs:alias%% %%alias_required%% Alias:</td>
                <td class="newuser" align="left"><input type="text" name="alias" size="25" maxlength="25" value="%%posted:alias%%" /></td>
            </tr>
            <tr>
                <td class="newuser" align="right">%%errs:name%% %%name_required%% %%RealNameIs%%:</td>
                <td class="newuser" align="left"><input type="text" name="name" size="25" maxlength="25" value="%%posted:name%%" /></td>
            </tr>
            <tr>
                <td class="newuser" align="right">%%errs:handle%% %%handle_required%% Chat Handle:</td>
                <td class="newuser" align="left"><input type="text" name="handle" size="8" maxlength="8" value="%%posted:handle%%" /></td>
            </tr>
            <tr>
                <td class="newuser" align="right">%%errs:netmail%% %%email_required%% E-Mail Address:</td>
                <td class="newuser" align="left"><input type="text" name="netmail" size="60" maxlength="60" value="%%posted:netmail%%" /></td>
            </tr>
            <tr>
                <td class="newuser" align="right">%%errs:address%% %%addr_required%% Mailing Address:</td>
                <td class="newuser" align="left"><input type="text" name="address" size="30" maxlength="30" value="%%posted:address%%" /></td>
            </tr>
            <tr>
                <td class="newuser" align="right">%%errs:location%% %%loc_required%% City and Province/State:</td>
                <td class="newuser" align="left"><input type="text" name="location" size="30" maxlength="30" value="%%posted:location%%" /></td>
            </tr>
            <tr>
                <td class="newuser" align="right">%%errs:zipcode%%  %%zip_required%% Postal/Zip Code:</td>
                <td class="newuser" align="left"><input type="text" name="zipcode" size="10" maxlength="10" value="%%posted:zipcode%%" /></td>
            </tr>
            <tr>
                <td class="newuser" align="right">%%errs:phone%% %%ph_required%% Phone Number (###-###-####):</td>
                <td class="newuser" align="left"><input type="text" name="phone" size="12" maxlength="12" value="%%posted:phone%%" /></td>
            </tr>
            <tr>
                <td class="newuser" align="right">%%errs:birthdate%% %%bd_required%% Birthdate (%%date_format%%):</td>
                <td class="newuser" align="left"><input type="text" name="birthdate" size="8" maxlength="8" value="%%posted:birthdate%%" /></td>
            </tr>
            <tr>
                <td class="newuser" align="right">%%errs:gender%% %%sex_required%% Gender:</td>
                <td class="newuser">@@gender_list@@</td>
            </tr>
            <tr>
                <td class="newuser" align="right">%%errs:editor%% Message Editor:</td>
                <td class="newuser">@@editor_list@@</td>
            </tr>
            <tr>
            <td class="newuser">&nbsp;</td>
            <td class="newuser">
                <br />
                <input type="image" src="@@image_dir@@/join.gif" value="Join!" />
            </td>
            </tr>
        </table>
    </form>

    <!-- end Main Content -->
    
