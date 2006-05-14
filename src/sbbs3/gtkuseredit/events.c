#include <gtk/gtk.h>
#include <glade/glade.h>

#include "sbbs.h"
#include "dirwrap.h"
#include "xpbeep.h"
#include "datewrap.h"
#include "semwrap.h"

#include "gtkuseredit.h"

/*
 * Sets the save buttons sensitive.
 */
void user_changed(GtkWidget *wiggy, gpointer data)
{
	GtkWidget	*w;

	w=glade_xml_get_widget(xml, "bSaveUser");
	if(w==NULL)
		fprintf(stderr,"Cannot get the save user button widget\n");
	else
		gtk_widget_set_sensitive(GTK_WIDGET(w),TRUE);
	w=glade_xml_get_widget(xml, "save1");
	if(w==NULL)
		fprintf(stderr,"Cannot get the save user menu widget\n");
	else
		gtk_widget_set_sensitive(GTK_WIDGET(w),TRUE);
}

/*
 * Prevents anything but digits from being entered into an input box
 */
void digit_insert_text_handler (GtkEntry *entry, const gchar *text, gint length, gint *position, gpointer data)
{
	GtkEditable *editable = GTK_EDITABLE(entry);
	int i, count=0, beep=0;
	gchar *result = g_new (gchar, length);

	for (i=0; i < length; i++) {
		if (!isdigit(text[i])) {
			beep=1;
			continue;
		}
		result[count++] = text[i];
	}

	if (count > 0) {
		g_signal_handlers_block_by_func (G_OBJECT (editable)
			,G_CALLBACK (digit_insert_text_handler)
			,data);
		gtk_editable_insert_text (editable, result, count, position);
		g_signal_handlers_unblock_by_func (G_OBJECT (editable)
			,G_CALLBACK (digit_insert_text_handler)
			,data);
	}
	if(beep)
		BEEP(440,100);

	g_signal_stop_emission_by_name (G_OBJECT (editable), "insert_text");

	g_free (result);
}

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
	GtkTextIter	start;
	GtkTextIter	end;

	if(current_user != 0) {
		user.number=current_user;
		if(user.number < 1 || user.number > totalusers) {
			fprintf(stderr,"Attempted to load illegal user number %d.\n",user.number);
			return;
		}
		if(getuserdat(&cfg, &user)) {
			fprintf(stderr,"Error loading user %d.\n",current_user);
			return;
		}
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

	/* Security Tab */
		/* Level */
		w=glade_xml_get_widget(xml, "sLevel");
		if(w==NULL)
			fprintf(stderr,"Cannot get the level widget\n");
		else
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(w),user.level);

		/* Expiration */
		w=glade_xml_get_widget(xml, "eExpiration");
		if(w==NULL)
			fprintf(stderr,"Cannot get the expiration widget\n");
		else {
			if(user.expire)
				unixtodstr(&cfg, user.expire, str);
			else
				strcpy(str,"Never");
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}

		/* Flag Sets */
		strcpy(str,"tFlagSet1.");
		for(i=0;i<26;i++) {
			str[9]='A'+i;
			w=glade_xml_get_widget(xml, str);
			if(w==NULL)
				fprintf(stderr,"Cannot get the %s widget\n",str);
			else {
				b = (user.flags1 & (1L << i)) != 0;
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),b);
			}
		}

		strcpy(str,"tFlagSet2.");
		for(i=0;i<26;i++) {
			str[9]='A'+i;
			w=glade_xml_get_widget(xml, str);
			if(w==NULL)
				fprintf(stderr,"Cannot get the %s widget\n",str);
			else {
				b = (user.flags2 & (1L << i)) != 0;
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),b);
			}
		}

		strcpy(str,"tFlagSet3.");
		for(i=0;i<26;i++) {
			str[9]='A'+i;
			w=glade_xml_get_widget(xml, str);
			if(w==NULL)
				fprintf(stderr,"Cannot get the %s widget\n",str);
			else {
				b = (user.flags3 & (1L << i)) != 0;
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),b);
			}
		}

		strcpy(str,"tFlagSet4.");
		for(i=0;i<26;i++) {
			str[9]='A'+i;
			w=glade_xml_get_widget(xml, str);
			if(w==NULL)
				fprintf(stderr,"Cannot get the %s widget\n",str);
			else {
				b = (user.flags4 & (1L << i)) != 0;
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),b);
			}
		}

		/* Exemptions */
		strcpy(str,"tExemptions.");
		for(i=0;i<26;i++) {
			str[11]='A'+i;
			w=glade_xml_get_widget(xml, str);
			if(w==NULL)
				fprintf(stderr,"Cannot get the %s widget\n",str);
			else {
				b = (user.exempt & (1L << i)) != 0;
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),b);
			}
		}

		/* Restrictions */
		strcpy(str,"tRestrictions.");
		for(i=0;i<26;i++) {
			str[13]='A'+i;
			w=glade_xml_get_widget(xml, str);
			if(w==NULL)
				fprintf(stderr,"Cannot get the %s widget\n",str);
			else {
				b = (user.rest & (1L << i)) != 0;
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),b);
			}
		}

		/* Credits */
		w=glade_xml_get_widget(xml, "sCredits");
		if(w==NULL)
			fprintf(stderr,"Cannot get the credits widget\n");
		else
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(w),user.cdt);

		/* Free Credits */
		w=glade_xml_get_widget(xml, "sFreeCredits");
		if(w==NULL)
			fprintf(stderr,"Cannot get the free credits widget\n");
		else
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(w),user.freecdt);

		/* Minutes */
		w=glade_xml_get_widget(xml, "sMinutes");
		if(w==NULL)
			fprintf(stderr,"Cannot get the minutes widget\n");
		else
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(w),user.min);

	/* Statistics */
		/* First On */
		w=glade_xml_get_widget(xml, "eFirstOn");
		if(w==NULL)
			fprintf(stderr,"Cannot get the first on widget\n");
		else {
			if(user.firston)
				unixtodstr(&cfg, user.firston, str);
			else
				strcpy(str,"Never");
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}

		/* Last On */
		w=glade_xml_get_widget(xml, "eLastOn");
		if(w==NULL)
			fprintf(stderr,"Cannot get the last on widget\n");
		else {
			if(user.laston)
				unixtodstr(&cfg, user.laston, str);
			else
				strcpy(str,"Never");
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}

		/* Total Logons */
		w=glade_xml_get_widget(xml, "sLogonsTotal");
		if(w==NULL)
			fprintf(stderr,"Cannot get the total logons widget\n");
		else
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(w),user.logons);

		/* Logons today */
		w=glade_xml_get_widget(xml, "sLogonsToday");
		if(w==NULL)
			fprintf(stderr,"Cannot get the logons today widget\n");
		else
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(w),user.ltoday);

		/* Total Posts */
		w=glade_xml_get_widget(xml, "sTotalPosts");
		if(w==NULL)
			fprintf(stderr,"Cannot get the total posts widget\n");
		else
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(w),user.posts);

		/* Posts Today */
		w=glade_xml_get_widget(xml, "sPostsToday");
		if(w==NULL)
			fprintf(stderr,"Cannot get the posts today widget\n");
		else
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(w),user.ptoday);

		/* Total Uploads */
		w=glade_xml_get_widget(xml, "sTotalUploads");
		if(w==NULL)
			fprintf(stderr,"Cannot get the total uploads widget\n");
		else
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(w),user.uls);

		/* Upload Bytes */
		w=glade_xml_get_widget(xml, "sUploadBytes");
		if(w==NULL)
			fprintf(stderr,"Cannot get the upload bytes widget\n");
		else
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(w),user.ulb);

		/* Total Time On */
		w=glade_xml_get_widget(xml, "sTotalTimeOn");
		if(w==NULL)
			fprintf(stderr,"Cannot get the total time on widget\n");
		else
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(w),user.timeon);

		/* Time On Today */
		w=glade_xml_get_widget(xml, "sTimeOnToday");
		if(w==NULL)
			fprintf(stderr,"Cannot get the time on today widget\n");
		else
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(w),user.ttoday);

		/* Time On Last Call */
		w=glade_xml_get_widget(xml, "sTimeOnLastCall");
		if(w==NULL)
			fprintf(stderr,"Cannot get the last call time on widget\n");
		else
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(w),user.tlast);

		/* Time On Extra */
		w=glade_xml_get_widget(xml, "sTimeOnExtra");
		if(w==NULL)
			fprintf(stderr,"Cannot get the extra time on widget\n");
		else
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(w),user.textra);

		/* Total Downloads */
		w=glade_xml_get_widget(xml, "sDownloadsTotal");
		if(w==NULL)
			fprintf(stderr,"Cannot get the total downloads widget\n");
		else
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(w),user.dls);

		/* Download Bytes */
		w=glade_xml_get_widget(xml, "sDownloadsBytes");
		if(w==NULL)
			fprintf(stderr,"Cannot get the download bytes widget\n");
		else
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(w),user.dlb);

		/* Download Leeches */
		w=glade_xml_get_widget(xml, "sDownloadsLeech");
		if(w==NULL)
			fprintf(stderr,"Cannot get the downloads leech widget\n");
		else
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(w),user.leech);

		/* Total Email */
		w=glade_xml_get_widget(xml, "sEmailTotal");
		if(w==NULL)
			fprintf(stderr,"Cannot get the total email widget\n");
		else
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(w),user.emails);

		/* Email Today */
		w=glade_xml_get_widget(xml, "sEmailToday");
		if(w==NULL)
			fprintf(stderr,"Cannot get the email today widget\n");
		else
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(w),user.etoday);

		/* Email To Sysop */
		w=glade_xml_get_widget(xml, "sEmailToSysop");
		if(w==NULL)
			fprintf(stderr,"Cannot get the email to sysop widget\n");
		else
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(w),user.fbacks);

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

		w=glade_xml_get_widget(xml, "sRows");
		if(w==NULL)
			fprintf(stderr,"Cannot get the rows widget\n");
		else
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(w),user.rows);

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
			gtk_combo_box_set_active(GTK_COMBO_BOX(w),0);
			for(i=0;i<cfg.total_prots;i++) {
				if(cfg.prot[i]->mnemonic==user.prot) {
					gtk_combo_box_set_active(GTK_COMBO_BOX(w),i+1);
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
			fprintf(stderr,"Cannot get the retain ctrl-a widget\n");
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
		w=glade_xml_get_widget(xml, "tExtendedComment");

		gtk_text_buffer_get_start_iter(gtk_text_view_get_buffer(GTK_TEXT_VIEW(w)), &start);
		gtk_text_buffer_get_end_iter(gtk_text_view_get_buffer(GTK_TEXT_VIEW(w)), &end);
		gtk_text_buffer_delete(gtk_text_view_get_buffer(GTK_TEXT_VIEW(w)), &start, &end);

		sprintf(str,"%suser/%4.4u.msg", cfg.data_dir,user.number);
		f=fopen(str,"r");
		if(f) {
			while((i=fread(str,1,sizeof(str),f)))
				gtk_text_buffer_insert_at_cursor(gtk_text_view_get_buffer(GTK_TEXT_VIEW(w)), str,i);
			fclose(f);
		}

	/*
	 * Set the save buttons as inactive to indicate that no changes were made
	 */
	w=glade_xml_get_widget(xml, "bSaveUser");
	if(w==NULL)
		fprintf(stderr,"Cannot get the save user button widget\n");
	else
		gtk_widget_set_sensitive(GTK_WIDGET(w),FALSE);
	w=glade_xml_get_widget(xml, "save1");
	if(w==NULL)
		fprintf(stderr,"Cannot get the save user menu widget\n");
	else
		gtk_widget_set_sensitive(GTK_WIDGET(w),FALSE);
}

void save_user(GtkWidget *wiggy, gpointer data)
{
	/* ToDo */
	GtkWidget	*w;
	char		str[1024];
	gboolean	b;
	int			i;
	FILE		*f;
	GtkTextIter	start;
	GtkTextIter	end;

	/* Toolbar indicators */
		b=user.misc&DELETED?TRUE:FALSE;
		w=glade_xml_get_widget(xml, "bDelete");
		if(w==NULL)
			fprintf(stderr,"Cannot get the deleted toolbar widget\n");
		else {
			switch(gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(w))) {
				case 0:
					user.misc &= ~DELETED;
					break;
				default:
					user.misc |= DELETED;
			}
		}

		b=user.misc&INACTIVE?TRUE:FALSE;
		w=glade_xml_get_widget(xml, "bRemove");
		if(w==NULL)
			fprintf(stderr,"Cannot get the removed toolbar widget\n");
		else {
			switch(gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(w))) {
				case 0:
					user.misc &= ~INACTIVE;
					break;
				default:
					user.misc |= INACTIVE;
			}
		}
		

	/* Peronal Tab */
		/* Alias */
		w=glade_xml_get_widget(xml, "eUserAlias");
		if(w==NULL)
			fprintf(stderr,"Cannot get the alias widget\n");
		else {
			strcpy(user.alias, gtk_entry_get_text(GTK_ENTRY(w)));
			if(user.number) {
				if(user.misc & DELETED)
					putusername(&cfg, user.number, "");
				else
					putusername(&cfg, user.number, user.alias);
			}
		}

		/* Real Name */
		w=glade_xml_get_widget(xml, "eRealName");
		if(w==NULL)
			fprintf(stderr,"Cannot get the real name widget\n");
		else
			strcpy(user.name, gtk_entry_get_text(GTK_ENTRY(w)));

		/* Computer */
		w=glade_xml_get_widget(xml, "eComputer");
		if(w==NULL)
			fprintf(stderr,"Cannot get the computer widget\n");
		else
			strcpy(user.comp, gtk_entry_get_text(GTK_ENTRY(w)));

		/* NetMail */
		w=glade_xml_get_widget(xml, "eNetMail");
		if(w==NULL)
			fprintf(stderr,"Cannot get the netmail widget\n");
		else
			strcpy(user.netmail, gtk_entry_get_text(GTK_ENTRY(w)));

		/* Phone */
		w=glade_xml_get_widget(xml, "ePhone");
		if(w==NULL)
			fprintf(stderr,"Cannot get the phone widget\n");
		else
			strcpy(user.phone,gtk_entry_get_text(GTK_ENTRY(w)));

		/* Note */
		w=glade_xml_get_widget(xml, "eNote");
		if(w==NULL)
			fprintf(stderr,"Cannot get the note widget\n");
		else
			strcpy(user.note, gtk_entry_get_text(GTK_ENTRY(w)));

		/* Comment */
		w=glade_xml_get_widget(xml, "eComment");
		if(w==NULL)
			fprintf(stderr,"Cannot get the comment widget\n");
		else
			strcpy(user.comment, gtk_entry_get_text(GTK_ENTRY(w)));

		/* Gender */
		w=glade_xml_get_widget(xml, "eGender");
		if(w==NULL)
			fprintf(stderr,"Cannot get the gender widget\n");
		else
			user.sex=*(gtk_entry_get_text(GTK_ENTRY(w)));

		/* Connection */
		w=glade_xml_get_widget(xml, "eConnection");
		if(w==NULL)
			fprintf(stderr,"Cannot get the connection widget\n");
		else
			strcpy(user.modem, gtk_entry_get_text(GTK_ENTRY(w)));

		/* Chat Handle */
		w=glade_xml_get_widget(xml, "eHandle");
		if(w==NULL)
			fprintf(stderr,"Cannot get the handle widget\n");
		else
			strcpy(user.handle, gtk_entry_get_text(GTK_ENTRY(w)));

		/* Birthdate - Already done */

		/* Password */
		w=glade_xml_get_widget(xml, "ePassword");
		if(w==NULL)
			fprintf(stderr,"Cannot get the password widget\n");
		else
			strcpy(user.pass, gtk_entry_get_text(GTK_ENTRY(w)));

		/* Address */
		w=glade_xml_get_widget(xml, "eAddress");
		if(w==NULL)
			fprintf(stderr,"Cannot get the address widget\n");
		else
			strcpy(user.address, gtk_entry_get_text(GTK_ENTRY(w)));

		/* Location */
		w=glade_xml_get_widget(xml, "eLocation");
		if(w==NULL)
			fprintf(stderr,"Cannot get the location widget\n");
		else
			strcpy(user.location, gtk_entry_get_text(GTK_ENTRY(w)));

		/* Postal/ZIP code */
		w=glade_xml_get_widget(xml, "eZip");
		if(w==NULL)
			fprintf(stderr,"Cannot get the postal/zip code widget\n");
		else
			strcpy(user.zipcode, gtk_entry_get_text(GTK_ENTRY(w)));

	/* Security Tab */
		/* Level */
		w=glade_xml_get_widget(xml, "sLevel");
		if(w==NULL)
			fprintf(stderr,"Cannot get the level widget\n");
		else
			user.level = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w));

		/* Expiration - Already done */

		/* Flag Sets */
		strcpy(str,"tFlagSet1.");
		for(i=0;i<26;i++) {
			str[9]='A'+i;
			w=glade_xml_get_widget(xml, str);
			if(w==NULL)
				fprintf(stderr,"Cannot get the %s widget\n",str);
			else {
				switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
					case 0:
						user.flags1 &= ~(1L<<i);
						break;
					default:
						user.flags1 |= (1L<<i);
				}
			}
		}

		strcpy(str,"tFlagSet2.");
		for(i=0;i<26;i++) {
			str[9]='A'+i;
			w=glade_xml_get_widget(xml, str);
			if(w==NULL)
				fprintf(stderr,"Cannot get the %s widget\n",str);
			else {
				switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
					case 0:
						user.flags2 &= ~(1L<<i);
						break;
					default:
						user.flags2 |= (1L<<i);
				}
			}
		}

		strcpy(str,"tFlagSet3.");
		for(i=0;i<26;i++) {
			str[9]='A'+i;
			w=glade_xml_get_widget(xml, str);
			if(w==NULL)
				fprintf(stderr,"Cannot get the %s widget\n",str);
			else {
				switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
					case 0:
						user.flags3 &= ~(1L<<i);
						break;
					default:
						user.flags3 |= (1L<<i);
				}
			}
		}

		strcpy(str,"tFlagSet4.");
		for(i=0;i<26;i++) {
			str[9]='A'+i;
			w=glade_xml_get_widget(xml, str);
			if(w==NULL)
				fprintf(stderr,"Cannot get the %s widget\n",str);
			else {
				switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
					case 0:
						user.flags4 &= ~(1L<<i);
						break;
					default:
						user.flags4 |= (1L<<i);
				}
			}
		}

		/* Exemptions */
		strcpy(str,"tExemptions.");
		for(i=0;i<26;i++) {
			str[11]='A'+i;
			w=glade_xml_get_widget(xml, str);
			if(w==NULL)
				fprintf(stderr,"Cannot get the %s widget\n",str);
			else {
				switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
					case 0:
						user.exempt &= ~(1L<<i);
						break;
					default:
						user.exempt |= (1L<<i);
				}
			}
		}

		/* Restrictions */
		strcpy(str,"tRestrictions.");
		for(i=0;i<26;i++) {
			str[13]='A'+i;
			w=glade_xml_get_widget(xml, str);
			if(w==NULL)
				fprintf(stderr,"Cannot get the %s widget\n",str);
			else {
				switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
					case 0:
						user.rest &= ~(1L<<i);
						break;
					default:
						user.rest |= (1L<<i);
				}
			}
		}

		/* Credits */
		w=glade_xml_get_widget(xml, "sCredits");
		if(w==NULL)
			fprintf(stderr,"Cannot get the credits widget\n");
		else
			user.cdt = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w));

		/* Free Credits */
		w=glade_xml_get_widget(xml, "sFreeCredits");
		if(w==NULL)
			fprintf(stderr,"Cannot get the free credits widget\n");
		else
			user.freecdt = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w));

		/* Minutes */
		w=glade_xml_get_widget(xml, "sMinutes");
		if(w==NULL)
			fprintf(stderr,"Cannot get the minutes widget\n");
		else
			user.min = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w));

	/* Statistics */
		/* First On - Already done */

		/* Last On - Already done */

		/* Total Logons */
		w=glade_xml_get_widget(xml, "sLogonsTotal");
		if(w==NULL)
			fprintf(stderr,"Cannot get the total logons widget\n");
		else
			user.logons = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w));

		/* Logons today */
		w=glade_xml_get_widget(xml, "sLogonsToday");
		if(w==NULL)
			fprintf(stderr,"Cannot get the logons today widget\n");
		else
			user.ltoday = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w));

		/* Total Posts */
		w=glade_xml_get_widget(xml, "sTotalPosts");
		if(w==NULL)
			fprintf(stderr,"Cannot get the total posts widget\n");
		else
			user.posts = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w));

		/* Posts Today */
		w=glade_xml_get_widget(xml, "sPostsToday");
		if(w==NULL)
			fprintf(stderr,"Cannot get the posts today widget\n");
		else
			user.ptoday = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w));

		/* Total Uploads */
		w=glade_xml_get_widget(xml, "sTotalUploads");
		if(w==NULL)
			fprintf(stderr,"Cannot get the total uploads widget\n");
		else
			user.uls = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w));

		/* Upload Bytes */
		w=glade_xml_get_widget(xml, "sUploadBytes");
		if(w==NULL)
			fprintf(stderr,"Cannot get the upload bytes widget\n");
		else
			user.ulb = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w));

		/* Total Time On */
		w=glade_xml_get_widget(xml, "sTotalTimeOn");
		if(w==NULL)
			fprintf(stderr,"Cannot get the total time on widget\n");
		else
			user.timeon = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w));

		/* Time On Today */
		w=glade_xml_get_widget(xml, "sTimeOnToday");
		if(w==NULL)
			fprintf(stderr,"Cannot get the time on today widget\n");
		else
			user.ttoday = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w));

		/* Time On Last Call */
		w=glade_xml_get_widget(xml, "sTimeOnLastCall");
		if(w==NULL)
			fprintf(stderr,"Cannot get the last call time on widget\n");
		else
			user.tlast = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w));

		/* Time On Extra */
		w=glade_xml_get_widget(xml, "sTimeOnExtra");
		if(w==NULL)
			fprintf(stderr,"Cannot get the extra time on widget\n");
		else
			user.textra = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w));

		/* Total Downloads */
		w=glade_xml_get_widget(xml, "sDownloadsTotal");
		if(w==NULL)
			fprintf(stderr,"Cannot get the total downloads widget\n");
		else
			user.dls = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w));

		/* Download Bytes */
		w=glade_xml_get_widget(xml, "sDownloadsBytes");
		if(w==NULL)
			fprintf(stderr,"Cannot get the download bytes widget\n");
		else
			user.dlb = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w));

		/* Download Leeches */
		w=glade_xml_get_widget(xml, "sDownloadsLeech");
		if(w==NULL)
			fprintf(stderr,"Cannot get the downloads leech widget\n");
		else
			user.leech = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w));

		/* Total Email */
		w=glade_xml_get_widget(xml, "sEmailTotal");
		if(w==NULL)
			fprintf(stderr,"Cannot get the total email widget\n");
		else
			user.emails = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w));

		/* Email Today */
		w=glade_xml_get_widget(xml, "sEmailToday");
		if(w==NULL)
			fprintf(stderr,"Cannot get the email today widget\n");
		else
			user.etoday = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w));

		/* Email To Sysop */
		w=glade_xml_get_widget(xml, "sEmailToSysop");
		if(w==NULL)
			fprintf(stderr,"Cannot get the email to sysop widget\n");
		else
			user.fbacks = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w));

	/* Settings */
		w=glade_xml_get_widget(xml, "cUserAUTOTERM");
		if(w==NULL)
			fprintf(stderr,"Cannot get the autoterm widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.misc&=~AUTOTERM;
					break;
				default:
					user.misc|=AUTOTERM;
			}
		}

		w=glade_xml_get_widget(xml, "cUserNO_EXASCII");
		if(w==NULL)
			fprintf(stderr,"Cannot get the no exascii widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.misc&=~NO_EXASCII;
					break;
				default:
					user.misc|=NO_EXASCII;
			}
		}

		w=glade_xml_get_widget(xml, "cUserANSI");
		if(w==NULL)
			fprintf(stderr,"Cannot get the ansi widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.misc&=~ANSI;
					break;
				default:
					user.misc|=ANSI;
			}
		}
				

		w=glade_xml_get_widget(xml, "cUserCOLOR");
		if(w==NULL)
			fprintf(stderr,"Cannot get the color widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.misc&=~COLOR;
					break;
				default:
					user.misc|=COLOR;
			}
		}

		w=glade_xml_get_widget(xml, "cUserRIP");
		if(w==NULL)
			fprintf(stderr,"Cannot get the RIP widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.misc&=~RIP;
					break;
				default:
					user.misc|=RIP;
			}
		}

		w=glade_xml_get_widget(xml, "cUserWIP");
		if(w==NULL)
			fprintf(stderr,"Cannot get the WIP widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.misc&=~WIP;
					break;
				default:
					user.misc|=WIP;
			}
		}

		w=glade_xml_get_widget(xml, "cUserUPAUSE");
		if(w==NULL)
			fprintf(stderr,"Cannot get the upause widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.misc&=~UPAUSE;
					break;
				default:
					user.misc|=UPAUSE;
			}
		}

		w=glade_xml_get_widget(xml, "cUserCOLDKEYS");
		if(w==NULL)
			fprintf(stderr,"Cannot get the coldkeys widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.misc&=~COLDKEYS;
					break;
				default:
					user.misc|=COLDKEYS;
			}
		}

		w=glade_xml_get_widget(xml, "cUserSPIN");
		if(w==NULL)
			fprintf(stderr,"Cannot get the spin widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.misc&=~SPIN;
					break;
				default:
					user.misc|=SPIN;
			}
		}

		w=glade_xml_get_widget(xml, "cUserRIP");
		if(w==NULL)
			fprintf(stderr,"Cannot get the RIP widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.misc&=~RIP;
					break;
				default:
					user.misc|=RIP;
			}
		}

		w=glade_xml_get_widget(xml, "sRows");
		if(w==NULL)
			fprintf(stderr,"Cannot get the rows widget\n");
		else
			user.rows = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w));

		w=glade_xml_get_widget(xml, "cCommandShell");
		if(w==NULL)
			fprintf(stderr,"Cannot get the command shell widget\n");
		else
			user.shell = gtk_combo_box_get_active(GTK_COMBO_BOX(w));

		w=glade_xml_get_widget(xml, "cUserEXPERT");
		if(w==NULL)
			fprintf(stderr,"Cannot get the expert widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.misc&=~EXPERT;
					break;
				default:
					user.misc&=~EXPERT;
			}
		}

		w=glade_xml_get_widget(xml, "cUserASK_NSCAN");
		if(w==NULL)
			fprintf(stderr,"Cannot get the ask new scan widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.misc&=~ASK_NSCAN;
					break;
				default:
					user.misc|=ASK_NSCAN;
			}
		}

		w=glade_xml_get_widget(xml, "cUserASK_SSCAN");
		if(w==NULL)
			fprintf(stderr,"Cannot get the ask to you scan widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.misc&=~ASK_SSCAN;
					break;
				default:
					user.misc|=ASK_SSCAN;
			}
		}

		w=glade_xml_get_widget(xml, "cUserCURSUB");
		if(w==NULL)
			fprintf(stderr,"Cannot get the save current sub widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.misc&=~CURSUB;
					break;
				default:
					user.misc&=~CURSUB;
			}
		}

		w=glade_xml_get_widget(xml, "cUserQUIET");
		if(w==NULL)
			fprintf(stderr,"Cannot get the quiet mode widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.misc&=~QUIET;
					break;
				default:
					user.misc|=QUIET;
			}
		}

		w=glade_xml_get_widget(xml, "cUserAUTOLOGON");
		if(w==NULL)
			fprintf(stderr,"Cannot get the auto logon widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.misc&=~AUTOLOGON;
					break;
				default:
					user.misc|=AUTOLOGON;
			}
		}

		w=glade_xml_get_widget(xml, "cUserEXPERT");
		if(w==NULL)
			fprintf(stderr,"Cannot get the expert widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.misc&=~EXPERT;
					break;
				default:
					user.misc|=EXPERT;
			}
		}

		w=glade_xml_get_widget(xml, "cUserCHAT_ECHO");
		if(w==NULL)
			fprintf(stderr,"Cannot get the chat echo widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.chat&=~CHAT_ECHO;
					break;
				default:
					user.chat|=CHAT_ECHO;
			}
		}

		w=glade_xml_get_widget(xml, "cUserCHAT_ACTION");
		if(w==NULL)
			fprintf(stderr,"Cannot get the chat action widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.chat&=~CHAT_ACTION;
					break;
				default:
					user.chat|=CHAT_ACTION;
			}
		}

		w=glade_xml_get_widget(xml, "cUserCHAT_NOPAGE");
		if(w==NULL)
			fprintf(stderr,"Cannot get the chat nopage widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.chat&=~CHAT_NOPAGE;
					break;
				default:
					user.chat|=CHAT_NOPAGE;
			}
		}

		w=glade_xml_get_widget(xml, "cUserCHAT_NOACT");
		if(w==NULL)
			fprintf(stderr,"Cannot get the chat no activity widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.chat&=~CHAT_NOACT;
					break;
				default:
					user.chat|=CHAT_NOACT;
			}
		}

		w=glade_xml_get_widget(xml, "cUserCHAT_SPLITP");
		if(w==NULL)
			fprintf(stderr,"Cannot get the chat split personal widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.chat&=~CHAT_SPLITP;
					break;
				default:
					user.chat&=~CHAT_SPLITP;
			}
		}

	/* Msg/File Settings */

		w=glade_xml_get_widget(xml, "cUserNETMAIL");
		if(w==NULL)
			fprintf(stderr,"Cannot get the netmail widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.misc&=~NETMAIL;
					break;
				default:
					user.misc|=NETMAIL;
			}
		}

		w=glade_xml_get_widget(xml, "cUserCLRSCRN");
		if(w==NULL)
			fprintf(stderr,"Cannot get the clear screen widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.misc&=~CLRSCRN;
					break;
				default:
					user.misc|=CLRSCRN;
			}
		}

		w=glade_xml_get_widget(xml, "cUserANFSCAN");
		if(w==NULL)
			fprintf(stderr,"Cannot get the ask new file scan widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.misc&=~ANFSCAN;
					break;
				default:
					user.misc|=ANFSCAN;
			}
		}

		w=glade_xml_get_widget(xml, "cUserEXTDESC");
		if(w==NULL)
			fprintf(stderr,"Cannot get the extended descriptions widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.misc&=~EXTDESC;
					break;
				default:
					user.misc|=EXTDESC;
			}
		}

		w=glade_xml_get_widget(xml, "cUserBATCHFLAG");
		if(w==NULL)
			fprintf(stderr,"Cannot get the batch flagging widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.misc&=~BATCHFLAG;
					break;
				default:
					user.misc|=BATCHFLAG;
			}
		}

		w=glade_xml_get_widget(xml, "cUserAUTOHANG");
		if(w==NULL)
			fprintf(stderr,"Cannot get the auto hangup after transfer widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.misc&=~AUTOHANG;
					break;
				default:
					user.misc|=AUTOHANG;
			}
		}

		w=glade_xml_get_widget(xml, "cUserCLRSCRN");
		if(w==NULL)
			fprintf(stderr,"Cannot get the clear screen widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.misc&=~CLRSCRN;
					break;
				default:
					user.misc|=CLRSCRN;
			}
		}

		w=glade_xml_get_widget(xml, "cExternalEditor");
		if(w==NULL)
			fprintf(stderr,"Cannot get the external editor widget\n");
		else
			user.xedit = gtk_combo_box_get_active(GTK_COMBO_BOX(w));

		w=glade_xml_get_widget(xml, "cDefaultDownloadProtocol");
		if(w==NULL)
			fprintf(stderr,"Cannot get the default download protocol widget\n");
		else {
			if(gtk_combo_box_get_active(GTK_COMBO_BOX(w))==0)
				user.prot=' ';
			else
				user.prot=cfg.prot[gtk_combo_box_get_active(GTK_COMBO_BOX(w))-1]->mnemonic;
		}

		w=glade_xml_get_widget(xml, "cTempQWKFileType");
		if(w==NULL)
			fprintf(stderr,"Cannot get the temp QWK file type widget\n");
		else
			strcpy(user.tmpext, cfg.fcomp[gtk_combo_box_get_active(GTK_COMBO_BOX(w))]->ext);

		w=glade_xml_get_widget(xml, "cUserQWK_FILES");
		if(w==NULL)
			fprintf(stderr,"Cannot get the include new files list widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.qwk&=~QWK_FILES;
					break;
				default:
					user.qwk|=QWK_FILES;
			}
		}

		w=glade_xml_get_widget(xml, "cUserQWK_EMAIL");
		if(w==NULL)
			fprintf(stderr,"Cannot get the include unread email widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.qwk&=~QWK_EMAIL;
					break;
				default:
					user.qwk|=QWK_EMAIL;
			}
		}

		w=glade_xml_get_widget(xml, "cUserQWK_ALLMAIL");
		if(w==NULL)
			fprintf(stderr,"Cannot get the include all email widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.qwk&=~QWK_ALLMAIL;
					break;
				default:
					user.qwk|=QWK_ALLMAIL;
			}
		}

		w=glade_xml_get_widget(xml, "cUserQWK_DELMAIL");
		if(w==NULL)
			fprintf(stderr,"Cannot get the delete email widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.qwk&=~QWK_DELMAIL;
					break;
				default:
					user.qwk|=QWK_DELMAIL;
			}
		}

		w=glade_xml_get_widget(xml, "cUserQWK_BYSELF");
		if(w==NULL)
			fprintf(stderr,"Cannot get the include messages by self widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.qwk&=~QWK_BYSELF;
					break;
				default:
					user.qwk|=QWK_BYSELF;
			}
		}

		w=glade_xml_get_widget(xml, "cUserQWK_EXPCTLA");
		if(w==NULL)
			fprintf(stderr,"Cannot get the expand ctrl-a widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.qwk&=~QWK_EXPCTLA;
					break;
				default:
					user.qwk|=QWK_EXPCTLA;
			}
		}

		w=glade_xml_get_widget(xml, "cUserQWK_RETCTLA");
		if(w==NULL)
			fprintf(stderr,"Cannot get the retain ctrl-a widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.qwk&=~QWK_RETCTLA;
					break;
				default:
					user.qwk|=QWK_RETCTLA;
			}
		}

		w=glade_xml_get_widget(xml, "cUserQWK_ATTACH");
		if(w==NULL)
			fprintf(stderr,"Cannot get the include attachments widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.qwk&=~QWK_ATTACH;
					break;
				default:
					user.qwk|=QWK_ATTACH;
			}
		}

		w=glade_xml_get_widget(xml, "cUserQWK_NOINDEX");
		if(w==NULL)
			fprintf(stderr,"Cannot get the don't include index files widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.qwk&=~QWK_NOINDEX;
					break;
				default:
					user.qwk|=QWK_NOINDEX;
			}
		}

		w=glade_xml_get_widget(xml, "cUserQWK_TZ");
		if(w==NULL)
			fprintf(stderr,"Cannot get the include TZ widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.qwk&=~QWK_TZ;
					break;
				default:
					user.qwk|=QWK_TZ;
			}
		}

		w=glade_xml_get_widget(xml, "cUserQWK_VIA");
		if(w==NULL)
			fprintf(stderr,"Cannot get the include VIA widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.qwk&=~QWK_VIA;
					break;
				default:
					user.qwk&=~QWK_VIA;
			}
		}

		w=glade_xml_get_widget(xml, "cUserQWK_NOCTRL");
		if(w==NULL)
			fprintf(stderr,"Cannot get the include extraneous control files widget\n");
		else {
			switch(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
				case 0:
					user.qwk&=~QWK_NOCTRL;
					break;
				default:
					user.qwk|=QWK_NOCTRL;
			}
		}

	/* Extended Comment */
		sprintf(str,"%suser/%4.4u.msg", cfg.data_dir,user.number);
		w=glade_xml_get_widget(xml, "tExtendedComment");
		gtk_text_buffer_get_start_iter(gtk_text_view_get_buffer(GTK_TEXT_VIEW(w)), &start);
		gtk_text_buffer_get_end_iter(gtk_text_view_get_buffer(GTK_TEXT_VIEW(w)), &end);
		f=fopen(str,"w");
		if(f) {
			fputs(gtk_text_buffer_get_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(w)),&start,&end,TRUE), f);
			fclose(f);
		}

	if(user.number) {
		putuserdat(&cfg, &user);
		load_user(wiggy, data);
	}
	else {
		newuserdat(&cfg, &user);
		update_current_user(user.number);
	}
}

