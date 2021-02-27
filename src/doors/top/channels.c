/******************************************************************************
CHANNELS.C   Channel auxilliary functions.

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

This module implements the auxilliary functions related to conferences and
channels, such as loading CHANNELS.CFG and displaying the channels list.  The
heavy-duty work of actually managing the channels is performed by the Channel
Management Interface (CMI), which is wholly contained in CMI.C.
******************************************************************************/

#include "top.h"

/* loadchan() - Loads channel and conference definitions from CHANNELS.CFG.
   Parameters:  None.
   Returns:  TRUE on success, FALSE if an error occurred.
*/
char loadchan(void)
{
FILE *chanfil = NULL; /* File stream used to load cfg file. */
unsigned long curch = 0; /* Current channel number. */
unsigned long nextconf = 4001000000UL; /* Number for the next conference. */
/* Pointer to the start of the options for the current setting. */
unsigned char *opt = NULL;
unsigned char loadstr[256]; /* Input buffer when loading the file. */
long chcount = -1; /* Number of channel definitions. */
XINT cres; /* Result code. */

/* Open the configuration file. */
sprintf(outbuf, "%schannels.cfg", cfg.toppath);
chanfil = fopen(outbuf, "rt");
if (!chanfil)
    {
    return 0;
    }

/* Read and process each line in the file. */
while (!feof(chanfil))
    {
    /* Read the next line. */
    if (!fgets(loadstr, 256, chanfil))
        {
        /* Abort if an error occurs while reading.  Usually this means an
           EOF in the middle of a line. */
        break;
        }
    /* Skip commented lines. */
    if (loadstr[0] == ';')
        {
        continue;
        }
    /* Strip newline.  Not sure why I didn't use the stripcr() macro here. */
    if (loadstr[strlen(loadstr) - 1] == '\n')
        {
        loadstr[strlen(loadstr) - 1] = 0;
        }
    /* Break the string into words. */
    cres = split_string(loadstr);

    if (cres > 1)
        {
        /* Only process if there are at least two words. */

        /* Grab the position of the start of the second word and use this
           as the options string. */
        opt = &word_str[word_pos[1]];

        /* Channel number or type. */
        if (!stricmp(get_word(0), "Channel"))
            {
            if (!stricmp(opt, "Conference"))
                {
                /* Channel is a conference, use the next internal conference
                   number. */
                curch = nextconf++;
                }
            else
                {
                /* Get the channel number. */
                curch = strtoul(opt, NULL, 10);
                if (curch > 3999999999UL)
                    {
                    /* Unlike many other areas of TOP the maximum limit
                       here is actually checked and enforced. */
                    curch = 3999999999UL;
                    }
                }
            /* Set the channel number for the current channel definition. */
            chan[++chcount].channel = curch;
            continue;
            }
        /* Fixed channel topic. */
        if (!stricmp(get_word(0), "Topic"))
            {
            memset(chan[chcount].topic, 0, 71);
            strncpy(chan[chcount].topic, opt, 70);
            continue;
            }
        /* Alternate names for the channel or conference. */
        if (!stricmp(get_word(0), "JoinAliases"))
            {
            memset(chan[chcount].joinaliases, 0, 31);
            strncpy(chan[chcount].joinaliases, opt, 30);
            continue;
            }
        /* Minimum security to join the channel. */
        if (!stricmp(get_word(0), "MinSecurity"))
            {
            chan[chcount].minsec = atol(opt);
            }
        /* Maximum security to join the channel. */
        if (!stricmp(get_word(0), "MaxSecurity"))
            {
            chan[chcount].maxsec = atol(opt);
            }
        /* Minimum security for automatic moderator status. */
        if (!stricmp(get_word(0), "ModeratorSecurity"))
            {
            chan[chcount].modsec = atol(opt);
            }
        /* Show this channel in a SCAN? */
        if (!stricmp(get_word(0), "Listed"))
            {
            chan[chcount].listed = seektruth(opt);
            }
        }
    }

fclose(chanfil);

/* Replace the configured "expected" maximum with the actual amount of
   definitions, so TOP doesn't get confused with blank ones. */
cfg.maxchandefs = ++chcount;

return 1;
}

