<!-- $Id$ -->
<CENTER>
	%%err_message%%
	<FORM ACTION="newuser.ssjs" METHOD="POST">
		<TABLE>
			<TR>
				<TD align="right">%%errs:alias%% Alias:</TD>
				<TD align="left"><INPUT TYPE="text" NAME="alias" size=25 MAXLENGTH=25 VALUE="%%posted:alias%%"></TD>
			</TR>
			<TR>
				<TD align="right">%%errs:name%% Real Name:</TD>
				<TD align="left"><INPUT TYPE="text" NAME="name" size=25 MAXLENGTH=25 VALUE="%%posted:name%%"></TD>
			</TR>
			<TR>
				<TD align="right">%%errs:handle%% Chat Handle:</TD>
				<TD align="left"><INPUT TYPE="text" NAME="handle" size=8 MAXLENGTH=8 VALUE="%%posted:handle%%"></TD>
			</TR>
			<TR>
				<TD align="right">%%errs:netmail%% E-Mail Address:</TD>
				<TD align="left"><INPUT TYPE="text" NAME="netmail" size=60 MAXLENGTH=60 VALUE="%%posted:netmail%%"></TD>
			</TR>
			<TR>
				<TD align="right">%%errs:address%% Mailing Address:</TD>
				<TD align="left"><INPUT TYPE="text" NAME="address" size=30 MAXLENGTH=30 VALUE="%%posted:address%%"></TD>
			</TR>
			<TR>
				<TD align="right">%%errs:location%% City and Province/State:</TD>
				<TD align="left"><INPUT TYPE="text" NAME="location" size=30 MAXLENGTH=30 VALUE="%%posted:location%%"></TD>
			</TR>
			<TR>
				<TD align="right">%%errs:zipcode%% Postal/Zip Code:</TD>
				<TD align="left"><INPUT TYPE="text" NAME="zipcode" size=10 MAXLENGTH=10 VALUE="%%posted:zipcode%%"></TD>
			</TR>
			<TR>
				<TD align="right">%%errs:phone%% Phone Number (###-###-####):</TD>
				<TD align="left"><INPUT TYPE="text" NAME="phone" size=12 MAXLENGTH=12 VALUE="%%posted:phone%%"></TD>
			</TR>
			<TR>
				<TD align="right">%%errs:birthdate%% Birthdate (MM/DD/YY):</TD>
				<TD align="left"><INPUT TYPE="text" NAME="birthdate" size=8 MAXLENGTH=8 VALUE="%%posted:birthdate%%"></TD>
			</TR>
			<TR>
				<TD align="right">%%errs:gender%% Gender:</TD>
				<TD>@@gender_list@@</TD>
			</TR>
		</TABLE>
		<INPUT TYPE="SUBMIT">
	</FORM>
</CENTER>
