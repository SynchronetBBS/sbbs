
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
        <th class="main_nodelist">Loction</th>
        <th class="main_nodelist">Gender</th>
        <th class="main_nodelist">Time</th>  
    </tr>
    <tr>    

<<REPEAT node_list>>
            <tr>
                <td class="main_nodelist" width="1%">@@node_list:node@@</td>  
                <td class="main_nodelist"><a href="mailto:">@@node_list:email@@</a>@@node_list:email@@</td>
                <td class="main_nodelist">@@node_list:action@@</td>
                <td class="main_nodelist" width="25%">@@node_list:location@@</td>
                <td class="main_nodelist" width="1%">@@node_list:gender@@</td>    
                <td class="main_nodelist" width="1%">@@node_list:timeon@@</td>     
            </tr>
<<END REPEAT node_list>>
        
        </tbody>
        </table>


<!-- end Main Content-->