void new_user(GtkWidget *wiggy, gpointer data)
{
	GtkWidget	*eCurrentUser;
	int			i;

	memset(&user,0,sizeof(user));
 
	/****************/   
	/* Set Defaults */
	/****************/
 
	/* security */
	user.level=cfg.new_level;
	user.flags1=cfg.new_flags1;
	user.flags2=cfg.new_flags2;
	user.flags3=cfg.new_flags3;
	user.flags4=cfg.new_flags4;
	user.rest=cfg.new_rest;
	user.exempt=cfg.new_exempt;
 
	user.cdt=cfg.new_cdt;
	user.min=cfg.new_min;
	user.freecdt=cfg.level_freecdtperday[user.level];
	 
	if(cfg.total_fcomps)
		strcpy(user.tmpext,cfg.fcomp[0]->ext);
	else
		strcpy(user.tmpext,"ZIP");
	for(i=0;i<cfg.total_xedits;i++)
		if(!stricmp(cfg.xedit[i]->code,cfg.new_xedit))
			break;
	if(i<cfg.total_xedits)
		user.xedit=i+1;
	 
	user.shell=cfg.new_shell;
	user.misc=(cfg.new_misc&~(DELETED|INACTIVE|QUIET|NETMAIL));
	user.misc|=AUTOTERM;    /* No way to frob the default value... */
	user.qwk=QWK_DEFAULT;
	user.firston=time(NULL);
	user.laston=time(NULL);	/* must set this or user may be purged prematurely */
	user.pwmod=time(NULL);
	user.sex=' ';
	user.prot=cfg.new_prot;

	if(cfg.new_expire)
		user.expire=time(NULL)+((long)cfg.new_expire*24L*60L*60L);

	eCurrentUser=glade_xml_get_widget(xml, "eCurrentUser");
	if(eCurrentUser==NULL) {
		fprintf(stderr,"Cannot get the current user widget\n");
		load_user(wiggy, data);
		return;
	}
	gtk_entry_set_text(GTK_ENTRY(eCurrentUser), "New"	);
	current_user=0;
	load_user(wiggy, data);
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
	GladeXML	*axml;
    axml = glade_xml_new(glade_path, "AboutWindow", NULL);
	if(axml==NULL) {
		fprintf(stderr,"Could not locate AboutWindow widget\n");
		return;
	}
    /* connect the signals in the interface */
    glade_xml_signal_autoconnect(axml);
	gtk_window_present(GTK_WINDOW(glade_xml_get_widget(axml, "AboutWindow")));
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
	user_changed(t, data);
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
	user_changed(t, data);
}

