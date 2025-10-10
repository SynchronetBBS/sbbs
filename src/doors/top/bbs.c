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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

This module contains the NODES and PAGE commands, and functions which front-end
the BBS-specific calls.
******************************************************************************/

#include "strwrap.h"
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
	        strcpy((char*)nodeinf.location, (char*)nodedat.location);
            strcpy((char*)nodeinf.statdesc,
				   (char*)top_output(OUT_STRING, getlang((unsigned char *)"NodeStatus")));
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
            itoa(nodeinf.node, (char*)outnum[0], 10);
            top_output(OUT_SCREEN, getlang((unsigned char *)"NodesNodeDisp"), outnum[0]);
            }
        if ((nodeinf.existbits & NEX_HANDLE) && cfg.usehandles)
        	{
            top_output(OUT_SCREEN, getlang((unsigned char *)"NodesHandleDisp"),
                       nodeinf.handle);
            }
        if ((nodeinf.existbits & NEX_REALNAME) && !cfg.usehandles)
            {
            top_output(OUT_SCREEN, getlang((unsigned char *)"NodesRealNameDisp"),
                       nodeinf.realname);
            }
        if (nodeinf.existbits & NEX_SPEED)
        	{
            ltoa(nodeinf.speed, (char*)outnum[0], 10);
            top_output(OUT_SCREEN, getlang((unsigned char *)"NodesSpeedDisp"), outnum[0]);
            }
        if (nodeinf.existbits & NEX_LOCATION)
        	{
            top_output(OUT_SCREEN, getlang((unsigned char *)"NodesLocationDisp"),
                       nodeinf.location);
            }
        if (nodeinf.existbits & NEX_STATUS)
        	{
            top_output(OUT_SCREEN, getlang((unsigned char *)"NodesStatusDisp"),
                       nodeinf.statdesc);
            }
        if (nodeinf.existbits & NEX_PAGESTAT)
            {
            top_output(OUT_SCREEN, getlang((unsigned char *)"NodesPageStatDisp"),
                       nodeinf.quiet ? getlang((unsigned char *)"PageStatNo") :
                       getlang((unsigned char *)"PageStatYes"));
            } // Also add GENDER and NUMCALLS!
        top_output(OUT_SCREEN, getlang((unsigned char *)"NodesListEndLine"));
        count++;
        }
    }

if (endpause)
	{
    // Language-enabled press-a-key later.
//    top_output(OUT_SCREEN, getlang((unsigned char *)"NodesListAfterKey"));
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
        top_output(OUT_SCREEN, getlang((unsigned char *)"NoMemToPage"));
        return;
        }
    }

if (nodenum < 0)
    {
    /* Prompt the user for a node number if none was passed. */
    do
        { // Need to change this back to the old way...
        top_output(OUT_SCREEN, getlang((unsigned char *)"PageWho"));
        od_input_str((char*)pnode, 5, '0', '?');
        if (pnode[0] == '?')
            {
            bbs_useron(FALSE);
            top_output(OUT_SCREEN, (unsigned char *)"@c");
            }
        }
    while(pnode[0] == '?');

    pagenode = atoi((char*)pnode);
    if (!pnode[0])
        {
        top_output(OUT_SCREEN, getlang((unsigned char *)"PageAborted"));
        dofree(pagemsg);
        return;
        }
    itoa(pagenode, (char*)pnode, 10);
    if (pagenode < 1 || pagenode >= cfg.maxnodes)
        {
        itoa(cfg.maxnodes - 1, (char*)outnum[0], 10);
        top_output(OUT_SCREEN, getlang((unsigned char *)"InvalidNode"), outnum[0]);
        dofree(pagemsg);
        return;
        }
    }
else
    {
    pagenode = nodenum;
    itoa(nodenum, (char*)pnode, 10);
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
    top_output(OUT_SCREEN, getlang((unsigned char *)"NodeNotInUse"), pnode);
    dofree(pagemsg);
    return;
	}
if (pnodeinf.quiet)
	{
    top_output(OUT_SCREEN, getlang((unsigned char *)"NodeDND"), pnode);
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
        top_output(OUT_SCREEN, getlang((unsigned char *)"Paged"), pnode);
        od_log_write((char*)top_output(OUT_STRING, getlang((unsigned char *)"LogSentPage"),
                     pnode));
        }
    }
else
    {
    top_output(OUT_SCREEN, getlang((unsigned char *)"CantPage"), pnode);
    od_log_write((char*)top_output(OUT_STRING, getlang((unsigned char *)"LogCantPage"), pnode));
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

top_output(OUT_SCREEN, getlang((unsigned char *)"NodesHdrPrefix"));

/* Show only the desired fields. */
if (ndata->existbits & NEX_NODE)
    {
    top_output(OUT_SCREEN, getlang((unsigned char *)"NodesNodeHdr"));
    }
if ((ndata->existbits & NEX_HANDLE) && cfg.usehandles)
    {
    top_output(OUT_SCREEN, getlang((unsigned char *)"NodesHandleHdr"));
    }
if ((ndata->existbits & NEX_REALNAME) && !cfg.usehandles)
    {
    top_output(OUT_SCREEN, getlang((unsigned char *)"NodesRealNameHdr"));
    }
if (ndata->existbits & NEX_SPEED)
    {
    top_output(OUT_SCREEN, getlang((unsigned char *)"NodesSpeedHdr"));
    }
if (ndata->existbits & NEX_LOCATION)
    {
    top_output(OUT_SCREEN, getlang((unsigned char *)"NodesLocationHdr"));
    }
if (ndata->existbits & NEX_STATUS)
    {
    top_output(OUT_SCREEN, getlang((unsigned char *)"NodesStatusHdr"));
    }
if (ndata->existbits & NEX_PAGESTAT)
    {
    top_output(OUT_SCREEN, getlang((unsigned char *)"NodesPageStatHdr"));
    } // Also add GENDER and NUMCALLS!

top_output(OUT_SCREEN, getlang((unsigned char *)"NodesHdrSuffix"));
top_output(OUT_SCREEN, getlang((unsigned char *)"NodesSepPrefix"));

/* Show separators for the desired fields. */
if (ndata->existbits & NEX_NODE)
    {
    top_output(OUT_SCREEN, getlang((unsigned char *)"NodesNodeSep"));
    }
if ((ndata->existbits & NEX_HANDLE) && cfg.usehandles)
    {
    top_output(OUT_SCREEN, getlang((unsigned char *)"NodesHandleSep"));
    }
if ((ndata->existbits & NEX_REALNAME) && !cfg.usehandles)
    {
    top_output(OUT_SCREEN, getlang((unsigned char *)"NodesRealNameSep"));
    }
if (ndata->existbits & NEX_SPEED)
    {
    top_output(OUT_SCREEN, getlang((unsigned char *)"NodesSpeedSep"));
    }
if (ndata->existbits & NEX_LOCATION)
    {
    top_output(OUT_SCREEN, getlang((unsigned char *)"NodesLocationSep"));
    }
if (ndata->existbits & NEX_STATUS)
    {
    top_output(OUT_SCREEN, getlang((unsigned char *)"NodesStatusSep"));
    }
if (ndata->existbits & NEX_PAGESTAT)
    {
    top_output(OUT_SCREEN, getlang((unsigned char *)"NodesPageStatSep"));
    } // Also add GENDER and NUMCALLS!

top_output(OUT_SCREEN, getlang((unsigned char *)"NodesSepSuffix"));

}
