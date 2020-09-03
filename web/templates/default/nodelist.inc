
<!-- $Id: nodelist.inc,v 1.9 2006/01/31 23:13:43 runemaster Exp $ -->

    <td class="main" valign="top"><br />
        <br />

    <!-- Main Content -->
     
<!-- Start Nodelisting -->

    <table class="main_nodelist"><tbody>
    <tr>
        <th class="main_nodelist">Node</th>
        <th class="main_nodelist">User</th>
        <th class="main_nodelist">Action</th>
        @@JS:if(show_location)'<th class="main_nodelist">Location</th>'@@
        @@JS:if(show_age)'<th class="main_nodelist">Age</th>'@@     
        @@JS:if(show_gender)'<th class="main_nodelist">Gender</th>'@@
        <th class="main_nodelist">Time</th>  
    </tr>

<<REPEAT who_online>>
            <tr>
                <td class="main_nodelist" width="1%">@@who_online:node@@</td>  
                <td class="main_nodelist"><a href="mailto:@@who_online:email@@">@@who_online:name@@</a></td>
                <td class="main_nodelist">@@who_online:action@@</td>
                @@JS:if(show_location)'<td class="main_nodelist" width="25%">'+Nz(RepeatObj.location)+'</td>'@@
				@@JS:if(show_age)'<td class="main_nodelist" width="1%">'+Nz(RepeatObj.age)+'</td>'@@
                @@JS:if(show_gender)'<td class="main_nodelist" width="1%">'+Nz(RepeatObj.gender)+'</td>'@@    
                <td class="main_nodelist" width="1%">@@who_online:timeon@@</td>     
            </tr>
<<END REPEAT who_online>>
        
        </tbody>
        </table>
        <br />

<!-- end Main Content-->

