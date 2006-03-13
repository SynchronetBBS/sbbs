#include <gtk/gtk.h>
#include <glade/glade.h>

#include "sbbs.h"
#include "dirwrap.h"

#include "gtkuseredit.h"

/* 
 * This is one of the two big gruntwork functions
 * (the other being save_user)
 */
void load_user(GtkWidget *wiggy, gpointer data)
{
	GtkWidget	*w;
	char		str[1024];
	gboolean	b;
	int			i;
	FILE		*f;

	user.number=current_user;
	if(user.number < 1 || user.number > totalusers) {
		fprintf(stderr,"Attempted to load illegal user number %d.\n",user.number);
		return;
	}
	if(getuserdat(&cfg, &user)) {
		fprintf(stderr,"Error loading user %d.\n",current_user);
		return;
	}

	/* Toolbar indicators */
		b=user.misc&DELETED?TRUE:FALSE;
		w=glade_xml_get_widget(xml, "bDelete");
		if(w==NULL)
			fprintf(stderr,"Cannot get the deleted toolbar widget\n");
		else
			gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(w),b);
		w=glade_xml_get_widget(xml, "delete1");
		if(w==NULL)
			fprintf(stderr,"Cannot get the deleted menu widget\n");
		else
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),b);

		b=user.misc&INACTIVE?TRUE:FALSE;
		w=glade_xml_get_widget(xml, "bRemove");
		if(w==NULL)
			fprintf(stderr,"Cannot get the removed toolbar widget\n");
		else
			gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(w),b);
		w=glade_xml_get_widget(xml, "remove1");
		if(w==NULL)
			fprintf(stderr,"Cannot get the remove menu widget\n");
		else
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),b);
		

	/* Peronal Tab */
		/* Alias */
		w=glade_xml_get_widget(xml, "eUserAlias");
		if(w==NULL)
			fprintf(stderr,"Cannot get the alias widget\n");
		else
			gtk_entry_set_text(GTK_ENTRY(w),user.alias);

		/* Real Name */
		w=glade_xml_get_widget(xml, "eRealName");
		if(w==NULL)
			fprintf(stderr,"Cannot get the real name widget\n");
		else
			gtk_entry_set_text(GTK_ENTRY(w),user.name);

		/* Computer */
		w=glade_xml_get_widget(xml, "eComputer");
		if(w==NULL)
			fprintf(stderr,"Cannot get the computer widget\n");
		else
			gtk_entry_set_text(GTK_ENTRY(w),user.comp);

		/* NetMail */
		w=glade_xml_get_widget(xml, "eNetMail");
		if(w==NULL)
			fprintf(stderr,"Cannot get the netmail widget\n");
		else
			gtk_entry_set_text(GTK_ENTRY(w),user.netmail);

		/* Phone */
		w=glade_xml_get_widget(xml, "ePhone");
		if(w==NULL)
			fprintf(stderr,"Cannot get the phone widget\n");
		else
			gtk_entry_set_text(GTK_ENTRY(w),user.phone);

		/* Note */
		w=glade_xml_get_widget(xml, "eNote");
		if(w==NULL)
			fprintf(stderr,"Cannot get the note widget\n");
		else
			gtk_entry_set_text(GTK_ENTRY(w),user.note);

		/* Comment */
		w=glade_xml_get_widget(xml, "eComment");
		if(w==NULL)
			fprintf(stderr,"Cannot get the comment widget\n");
		else
			gtk_entry_set_text(GTK_ENTRY(w),user.comment);

		/* Gender */
		w=glade_xml_get_widget(xml, "eGender");
		if(w==NULL)
			fprintf(stderr,"Cannot get the gender widget\n");
		else {
			str[0]=user.sex;
			str[1]=0;
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}

		/* Connection */
		w=glade_xml_get_widget(xml, "eConnection");
		if(w==NULL)
			fprintf(stderr,"Cannot get the connection widget\n");
		else
			gtk_entry_set_text(GTK_ENTRY(w),user.modem);

		/* Chat Handle */
		w=glade_xml_get_widget(xml, "eHandle");
		if(w==NULL)
			fprintf(stderr,"Cannot get the handle widget\n");
		else
			gtk_entry_set_text(GTK_ENTRY(w),user.handle);

		/* Birthdate */
		w=glade_xml_get_widget(xml, "eBirthdate");
		if(w==NULL)
			fprintf(stderr,"Cannot get the birthdate widget\n");
		else
			gtk_entry_set_text(GTK_ENTRY(w),user.birth);

		/* Password */
		w=glade_xml_get_widget(xml, "ePassword");
		if(w==NULL)
			fprintf(stderr,"Cannot get the password widget\n");
		else
			gtk_entry_set_text(GTK_ENTRY(w),user.pass);

		/* Address */
		w=glade_xml_get_widget(xml, "eAddress");
		if(w==NULL)
			fprintf(stderr,"Cannot get the address widget\n");
		else
			gtk_entry_set_text(GTK_ENTRY(w),user.address);

		/* Location */
		w=glade_xml_get_widget(xml, "eLocation");
		if(w==NULL)
			fprintf(stderr,"Cannot get the location widget\n");
		else
			gtk_entry_set_text(GTK_ENTRY(w),user.location);

		/* Postal/ZIP code */
		w=glade_xml_get_widget(xml, "eZip");
		if(w==NULL)
			fprintf(stderr,"Cannot get the postal/zip code widget\n");
		else
			gtk_entry_set_text(GTK_ENTRY(w),user.zipcode);

		/* Postal/ZIP code */
		w=glade_xml_get_widget(xml, "eZip");
		if(w==NULL)
			fprintf(stderr,"Cannot get the postal/zip code widget\n");
		else
			gtk_entry_set_text(GTK_ENTRY(w),user.zipcode);

	/* Security Tab */
		/* Level */
		w=glade_xml_get_widget(xml, "eLevel");
		if(w==NULL)
			fprintf(stderr,"Cannot get the level widget\n");
		else {
			sprintf(str,"%u",user.level);
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}

		/* Expiration */
		w=glade_xml_get_widget(xml, "eExpiration");
		if(w==NULL)
			fprintf(stderr,"Cannot get the expiration widget\n");
		else {
			unixtodstr(&cfg, user.expire, str);
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}

		/* Flag Sets */
		w=glade_xml_get_widget(xml, "eFlagSet1");
		if(w==NULL)
			fprintf(stderr,"Cannot get the flag set 1 widget\n");
		else {
			ltoaf(user.flags1, str);
			truncsp(str);
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}
		w=glade_xml_get_widget(xml, "eFlagSet2");
		if(w==NULL)
			fprintf(stderr,"Cannot get the flag set 2 widget\n");
		else {
			ltoaf(user.flags2, str);
			truncsp(str);
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}
		w=glade_xml_get_widget(xml, "eFlagSet3");
		if(w==NULL)
			fprintf(stderr,"Cannot get the flag set 3 widget\n");
		else {
			ltoaf(user.flags3, str);
			truncsp(str);
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}
		w=glade_xml_get_widget(xml, "eFlagSet4");
		if(w==NULL)
			fprintf(stderr,"Cannot get the flag set 4 widget\n");
		else {
			ltoaf(user.flags4, str);
			truncsp(str);
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}

		/* Exemptions */
		w=glade_xml_get_widget(xml, "eExemptions");
		if(w==NULL)
			fprintf(stderr,"Cannot get the exemptions widget\n");
		else {
			ltoaf(user.exempt, str);
			truncsp(str);
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}

		/* Restrictions */
		w=glade_xml_get_widget(xml, "eRestrictions");
		if(w==NULL)
			fprintf(stderr,"Cannot get the restrictions widget\n");
		else {
			ltoaf(user.rest, str);
			truncsp(str);
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}

		/* Credits */
		w=glade_xml_get_widget(xml, "eCredits");
		if(w==NULL)
			fprintf(stderr,"Cannot get the credits widget\n");
		else {
			sprintf(str,"%u",user.cdt);
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}

		/* Free Credits */
		w=glade_xml_get_widget(xml, "eFreeCredits");
		if(w==NULL)
			fprintf(stderr,"Cannot get the free credits widget\n");
		else {
			sprintf(str,"%u",user.freecdt);
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}

		/* Minutes */
		w=glade_xml_get_widget(xml, "eMinutes");
		if(w==NULL)
			fprintf(stderr,"Cannot get the minutes widget\n");
		else {
			sprintf(str,"%u",user.min);
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}

	/* Statistics */
		/* First On */
		w=glade_xml_get_widget(xml, "eFirstOn");
		if(w==NULL)
			fprintf(stderr,"Cannot get the first on widget\n");
		else {
			unixtodstr(&cfg, user.firston, str);
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}

		/* Last On */
		w=glade_xml_get_widget(xml, "eLastOn");
		if(w==NULL)
			fprintf(stderr,"Cannot get the last on widget\n");
		else {
			unixtodstr(&cfg, user.laston, str);
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}

		/* First On */
		w=glade_xml_get_widget(xml, "eFirstOn");
		if(w==NULL)
			fprintf(stderr,"Cannot get the first on widget\n");
		else {
			unixtodstr(&cfg, user.firston, str);
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}

		/* Total Logons */
		w=glade_xml_get_widget(xml, "eLogonsTotal");
		if(w==NULL)
			fprintf(stderr,"Cannot get the total logons widget\n");
		else {
			sprintf(str, "%hu", user.logons);
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}

		/* Logons today */
		w=glade_xml_get_widget(xml, "eLogonsToday");
		if(w==NULL)
			fprintf(stderr,"Cannot get the logons today widget\n");
		else {
			sprintf(str, "%hu", user.ltoday);
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}

		/* Total Posts */
		w=glade_xml_get_widget(xml, "eTotalPosts");
		if(w==NULL)
			fprintf(stderr,"Cannot get the total posts widget\n");
		else {
			sprintf(str, "%hu", user.posts);
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}

		/* Posts Today */
		w=glade_xml_get_widget(xml, "ePostsToday");
		if(w==NULL)
			fprintf(stderr,"Cannot get the posts today widget\n");
		else {
			sprintf(str, "%hu", user.ptoday);
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}

		/* Total Uploads */
		w=glade_xml_get_widget(xml, "eTotalUploads");
		if(w==NULL)
			fprintf(stderr,"Cannot get the total uploads widget\n");
		else {
			sprintf(str, "%hu", user.uls);
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}

		/* Upload Bytes */
		w=glade_xml_get_widget(xml, "eUploadBytes");
		if(w==NULL)
			fprintf(stderr,"Cannot get the upload bytes widget\n");
		else {
			sprintf(str, "%u", user.ulb);
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}

		/* Total Time On */
		w=glade_xml_get_widget(xml, "eTotalTimeOn");
		if(w==NULL)
			fprintf(stderr,"Cannot get the total time on widget\n");
		else {
			sprintf(str, "%hu", user.timeon);
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}

		/* Time On Today */
		w=glade_xml_get_widget(xml, "eTimeOnToday");
		if(w==NULL)
			fprintf(stderr,"Cannot get the time on today widget\n");
		else {
			sprintf(str, "%hu", user.ttoday);
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}

		/* Time On Last Call */
		w=glade_xml_get_widget(xml, "eTimeOnLastCall");
		if(w==NULL)
			fprintf(stderr,"Cannot get the last call time on widget\n");
		else {
			sprintf(str, "%hu", user.tlast);
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}

		/* Time On Extra */
		w=glade_xml_get_widget(xml, "eTimeOnExtra");
		if(w==NULL)
			fprintf(stderr,"Cannot get the extra time on widget\n");
		else {
			sprintf(str, "%hu", user.textra);
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}

		/* Total Downloads */
		w=glade_xml_get_widget(xml, "eDownloadsTotal");
		if(w==NULL)
			fprintf(stderr,"Cannot get the total downloads widget\n");
		else {
			sprintf(str, "%hu", user.dls);
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}

		/* Download Bytes */
		w=glade_xml_get_widget(xml, "eDownloadsBytes");
		if(w==NULL)
			fprintf(stderr,"Cannot get the download bytes widget\n");
		else {
			sprintf(str, "%u", user.dlb);
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}

		/* Download Leeches */
		w=glade_xml_get_widget(xml, "eDownloadsLeech");
		if(w==NULL)
			fprintf(stderr,"Cannot get the downloads leech widget\n");
		else {
			sprintf(str, "%hhu", user.leech);
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}

		/* Total Email */
		w=glade_xml_get_widget(xml, "eEmailTotal");
		if(w==NULL)
			fprintf(stderr,"Cannot get the total email widget\n");
		else {
			sprintf(str, "%hu", user.emails);
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}

		/* Email Today */
		w=glade_xml_get_widget(xml, "eEmailToday");
		if(w==NULL)
			fprintf(stderr,"Cannot get the email today widget\n");
		else {
			sprintf(str, "%hu", user.etoday);
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}

		/* Email To Sysop */
		w=glade_xml_get_widget(xml, "eEmailToSysop");
		if(w==NULL)
			fprintf(stderr,"Cannot get the email to sysop widget\n");
		else {
			sprintf(str, "%hu", user.fbacks);
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}

	/* Settings */
		w=glade_xml_get_widget(xml, "cUserAUTOTERM");
		if(w==NULL)
			fprintf(stderr,"Cannot get the autoterm widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.misc&AUTOTERM);

		w=glade_xml_get_widget(xml, "cUserNO_EXASCII");
		if(w==NULL)
			fprintf(stderr,"Cannot get the no exascii widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.misc&NO_EXASCII);

		w=glade_xml_get_widget(xml, "cUserANSI");
		if(w==NULL)
			fprintf(stderr,"Cannot get the ansi widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.misc&ANSI);

		w=glade_xml_get_widget(xml, "cUserCOLOR");
		if(w==NULL)
			fprintf(stderr,"Cannot get the color widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.misc&COLOR);

		w=glade_xml_get_widget(xml, "cUserRIP");
		if(w==NULL)
			fprintf(stderr,"Cannot get the RIP widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.misc&RIP);

		w=glade_xml_get_widget(xml, "cUserWIP");
		if(w==NULL)
			fprintf(stderr,"Cannot get the WIP widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.misc&WIP);

		w=glade_xml_get_widget(xml, "cUserUPAUSE");
		if(w==NULL)
			fprintf(stderr,"Cannot get the upause widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.misc&UPAUSE);

		w=glade_xml_get_widget(xml, "cUserCOLDKEYS");
		if(w==NULL)
			fprintf(stderr,"Cannot get the coldkeys widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.misc&COLDKEYS);

		w=glade_xml_get_widget(xml, "cUserSPIN");
		if(w==NULL)
			fprintf(stderr,"Cannot get the spin widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.misc&SPIN);

		w=glade_xml_get_widget(xml, "cUserRIP");
		if(w==NULL)
			fprintf(stderr,"Cannot get the RIP widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.misc&RIP);

		w=glade_xml_get_widget(xml, "eRows");
		if(w==NULL)
			fprintf(stderr,"Cannot get the rows widget\n");
		else {
			sprintf(str, "%hhu", user.rows);
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}

		w=glade_xml_get_widget(xml, "cCommandShell");
		if(w==NULL)
			fprintf(stderr,"Cannot get the command shell widget\n");
		else
			gtk_combo_box_set_active(GTK_COMBO_BOX(w),user.shell);

		w=glade_xml_get_widget(xml, "cUserEXPERT");
		if(w==NULL)
			fprintf(stderr,"Cannot get the expert widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.misc&EXPERT);

		w=glade_xml_get_widget(xml, "cUserASK_NSCAN");
		if(w==NULL)
			fprintf(stderr,"Cannot get the ask new scan widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.misc&ASK_NSCAN);

		w=glade_xml_get_widget(xml, "cUserASK_SSCAN");
		if(w==NULL)
			fprintf(stderr,"Cannot get the ask to you scan widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.misc&ASK_SSCAN);

		w=glade_xml_get_widget(xml, "cUserCURSUB");
		if(w==NULL)
			fprintf(stderr,"Cannot get the save current sub widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.misc&CURSUB);

		w=glade_xml_get_widget(xml, "cUserQUIET");
		if(w==NULL)
			fprintf(stderr,"Cannot get the quiet mode widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.misc&QUIET);

		w=glade_xml_get_widget(xml, "cUserAUTOLOGON");
		if(w==NULL)
			fprintf(stderr,"Cannot get the auto logon widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.misc&AUTOLOGON);

		w=glade_xml_get_widget(xml, "cUserEXPERT");
		if(w==NULL)
			fprintf(stderr,"Cannot get the expert widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.misc&EXPERT);

		w=glade_xml_get_widget(xml, "cUserCHAT_ECHO");
		if(w==NULL)
			fprintf(stderr,"Cannot get the chat echo widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.chat&CHAT_ECHO);

		w=glade_xml_get_widget(xml, "cUserCHAT_ACTION");
		if(w==NULL)
			fprintf(stderr,"Cannot get the chat action widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.chat&CHAT_ACTION);

		w=glade_xml_get_widget(xml, "cUserCHAT_NOPAGE");
		if(w==NULL)
			fprintf(stderr,"Cannot get the chat nopage widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.chat&CHAT_NOPAGE);

		w=glade_xml_get_widget(xml, "cUserCHAT_NOACT");
		if(w==NULL)
			fprintf(stderr,"Cannot get the chat no activity widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.chat&CHAT_NOACT);

		w=glade_xml_get_widget(xml, "cUserCHAT_SPLITP");
		if(w==NULL)
			fprintf(stderr,"Cannot get the chat split personal widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.chat&CHAT_SPLITP);

	/* Msg/File Settings */

		w=glade_xml_get_widget(xml, "cUserNETMAIL");
		if(w==NULL)
			fprintf(stderr,"Cannot get the netmail widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.misc&NETMAIL);

		w=glade_xml_get_widget(xml, "cUserCLRSCRN");
		if(w==NULL)
			fprintf(stderr,"Cannot get the clear screen widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.misc&CLRSCRN);

		w=glade_xml_get_widget(xml, "cUserANFSCAN");
		if(w==NULL)
			fprintf(stderr,"Cannot get the ask new file scan widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.misc&ANFSCAN);

		w=glade_xml_get_widget(xml, "cUserEXTDESC");
		if(w==NULL)
			fprintf(stderr,"Cannot get the extended descriptions widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.misc&EXTDESC);

		w=glade_xml_get_widget(xml, "cUserBATCHFLAG");
		if(w==NULL)
			fprintf(stderr,"Cannot get the batch flagging widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.misc&BATCHFLAG);

		w=glade_xml_get_widget(xml, "cUserAUTOHANG");
		if(w==NULL)
			fprintf(stderr,"Cannot get the auto hangup after transfer widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.misc&AUTOHANG);

		w=glade_xml_get_widget(xml, "cUserCLRSCRN");
		if(w==NULL)
			fprintf(stderr,"Cannot get the clear screen widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.misc&CLRSCRN);

		w=glade_xml_get_widget(xml, "cExternalEditor");
		if(w==NULL)
			fprintf(stderr,"Cannot get the external editor widget\n");
		else
			gtk_combo_box_set_active(GTK_COMBO_BOX(w),user.xedit);

		w=glade_xml_get_widget(xml, "cDefaultDownloadProtocol");
		if(w==NULL)
			fprintf(stderr,"Cannot get the default download protocol widget\n");
		else {
			gtk_combo_box_set_active(GTK_COMBO_BOX(w),-1);
			for(i=0;i<cfg.total_prots;i++) {
				if(cfg.prot[i]->mnemonic==user.prot) {
					gtk_combo_box_set_active(GTK_COMBO_BOX(w),i);
					break;
				}
			}
		}

		w=glade_xml_get_widget(xml, "cTempQWKFileType");
		if(w==NULL)
			fprintf(stderr,"Cannot get the temp QWK file type widget\n");
		else {
			gtk_combo_box_set_active(GTK_COMBO_BOX(w),-1);
			for(i=0;i<cfg.total_fcomps;i++) {
				if(!stricmp(cfg.fcomp[i]->ext,user.tmpext)) {
					gtk_combo_box_set_active(GTK_COMBO_BOX(w),i);
					break;
				}
			}
		}

		w=glade_xml_get_widget(xml, "cUserQWK_FILES");
		if(w==NULL)
			fprintf(stderr,"Cannot get the include new files list widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.qwk&QWK_FILES);

		w=glade_xml_get_widget(xml, "cUserQWK_EMAIL");
		if(w==NULL)
			fprintf(stderr,"Cannot get the include unread email widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.qwk&QWK_EMAIL);

		w=glade_xml_get_widget(xml, "cUserQWK_ALLMAIL");
		if(w==NULL)
			fprintf(stderr,"Cannot get the include all email widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.qwk&QWK_ALLMAIL);

		w=glade_xml_get_widget(xml, "cUserQWK_DELMAIL");
		if(w==NULL)
			fprintf(stderr,"Cannot get the delete email widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.qwk&QWK_DELMAIL);

		w=glade_xml_get_widget(xml, "cUserQWK_BYSELF");
		if(w==NULL)
			fprintf(stderr,"Cannot get the include messages by self widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.qwk&QWK_BYSELF);

		w=glade_xml_get_widget(xml, "cUserQWK_EXPCTLA");
		if(w==NULL)
			fprintf(stderr,"Cannot get the expand ctrl-a widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.qwk&QWK_EXPCTLA);

		w=glade_xml_get_widget(xml, "cUserQWK_RETCTLA");
		if(w==NULL)
			fprintf(stderr,"Cannot get the retail ctrl-a widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.qwk&QWK_RETCTLA);

		w=glade_xml_get_widget(xml, "cUserQWK_ATTACH");
		if(w==NULL)
			fprintf(stderr,"Cannot get the include attachments widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.qwk&QWK_ATTACH);

		w=glade_xml_get_widget(xml, "cUserQWK_NOINDEX");
		if(w==NULL)
			fprintf(stderr,"Cannot get the don't include index files widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.qwk&QWK_NOINDEX);

		w=glade_xml_get_widget(xml, "cUserQWK_TZ");
		if(w==NULL)
			fprintf(stderr,"Cannot get the include TZ widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.qwk&QWK_TZ);

		w=glade_xml_get_widget(xml, "cUserQWK_VIA");
		if(w==NULL)
			fprintf(stderr,"Cannot get the include VIA widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.qwk&QWK_VIA);

		w=glade_xml_get_widget(xml, "cUserQWK_NOCTRL");
		if(w==NULL)
			fprintf(stderr,"Cannot get the include extraneous control files widget\n");
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),user.qwk&QWK_NOCTRL);

	/* Extended Comment */
		sprintf(str,"%suser/%4.4u.msg", cfg.data_dir,user.number);
		f=fopen(str,"r");
		if(f) {
			w=glade_xml_get_widget(xml, "tExtendedComment");
			while((i=fread(str,1,sizeof(str),f)))
				gtk_text_buffer_insert_at_cursor(gtk_text_view_get_buffer(GTK_TEXT_VIEW(w)), str,i);
			fclose(f);
		}
}

int update_current_user(int new_user)
{
	char	str[11];
	GtkWidget	*eCurrentUser;

	if(new_user<1 || new_user>totalusers)
		new_user=current_user;
	sprintf(str,"%d",new_user);
	eCurrentUser=glade_xml_get_widget(xml, "eCurrentUser");
	if(eCurrentUser==NULL) {
		fprintf(stderr,"Cannot get the current user widget\n");
		return(-1);
	}
	if(strcmp(gtk_entry_get_text(GTK_ENTRY(eCurrentUser)),str))
		gtk_entry_set_text(GTK_ENTRY(eCurrentUser), str);
	if(new_user!=current_user) {
		current_user=new_user;
		load_user(eCurrentUser,NULL);
	}
	return(0);
}

void current_user_changed(GtkWidget *w, gpointer data)
{
	int new_user;

	new_user=atoi(gtk_entry_get_text(GTK_ENTRY(w)));
	update_current_user(new_user);
}

void first_user(GtkWidget *w, gpointer data)
{
	update_current_user(1);
}

void prev_user(GtkWidget *w, gpointer data)
{
	update_current_user(current_user-1);
}

void next_user(GtkWidget *w, gpointer data)
{
	update_current_user(current_user+1);
}

void last_user(GtkWidget *w, gpointer data)
{
	update_current_user(totalusers);
}

void show_about_box(GtkWidget *unused, gpointer data)
{
	GtkWidget	*w;
	w=glade_xml_get_widget(xml, "AboutWindow");
	if(w==NULL) {
		fprintf(stderr,"Cannot get about window widget\n");
		return;
	}
	gtk_widget_show(GTK_WIDGET(w));
}

void user_toggle_delete(GtkWidget *t, gpointer data)
{
	gboolean	deleted;
	GtkWidget	*w;

	g_object_get(G_OBJECT(t), "active", &deleted, NULL);

	w=glade_xml_get_widget(xml, "bDelete");
	if(w==NULL)
		fprintf(stderr,"Cannot get the deleted toolbar widget\n");
	else
		gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(w),deleted);
	w=glade_xml_get_widget(xml, "delete1");
	if(w==NULL)
		fprintf(stderr,"Cannot get the deleted menu widget\n");
	else
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),deleted);
}

void user_toggle_inactive(GtkWidget *t, gpointer data)
{
	gboolean	inactive;
	GtkWidget	*w;

	g_object_get(G_OBJECT(t), "active", &inactive, NULL);

	w=glade_xml_get_widget(xml, "bRemove");
	if(w==NULL)
		fprintf(stderr,"Cannot get the removed toolbar widget\n");
	else
		gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(w),inactive);
	w=glade_xml_get_widget(xml, "remove1");
	if(w==NULL)
		fprintf(stderr,"Cannot get the remove menu widget\n");
	else
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),inactive);
}

void find_user(GtkWidget *t, gpointer data)
{
	GtkWidget	*w;
	unsigned int	nu;

	w=glade_xml_get_widget(xml, "eMatchUser");
	if(w==NULL)
		fprintf(stderr,"Cannot get the find user entry widget\n");
	else {
		nu=matchuser(&cfg, gtk_entry_get_text(GTK_ENTRY(w)), TRUE);
		if(nu==0) {
			w=glade_xml_get_widget(xml, "NotFoundWindow");
			if(w==NULL)
				fprintf(stderr,"Could not locate NotFoundWindow widget\n");
			else
				gtk_widget_show(GTK_WIDGET(w));
		}
		else
			update_current_user(nu);
	}
}

void close_notfoundwindow(GtkWidget *t, gpointer data)
{
	GtkWidget	*w;

	w=glade_xml_get_widget(xml, "NotFoundWindow");
	if(w==NULL)
		fprintf(stderr,"Could not locate NotFoundWindow widget\n");
	else
		gtk_widget_hide(GTK_WIDGET(w));
}