void find_user(GtkWidget *t, gpointer data)
{
	GtkWidget	*w;
	unsigned int	nu;

	w=glade_xml_get_widget(xml, "eMatchUser");
	if(w==NULL)
		fprintf(stderr,"Cannot get the find user entry widget\n");
	else {
		nu=matchuser(&cfg, (char *)gtk_entry_get_text(GTK_ENTRY(w)), TRUE);
		if(nu==0) {
			GladeXML	*cxml;
			cxml = glade_xml_new(glade_path, "NotFoundWindow", NULL);
		    /* connect the signals in the interface */
		    glade_xml_signal_autoconnect(cxml);
			if(cxml==NULL)
				fprintf(stderr,"Could not locate NotFoundWindow widget\n");
			gtk_window_present(GTK_WINDOW(glade_xml_get_widget(cxml, "NotFoundWindow")));
		}
		else
			update_current_user(nu);
	}
}

void close_this_window(GtkWidget *t, gpointer data)
{
	gtk_widget_destroy(GTK_WIDGET(gtk_widget_get_toplevel(t)));
}

int got_date;

void destroy_calendar_window(GtkWidget *t, gpointer data)
{
	if(!got_date)
		gtk_main_quit();
}

void changed_day(GtkWidget *t, gpointer data)
{
	got_date=1;
	gtk_main_quit();
}

