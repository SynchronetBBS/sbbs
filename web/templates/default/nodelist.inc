
<!-- $Id$ -->

</td>

    <td class="main" valign="top"><br />
        <br />

    <!-- Main Content -->
     
<!-- start Nodelisting -->

    <table class="main_nodelist"><tbody>
    <tr>
        <th class="main_nodelist">Node</th>
        <th class="main_nodelist">User</th>
        <th class="main_nodelist">Action</th>
        <th class="main_nodelist">Location</th>
        <th class="main_nodelist">Gender</th>
        <th class="main_nodelist">Time</th>  
    </tr>
    <tr>    

<<REPEAT who_online>>
            <tr>
                <td class="main_nodelist" width="1%">@@who_online:node@@</td>  
                <td class="main_nodelist"><a href="mailto:@@who_online:email@@">@@who_online:name@@</a></td>
                <td class="main_nodelist">@@who_online:action@@</td>
                <td class="main_nodelist" width="25%">@@who_online:location@@</td>
                <td class="main_nodelist" width="1%">@@who_online:gender@@</td>    
                <td class="main_nodelist" width="1%">@@who_online:timeon@@</td>     
            </tr>
<<END REPEAT who_online>>
        
        </tbody>
        </table>


<!-- end Main Content-->

