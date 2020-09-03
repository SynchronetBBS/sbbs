<!-- $Id: nodelist.inc,v 1.1 2006/02/22 02:16:17 runemaster Exp $ -->

<!-- Begin Main Content -->

<div id="contentcontainer">
	<div id="content2">

<!-- start Nodelisting -->

<div id="nodelist">
    <table class="nlist" cellpadding="2" cellspacing="1" border="0">
	<tbody>
    	<tr>
        	<th class="nlist2">Node</th>
	        <th class="nlist2">User</th>
	        <th class="nlist2">Action</th>
	        @@JS:if(show_location)'<th class="nlist2">Location</th>'@@
	        @@JS:if(show_age)'<th class="nlist2">Age</th>'@@     
	        @@JS:if(show_gender)'<th class="nlist2">Gender</th>'@@
	        <th class="nlist2">Time</th>  
	    </tr>

		<<REPEAT who_online>>
        <tr>
            <td class="nlist2" width="1%">@@who_online:node@@</td>  
            <td class="nlist2"><a href="mailto:@@who_online:email@@">@@who_online:name@@</a></td>
            <td class="nlist2">@@who_online:action@@</td>
            @@JS:if(show_location)'<td class="nlist2" width="25%">'+Nz(RepeatObj.location)+'</td>'@@
			@@JS:if(show_age)'<td class="nlist2" width="1%">'+Nz(RepeatObj.age)+'</td>'@@
			@@JS:if(show_gender)'<td class="nlist2" width="1%">'+Nz(RepeatObj.gender)+'</td>'@@    
            <td class="nlist2" width="1%">@@who_online:timeon@@</td>     
        </tr>
		<<END REPEAT who_online>>
   	</tbody>
	</table>
	</div>
	</div>
	</div>
	<div class="cleardiv2"></div>
	</div>
</div>

<!-- end Main Content-->