int get_date(GtkWidget *t, isoDate_t *date)
{
	GladeXML	*cxml;
	GtkWidget	*w;
	GtkWidget	*win;
	GtkWidget	*thiswin;
	gint		x,x_off;
	gint		y,y_off;
	guint		year;
	guint		month;
	guint		day;
	isoDate_t	odate=*date;

	got_date=0;
    cxml = glade_xml_new(glade_path, "CalendarWindow", NULL);
	if(cxml==NULL) {
		fprintf(stderr,"Could not locate Calendar Window XML\n");
		return(-1);
	}
    /* connect the signals in the interface */
    glade_xml_signal_autoconnect(cxml);
	win=glade_xml_get_widget(cxml, "CalendarWindow");
	if(win==NULL) {
		fprintf(stderr,"Could not locate Calendar window\n");
		return(-1);
	}

	thiswin = gtk_widget_get_toplevel(t);
	if(thiswin==NULL) {
		fprintf(stderr,"Could not locate main window\n");
		return(-1);
	}
	if(!(gtk_widget_translate_coordinates(GTK_WIDGET(t)
			,GTK_WIDGET(thiswin), 0, 0, &x_off, &y_off))) {
		fprintf(stderr,"Could not get position of button in window");
	}
	gtk_window_get_position(GTK_WINDOW(thiswin), &x, &y);

	gtk_window_move(GTK_WINDOW(win), x+x_off, y+y_off);

	w=glade_xml_get_widget(cxml, "Calendar");
	if(w==NULL) {
		fprintf(stderr,"Could not locate Calendar widget\n");
		return(-1);
	}
	gtk_calendar_select_month(GTK_CALENDAR(w), isoDate_month(*date)-1, isoDate_year(*date));
	gtk_calendar_select_day(GTK_CALENDAR(w), isoDate_day(*date));
	gtk_window_present(GTK_WINDOW(win));
	/* Wait for window to close... */
	gtk_main();
	w=glade_xml_get_widget(cxml, "Calendar");
	if(w==NULL)
		return(-1);
	gtk_calendar_get_date(GTK_CALENDAR(w), &year, &month, &day);
	gtk_widget_destroy(GTK_WIDGET(gtk_widget_get_toplevel(GTK_WIDGET(w))));
	*date=isoDate_create(year, month+1, day);
	return(odate!=*date);
}