/* list_channels() - Display a list of channels and conferences.
   Parameters:  None.
   Returns:  Nothing.
*/
void list_channels(void)
{
long chc; /* Counter. */

/* Prepare the screen. */
top_output(OUT_SCREEN, getlang("ChannelListHdr"));
top_output(OUT_SCREEN, getlang("ChannelListSep"));

/* Display each definition. */
for (chc = 0; chc < cfg.maxchandefs; chc++)
    {
    if (!chan[chc].listed || user.security < chan[chc].minsec ||
        user.security > chan[chc].maxsec)
        {
        /* Don't show if the user's security isn't in range, or if the
           channel is flagged unlisted. */
        continue;
        }

    /* Show the channel. */
    top_output(OUT_SCREEN, getlang("ChannelListFormat"),
               channelname(chan[chc].channel), chan[chc].topic);
    }

}

/* channelname() - Get the short name of a channel.
   Parameters:  chnum - Channel number to get the name of.
   Returns:  String containing the name.
   Notes:  Normally used in a SCAN command or channel listing to get a
           displayable name.  Always returns a pointer to the global
           variable wordret, which is used as an output buffer.
*/
unsigned char *channelname(unsigned long chnum)
{
XINT cd; /* Counter. */
long cdef; /* Channel definition number for channel #chnum. */

/* Clear the buffer. */
memset(wordret, 0, 31);

/* Conference. */
if (chnum > 4000999999UL && chnum < 0xFFFFFFFFUL)
    {
    /* Find the channel definition for this channel.  It is assumed that
       one will exist because this function is only used during output. */
    cdef = findchannelrec(chnum);
    // Should assert the definition's existence.

    /* Change underscores to spaces inside the name.  The intention was to
       allow multi-word channels to be defined with underscores.
       Unfortunately, the current command processor does not support this. */
    for (cd = 0; chan[cdef].joinaliases[cd] != ' ' &&
         cd < strlen(chan[cdef].joinaliases); cd++)
        {
        wordret[cd] = chan[cdef].joinaliases[cd];
        if (wordret[cd] == '_')
            {
            wordret[cd] = ' ';
            }
        }
    }
/* Personal channel. */
if (chnum >= 4000000000UL && chnum <= 4000999999UL)
    {
    /* The short name of a personal channel is simply the owner's handle. */
    strcpy(wordret, top_output(OUT_STRINGNF, getlang("UserChanShortName"),
           handles[(XINT) (chnum - 4000000000UL)].string));
    }
/* Normal (numeric) channel. */
if (chnum < 4000000000UL)
    {
    /* Simply returns the channel number as a string. */
    ultoa(chnum, wordret, 10);
    }

return wordret;
}

/* checkchannelname() - Gets the channel number for a name or alias.
   Parameters:  chnam - Name or alias of channel to check.
   Returns:  Channel number for the name given, or 0 if none is found.
*/
unsigned long checkchannelname(unsigned char *chnam)
{
XINT cc; /* Counter. */

/* Check each channel definition. */
for (cc = 0; cc < cfg.maxchandefs; cc++)
    {
    /* Find a match for all aliases. */
    if (checkcmdmatch(chnam, chan[cc].joinaliases) > 0)
        {
        return chan[cc].channel;
        }
    }

/* No match could be found. */
return 0L;
}

/* findchannelrec() - Finds the channel definition number for a channel.
   Parameters:  fchan - Channel number to find a definition for.
   Returns:  Channel definition number for the given channel, or -1 if
             none can be found.
*/
long findchannelrec(unsigned long fchan)
{
long fc; /* Counter. */

/* Check each channel definition. */
for (fc = 0; fc < cfg.maxchandefs; fc++)
    {
    /* A match occurs if the defined channel matches the given one. */
    if (fchan == chan[fc].channel)
        {
        return fc;
        }
    }

/* No match could be found. */
return -1L;
}
