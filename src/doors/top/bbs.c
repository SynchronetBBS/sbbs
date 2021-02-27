/******************************************************************************
BBS.C        Generic BBS functions.

    Copyright 1993 - 2000 Paul J. Sidorsky

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License, version 2, as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

This module contains the NODES and PAGE commands, and functions which front-end
the BBS-specific calls.
******************************************************************************/

#include "top.h"

/* bbs_useron() - Displays who is logged on to the BBS.  (NODES command.)
   Parameters:  endpause - TRUE to pause after displaying the list.
   Returns:  Nothing.
   Notes:  The code to support the endpause flag has been removed so the
           parameter is effectively unused.
*/
void bbs_useron(char endpause)
{
XINT d, count, key, res = 1; /* Counters, keypress holder, result code. */
char nonstop = 0; /* Nonstop mode flag. */
bbsnodedata_typ nodeinf; /* Buffer for BBS data. */
node_idx_typ nodedat; /* Buffer for NODEIDX data. */

/* Set up and display the header. */
// Should be an assertion
if (bbs_call_setexistbits != NULL)
    (*bbs_call_setexistbits)(&nodeinf);
bbs_useronhdr(&nodeinf);

check_nodes_used(FALSE);

for (d = 0, count = 0; d < MAXNODES; d++)
	{
    /* Show a more prompt if 20 nodes have been shown. */
    if ((count % 20 == 0) && !nonstop && count > 0) // Use screenlength!
        {
        nonstop = moreprompt();
        if (nonstop == -1)
            {
            break;
            }
    	}

	memset(&nodeinf, 0, sizeof(bbsnodedata_typ) - 2);
	memset(&nodedat, 0, sizeof(node_idx_typ));
    if (activenodes[d])
    	{
        /* If the node is active in TOP then use the NODEIDX data.  This is
           done because it is faster and more reliable. */
        res = get_node_data(d, &nodedat);
        if (!res)
        	{
            fixname(nodeinf.handle, nodedat.handle);
	        fixname(nodeinf.realname, nodedat.realname);
    	    nodeinf.node = d;
        	nodeinf.speed = nodedat.speed;
	        strcpy(nodeinf.location, nodedat.location);
            strcpy(nodeinf.statdesc,
				   top_output(OUT_STRING, getlang("NodeStatus")));
	        nodeinf.quiet = nodedat.quiet;
    	    nodeinf.gender = nodedat.gender;
        	nodeinf.hidden = 0;
        	}
        }
    else
    	{
        /* Load the data from the BBS's useron file. */
        if (bbs_call_loaduseron)
            res = (*bbs_call_loaduseron)(d, &nodeinf);
        if (res == -2)
        	{
            break;
            }
        }

    /* Display the appropriate information for the node. */
    if (!nodeinf.hidden && !res)
    	{
        if (nodeinf.existbits & NEX_NODE)
        	{
            itoa(nodeinf.node, outnum[0], 10);
            top_output(OUT_SCREEN, getlang("NodesNodeDisp"), outnum[0]);
            }
        if ((nodeinf.existbits & NEX_HANDLE) && cfg.usehandles)
        	{
            top_output(OUT_SCREEN, getlang("NodesHandleDisp"),
                       nodeinf.handle);
            }
        if ((nodeinf.existbits & NEX_REALNAME) && !cfg.usehandles)
            {
            top_output(OUT_SCREEN, getlang("NodesRealNameDisp"),
                       nodeinf.realname);
            }
        if (nodeinf.existbits & NEX_SPEED)
        	{
            ltoa(nodeinf.speed, outnum[0], 10);
            top_output(OUT_SCREEN, getlang("NodesSpeedDisp"), outnum[0]);
            }
        if (nodeinf.existbits & NEX_LOCATION)
        	{
            top_output(OUT_SCREEN, getlang("NodesLocationDisp"),
                       nodeinf.location);
            }
        if (nodeinf.existbits & NEX_STATUS)
        	{
            top_output(OUT_SCREEN, getlang("NodesStatusDisp"),
                       nodeinf.statdesc);
            }
        if (nodeinf.existbits & NEX_PAGESTAT)
            {
            top_output(OUT_SCREEN, getlang("NodesPageStatDisp"),
                       nodeinf.quiet ? getlang("PageStatNo") :
                       getlang("PageStatYes"));
            } // Also add GENDER and NUMCALLS!
        top_output(OUT_SCREEN, getlang("NodesListEndLine"));
        count++;
        }
    }

if (endpause)
	{
    // Language-enabled press-a-key later.
//    top_output(OUT_SCREEN, getlang("NodesListAfterKey"));
    }

}

