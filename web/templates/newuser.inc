<!-- $Id$ -->
<div align="center"><p><a href="http://^^JS:system.inet_addr^^/">Home</a> : <b>New User Application</b></p></div>
<center>
	%%err_message%%
	<form action="newuser.ssjs" method="post">
		<table>
			<tr>
				<td align="right">%%errs:alias%% Alias:</td>
				<td align="left"><input type="text" name="alias" size=25 maxlength=25 value="%%posted:alias%%"></td>
			</tr>
			<tr>
				<td align="right">%%errs:name%% Real Name:</td>
				<td align="left"><input type="text" name="name" size=25 maxlength=25 value="%%posted:name%%"></td>
			</tr>
			<tr>
				<td align="right">%%errs:handle%% Chat Handle:</td>
				<td align="left"><input type="text" name="handle" size=8 maxlength=8 value="%%posted:handle%%"></td>
			</tr>
			<tr>
				<td align="right">%%errs:netmail%% E-Mail Address:</td>
				<td align="left"><input type="text" name="netmail" size=60 maxlength=60 value="%%posted:netmail%%"></td>
			</tr>
			<tr>
				<td align="right">%%errs:address%% Mailing Address:</td>
				<td align="left"><input type="text" name="address" size=30 maxlength=30 value="%%posted:address%%"></td>
			</tr>
			<tr>
				<td align="right">%%errs:location%% City and Province/State:</td>
				<td align="left"><input type="text" name="location" size=30 maxlength=30 value="%%posted:location%%"></td>
			</tr>
			<tr>
				<td align="right">%%errs:zipcode%% Postal/Zip Code:</td>
				<td align="left"><input type="text" name="zipcode" size=10 maxlength=10 value="%%posted:zipcode%%"></td>
			</tr>
			<tr>
				<td align="right">%%errs:phone%% Phone Number (###-###-####):</td>
				<td align="left"><input type="text" name="phone" size=12 maxlength=12 value="%%posted:phone%%"></td>
			</tr>
			<tr>
				<td align="right">%%errs:birthdate%% Birthdate (MM/DD/YY):</td>
				<td align="left"><input type="text" name="birthdate" size=8 maxlength=8 value="%%posted:birthdate%%"></td>
			</tr>
			<tr>
				<td align="right">%%errs:gender%% Gender:</td>
				<td>@@gender_list@@</td>
			</tr>
            <tr>
            <td>&nbsp;</td>
            <td>
            <br />
            <button type="submit">Join!</button>
            </td>
		</table>

	</form>
</center>