void get_expiration(GtkWidget *t, gpointer data)
{
	isoDate_t	date;
	GtkWidget	*w;
	char		str[9];

	date=time_to_isoDate(user.expire?user.expire:time(NULL));
	if(!get_date(t, &date)) {
		user.expire = isoDateTime_to_time(date,0);
		user_changed(t, data);
		w=glade_xml_get_widget(xml, "eExpiration");
		if(w==NULL)
			fprintf(stderr,"Cannot get the expiration widget\n");
		else {
			if(user.expire)
				unixtodstr(&cfg, user.expire, str);
			else
				strcpy(str,"Never");
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}
	}
}

void clear_expire(GtkWidget *t, gpointer data)
{
	GtkWidget	*w;

	w=glade_xml_get_widget(xml, "eExpiration");
	if(w==NULL)
		fprintf(stderr,"Cannot get the expiration widget\n");
	user.expire=0;
	gtk_entry_set_text(GTK_ENTRY(w),"Never");
	user_changed(t,data);
}

void get_birthdate(GtkWidget *t, gpointer data)
{
	isoDate_t	date;
	GtkWidget	*w;
	int			year;
	char		str[9];

	year=atoi(user.birth+6)+1900;
	if(year<Y2K_2DIGIT_WINDOW)
		year+=100;
	if(cfg.sys_misc&SM_EURODATE)
		date=isoDate_create(year, atoi(user.birth+3), atoi(user.birth));
	else
		date=isoDate_create(year, atoi(user.birth), atoi(user.birth+3));
	if(!get_date(t, &date)) {
		if(cfg.sys_misc&SM_EURODATE)
			sprintf(user.birth,"%02u/%02u/%02u",isoDate_day(date),isoDate_month(date),isoDate_year(date));
		else
			sprintf(user.birth,"%02u/%02u/%02u",isoDate_month(date),isoDate_day(date),isoDate_year(date));
		user_changed(t, data);
		w=glade_xml_get_widget(xml, "eBirthdate");
		if(w==NULL)
			fprintf(stderr,"Cannot get the birthdate widget\n");
		else
			gtk_entry_set_text(GTK_ENTRY(w),user.birth);
	}
}