/* bbs_page() - Sends a message from TOP to a user on the BBS (PAGE cmd.)
   Parameters:  nodenum - Node number to page, or -1 to prompt user.
                pagebuf - String containing the page text, or NULL to use the
                          editor.
   Returns:  Nothing.
*/
void bbs_page(XINT nodenum, unsigned char *pagebuf)
{
/* Final page buffer, string conversion of node number. */
unsigned char *pagemsg = NULL, pnode[6];
XINT pagenode, pres; /* Final node to page, result code. */
bbsnodedata_typ pnodeinf; /* Buffer for BBS data. */
node_idx_typ pnodedat; /* Buffer for NODEIDX data. */

/* Allocate an edit buffer if no string was passed. */
if (pagebuf == NULL)
    {
    pagemsg = malloc(1601);
    if (!pagemsg)
        {
        top_output(OUT_SCREEN, getlang("NoMemToPage"));
        return;
        }
    }

if (nodenum < 0)
    {
    /* Prompt the user for a node number if none was passed. */
    do
        { // Need to change this back to the old way...
        top_output(OUT_SCREEN, getlang("PageWho"));
        od_input_str(pnode, 5, '0', '?');
        if (pnode[0] == '?')
            {
            bbs_useron(FALSE);
            top_output(OUT_SCREEN, "@c");
            }
        }
    while(pnode[0] == '?');

    pagenode = atoi(pnode);
    if (!pnode[0])
        {
        top_output(OUT_SCREEN, getlang("PageAborted"));
        dofree(pagemsg);
        return;
        }
    itoa(pagenode, pnode, 10);
    if (pagenode < 1 || pagenode >= cfg.maxnodes)
        {
        itoa(cfg.maxnodes - 1, outnum[0], 10);
        top_output(OUT_SCREEN, getlang("InvalidNode"), outnum[0]);
        dofree(pagemsg);
        return;
        }
    }
else
    {
    pagenode = nodenum;
    itoa(nodenum, pnode, 10);
    }

pres = 0;
memset(&pnodeinf, 0, sizeof(bbsnodedata_typ) - 2);
memset(&pnodedat, 0, sizeof(node_idx_typ));
if (activenodes[pagenode])
	{
    /* If the user's in TOP, use the NODEIDX data instead. */
    pres = get_node_data(pagenode, &pnodedat);
    pnodeinf.quiet = pnodedat.quiet;
    }
else
	{
    pres = 1;
    /* Load the user's data from the BBS. */
    if (bbs_call_loaduseron)
        pres = (*bbs_call_loaduseron)(pagenode, &pnodeinf);
    if (pres)
    	{
		pnodeinf.hidden = 1;
        }
    }

/* Page is aborted if node is hidden or not in use, or if user is in Quiet
   mode. */
if (pnodeinf.hidden)
	{
    top_output(OUT_SCREEN, getlang("NodeNotInUse"), pnode);
    dofree(pagemsg);
    return;
	}
if (pnodeinf.quiet)
	{
    top_output(OUT_SCREEN, getlang("NodeDND"), pnode);
    dofree(pagemsg);
    return;
    }

if (pagebuf == NULL)
    {
    /* Call the editor if no page was passed. */
    memset(pagemsg, 0, 1601);
    (*bbs_call_pageedit)(pagenode, pagemsg); // use retval later
    }
else
    {
    pagemsg = pagebuf;
    }

/* Performs the actual paging of the user if that function is available. */
if (bbs_call_page != NULL)
    {
    if ((*bbs_call_page)(pagenode, pagemsg))
        {
        top_output(OUT_SCREEN, getlang("Paged"), pnode);
        od_log_write(top_output(OUT_STRING, getlang("LogSentPage"),
                     pnode));
        }
    }
else
    {
    top_output(OUT_SCREEN, getlang("CantPage"), pnode);
    od_log_write(top_output(OUT_STRING, getlang("LogCantPage"), pnode));
    }

// Possible else & case for errorcodes

/* Free the edit buffer if it was used. */
if (pagebuf == NULL)
    {
    dofree(pagemsg);
    }

}

