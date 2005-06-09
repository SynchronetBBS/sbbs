/******************************************************************************
NODECFG.C    Node configuration file processor.

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

This module contains the code to load and process NODES.CFG.
******************************************************************************/

#include "top.h"

/* loadnodecfg() - Loads and processes NODES.CFG.
   Parameters:  exepath - Path (including trailing \) to TOP.EXE.
   Returns:  TRUE on success, FALSE on error.
*/
XINT loadnodecfg(unsigned char *exepath)
    {
    unsigned char loadstr[256 + 1]; /* Line input buffer. */
    XINT code, flag = 0; /* Node code, finished flag. */
    FILE *fil = NULL; /* File stream for loading NODES.CFG. */

    /* Open the configuration file. */
    strcat(exepath, "nodes.cfg");
    fil = fopen(exepath, "rt");
    if (fil == NULL)
        {
        return 0;
        }

    // Need err on fgets()

    /* Loop until end of file or this node's configuration is found. */
    while(!feof(fil) && !flag)
        {
        /* Read a line and split it into keyword and options. */
        fgets(loadstr, 256, fil);
        stripcr(loadstr);
        code = splitnodecfgline(loadstr);
        /* Look for a "Node" line in the configuration file. */
        if (code == NODECODE_NUMBER)
            {
            /* If the line bears our node's number, we can start
               processing. */
            if (atoi(loadstr) == od_control.od_node)
                {
                flag = 1;
                }
            }
        }

    /* Node configuration not found. */
    if (!flag)
        {
        fclose(fil);
        return 0;
        }

    /* Initialize variables. */
    memset(nodecfg, 0, sizeof(TOP_nodecfg_typ));
    flag = 0;

    /* Loop until end of file or until processing is done. */
    while(!feof(fil) && !flag)
        {
        /* Read a line and split it into keyword and options. */
        fgets(loadstr, 256, fil);
        stripcr(loadstr);
        code = splitnodecfgline(loadstr);
        /* React to each type of configuration line.  See the
           splitnodecfgline() function or TOPCFG.H to see which node code
           corresponds to which line. */
        switch(code)
            {
            case NODECODE_TYPE:
                strupr(loadstr);
                if (strstr(loadstr, "REMOTE")) nodecfg->type = NODE_REMOTE;
                if (strstr(loadstr, "LOCAL")) nodecfg->type = NODE_LOCAL;
                if (strstr(loadstr, "LAN")) nodecfg->type = NODE_LAN;
                break;
            case NODECODE_DROPFILE:
                /* strupr(loadstr); */
                strcpy(nodecfg->dropfilepath, loadstr);
                verifypath(nodecfg->dropfilepath);
                break;
            case NODECODE_CFGFILE:
                /* strupr(loadstr); */
                strcpy(nodecfg->cfgfile, loadstr);
                break;
            case NODECODE_LOGFILE:
                /* strupr(loadstr); */
                strcpy(nodecfg->logfile, loadstr);
                break;
            case NODECODE_SOLID:
                nodecfg->solidnode = seektruth(loadstr);
                break;
            case NODECODE_FOSSIL:
                nodecfg->fossil = seektruth(loadstr);
                break;
            case NODECODE_PORT:
                nodecfg->port = atoi(loadstr);
                break;
            case NODECODE_SPEED:
                nodecfg->speed = atoi(loadstr);
                break;
            case NODECODE_ADDRESS:
                nodecfg->portaddress = atoi(loadstr);
                break;
            case NODECODE_IRQ:
                nodecfg->portirq = atoi(loadstr);
                break;
            case NODECODE_FIFO:
                nodecfg->usefifo = seektruth(loadstr);
                break;
            case NODECODE_FIFOTRIG:
                nodecfg->fifotrigger = atoi(loadstr);
                break;
            case NODECODE_RXBUF:
                nodecfg->rxbufsize = atoi(loadstr);
                break;
            case NODECODE_TXBUF:
                nodecfg->txbufsize = atoi(loadstr);
                break;
            case NODECODE_RTSCTS:
                nodecfg->usertscts = !(seektruth(loadstr)) + 1;
                break;
            case NODECODE_END:
                /* Flag so the loop will stop processing. */
                flag = 1;
                break;
            }
        }

    fclose(fil);

    return 1;
    }

/* splitnodecfgline() - Splits a NODES.CFG line into keyword and options.
   Parameters:  ncline - Line to split.
   Returns:  Node code value (see NODECODE_ constants in TOPCFG.H).
   Notes:  See NODES.CFG for details about each keyword.
*/
XINT splitnodecfgline(unsigned char *ncline)
    {
    XINT wdcount; /* Word count holder. */
    XINT nodecode = NODECODE_UNKNOWN; /* Node code. */

    if (ncline[0] == ';')
        {
        return NODECODE_COMMENT; /* Comment. */
        }

    /* Split the line into words. */
    wdcount = split_string(ncline);

    if (wdcount > 1)
        {
        /* Grab everything after the first word from the word string. */
        strcpy(ncline, &word_str[word_pos[1]]);
        }
    else
        {
        /* No second word available. */
        ncline[0] = '\0';
        }

    /* Set the node code based on the keyword type.  Again, see NODES.CFG
       comments for keyword descriptions. */
    if (!stricmp(get_word(0), "Node")) nodecode = NODECODE_NUMBER;
    if (!stricmp(get_word(0), "Type")) nodecode = NODECODE_TYPE;
    if (!stricmp(get_word(0), "DropFilePath")) nodecode = NODECODE_DROPFILE;
    if (!stricmp(get_word(0), "ConfigFile")) nodecode = NODECODE_CFGFILE;
    if (!stricmp(get_word(0), "LogFile")) nodecode = NODECODE_LOGFILE;
    if (!stricmp(get_word(0), "SolidNode")) nodecode = NODECODE_SOLID;
    if (!stricmp(get_word(0), "FOSSIL")) nodecode = NODECODE_FOSSIL;
    if (!stricmp(get_word(0), "Port")) nodecode = NODECODE_PORT;
    if (!stricmp(get_word(0), "Speed")) nodecode = NODECODE_SPEED;
    if (!stricmp(get_word(0), "PortAddress")) nodecode = NODECODE_ADDRESS;
    if (!stricmp(get_word(0), "PortIRQ")) nodecode = NODECODE_IRQ;
    if (!stricmp(get_word(0), "UseFIFO")) nodecode = NODECODE_FIFO;
    if (!stricmp(get_word(0), "FIFOTrigger")) nodecode = NODECODE_FIFOTRIG;
    if (!stricmp(get_word(0), "RxBufSize")) nodecode = NODECODE_RXBUF;
    if (!stricmp(get_word(0), "TxBufSize")) nodecode = NODECODE_TXBUF;
    if (!stricmp(get_word(0), "UseRTSCTS")) nodecode = NODECODE_RTSCTS;
    if (!stricmp(get_word(0), "EndNode")) nodecode = NODECODE_END;

    return nodecode;
    }

