/* Copyright 2004 by Darryl Perry
 * 
 * This file is part of Space Dynasty Elite.
 * 
 * Space Dynasty Elite is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Foobar is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Foobar; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <string.h>
#include <OpenDoor.h>

void sde_input_str(char *sStr, int iMaxlen, unsigned char hichar, unsigned char lochar)
{
	int i,ok;
	char ch=0,prevchar,dispch;

	od_printf("%-*s",iMaxlen,sStr);
	for(i=iMaxlen;i>strlen(sStr);i--)
		od_printf("\b");
	i=strlen(sStr);
	do{
		ok=FALSE;
		prevchar=ch;
		ch=od_get_key(TRUE);
		dispch=ch;

		if(ch=='\b' || ch==127){
			if(i>0){
				od_disp_str("\b \b");
				i--;
				prevchar=*sStr;
				dispch='\0';
				}
			ok=FALSE;
			}
		else{
			if(ch>=hichar && ch<=lochar){
				prevchar=sStr[i];
				if(ch!='\n' && ch!='\r' && ch!='\x1b' && ch!='\b' && ch!=127)
					sStr[i]=ch;
				else{
					sStr[i]='\0';
					}
				od_printf("%c",sStr[i]);
				i++;
				}
			}
	}while(ch!='\r' && ch != '\n' && i<=iMaxlen);
	od_clear_keybuffer();

	sStr[i]='\0';
}