/* bbs_useronhdr() - Shows the header for the NODES command.
   Parameters:  ndata - Pointer to the BBS node data being used.
   Returns:  Nothing.
   Notes:  Only the existbits are used from ndata.  The buffer pointed to
           by ndata should first be initialized using a call to the function
           pointed to by bbs_call_setexistbits.
*/
void bbs_useronhdr(bbsnodedata_typ *ndata)
{

top_output(OUT_SCREEN, getlang("NodesHdrPrefix"));

/* Show only the desired fields. */
if (ndata->existbits & NEX_NODE)
    {
    top_output(OUT_SCREEN, getlang("NodesNodeHdr"));
    }
if ((ndata->existbits & NEX_HANDLE) && cfg.usehandles)
    {
    top_output(OUT_SCREEN, getlang("NodesHandleHdr"));
    }
if ((ndata->existbits & NEX_REALNAME) && !cfg.usehandles)
    {
    top_output(OUT_SCREEN, getlang("NodesRealNameHdr"));
    }
if (ndata->existbits & NEX_SPEED)
    {
    top_output(OUT_SCREEN, getlang("NodesSpeedHdr"));
    }
if (ndata->existbits & NEX_LOCATION)
    {
    top_output(OUT_SCREEN, getlang("NodesLocationHdr"));
    }
if (ndata->existbits & NEX_STATUS)
    {
    top_output(OUT_SCREEN, getlang("NodesStatusHdr"));
    }
if (ndata->existbits & NEX_PAGESTAT)
    {
    top_output(OUT_SCREEN, getlang("NodesPageStatHdr"));
    } // Also add GENDER and NUMCALLS!

top_output(OUT_SCREEN, getlang("NodesHdrSuffix"));
top_output(OUT_SCREEN, getlang("NodesSepPrefix"));

/* Show separators for the desired fields. */
if (ndata->existbits & NEX_NODE)
    {
    top_output(OUT_SCREEN, getlang("NodesNodeSep"));
    }
if ((ndata->existbits & NEX_HANDLE) && cfg.usehandles)
    {
    top_output(OUT_SCREEN, getlang("NodesHandleSep"));
    }
if ((ndata->existbits & NEX_REALNAME) && !cfg.usehandles)
    {
    top_output(OUT_SCREEN, getlang("NodesRealNameSep"));
    }
if (ndata->existbits & NEX_SPEED)
    {
    top_output(OUT_SCREEN, getlang("NodesSpeedSep"));
    }
if (ndata->existbits & NEX_LOCATION)
    {
    top_output(OUT_SCREEN, getlang("NodesLocationSep"));
    }
if (ndata->existbits & NEX_STATUS)
    {
    top_output(OUT_SCREEN, getlang("NodesStatusSep"));
    }
if (ndata->existbits & NEX_PAGESTAT)
    {
    top_output(OUT_SCREEN, getlang("NodesPageStatSep"));
    } // Also add GENDER and NUMCALLS!

top_output(OUT_SCREEN, getlang("NodesSepSuffix"));

}
