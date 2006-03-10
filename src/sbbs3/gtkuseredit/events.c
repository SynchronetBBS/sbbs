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

	user.number=current_user;
	if(user.number < 1 || user.number > totalusers) {
		fprintf(stderr,"Attempted to load illegal user number %d.\n",user.number);
		return;
	}
	if(getuserdat(&cfg, &user)) {
		fprintf(stderr,"Error loading user %d.\n",current_user);
		return;
	}

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
