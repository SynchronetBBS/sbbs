<!-- $Id: choosegroup.inc,v 1.1 2006/02/22 02:16:41 runemaster Exp $ -->

<!-- Main Content -->  
    
<div id="contentcontainer">
	<div id="content2">
    <div id="grplist">
	<div align="center">
<table class="grouplist" border="0" cellpadding="2" cellspacing="1">
<tr>
<th class="grouplist2">Message Group</th><th class="grouplist2">Subs</th>
</tr>
<<REPEAT groups>>
<tr>
    <td class="grouplist2"><a class="grouplist2" href="choosesubs.ssjs?msg_grp=^^groups:name^^">%%groups:description%%</a></td>
    <td class="grouplist2r">@@JS:msg_area.grp_list[RepeatObj.index].sub_list.length@@</td>
</tr>
<<END REPEAT groups>>
</table>
		</div>
		</div>
		</div>
		</div>
	</div>
	<div class="cleardiv2"></div>
	</div>
</div>

<!-- end Main Content -->