void get_firston(GtkWidget *t, gpointer data)
{
	isoDate_t	date;
	GtkWidget	*w;
	char		str[9];

	date=time_to_isoDate(user.firston?user.firston:time(NULL));
	if(!get_date(t, &date)) {
		user.firston = isoDateTime_to_time(date,0);
		user_changed(t, data);
		w=glade_xml_get_widget(xml, "eFirstOn");
		if(w==NULL)
			fprintf(stderr,"Cannot get the first on widget\n");
		else {
			if(user.firston)
				unixtodstr(&cfg, user.firston, str);
			else
				strcpy(str,"Never");
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}
	}
}

void clear_firston(GtkWidget *t, gpointer data)
{
	GtkWidget	*w;

	w=glade_xml_get_widget(xml, "eFirstOn");
	if(w==NULL)
		fprintf(stderr,"Cannot get the first on widget\n");
	user.firston=0;
	gtk_entry_set_text(GTK_ENTRY(w),"Never");
	user_changed(t,data);
}

void get_laston(GtkWidget *t, gpointer data)
{
	isoDate_t	date;
	GtkWidget	*w;
	char		str[9];

	date=time_to_isoDate(user.laston?user.laston:time(NULL));
	if(!get_date(t, &date)) {
		user.laston = isoDateTime_to_time(date,0);
		user_changed(t, data);
		w=glade_xml_get_widget(xml, "eLastOn");
		if(w==NULL)
			fprintf(stderr,"Cannot get the expiration widget\n");
		else {
			if(user.laston)
				unixtodstr(&cfg, user.laston, str);
			else
				strcpy(str,"Never");
			gtk_entry_set_text(GTK_ENTRY(w),str);
		}
	}
}

void clear_laston(GtkWidget *t, gpointer data)
{
	GtkWidget	*w;

	w=glade_xml_get_widget(xml, "eLastOn");
	if(w==NULL)
		fprintf(stderr,"Cannot get the expiration widget\n");
	user.laston=0;
	gtk_entry_set_text(GTK_ENTRY(w),"Never");
	user_changed(t,data);
}
