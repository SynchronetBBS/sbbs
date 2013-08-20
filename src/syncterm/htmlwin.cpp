/* Copyright (C), 2007 by Stephen Hurd */

#undef _DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include <wx/wx.h>
#include <wx/app.h>
#include <wx/html/htmlwin.h>
#include <wx/fs_inet.h>

#include <threadwrap.h>
#include <semwrap.h>

#include "htmlwin.h"

static sem_t		appstarted;
static sem_t		shown;
static sem_t		state_changed;
static pthread_mutex_t	update_mutex;

static wxString	update_str;
enum update_type {
	 HTML_WIN_UPDATE_NONE
	,HTML_WIN_UPDATE_ADD
	,HTML_WIN_UPDATE_REPLACE
};
static enum update_type		update_type=HTML_WIN_UPDATE_NONE;

static wxFrame		*frame;
static wxHtmlWindow *htmlWindow;

static int			window_width=640;
static int			window_height=400;
static int			window_xpos=50;
static int			window_ypos=50;
static void(*output_callback)(const char *);
static int(*url_callback)(const char *, char *, size_t, char *, size_t);

static bool			html_thread_running=false;

enum html_window_state {
	 HTML_WIN_STATE_RAISED
	,HTML_WIN_STATE_LOWERED
	,HTML_WIN_STATE_ICONIZED
	,HTML_WIN_STATE_HIDDEN
};
static enum html_window_state	html_window_requested_state=HTML_WIN_STATE_RAISED;

class MyHTML: public wxHtmlWindow
{
public:

	MyHTML(wxFrame *parent, int id);
	void Clicked(wxHtmlLinkEvent &ev);

protected:
	wxHtmlOpeningStatus OnOpeningURL(wxHtmlURLType type,const wxString& url, wxString *redirect) const;
	void OnKeyDown(wxKeyEvent& event);
	void OnUpdate(wxCommandEvent &event);
	void OnState(wxCommandEvent &event);
	void UnHide(void);

	DECLARE_EVENT_TABLE()
};

MyHTML::MyHTML(wxFrame *parent, int id) : wxHtmlWindow(parent, id)
{
}

wxHtmlOpeningStatus MyHTML::OnOpeningURL(wxHtmlURLType type,const wxString& url, wxString *redirect) const
{
	char	cache[4096];
	char	orig[4096];
	char	buf[1024];
	FILE	*outfile;
	int		ret;

	ret=url_callback(url.mb_str(), cache, sizeof(cache), orig, sizeof(orig));
	wxString cstr(cache,wxConvUTF8);
	wxString remotefile(orig,wxConvUTF8);
	redirect->Empty();
	redirect->Append(cstr);
	switch(ret) {
		case URL_ACTION_DOWNLOAD:
			outfile=fopen(cache+5 /* file:// */,"wb");
			if(outfile) {
				wxFileSystem *fs=new wxFileSystem();
				wxFSFile *infile=fs->OpenFile(remotefile);
				wxInputStream *in=infile->GetStream();
				while(!in->Eof())
					fwrite(buf, in->Read(buf,sizeof(buf)).LastRead(), 1, outfile);
				fclose(outfile);
				delete infile;
				delete fs;
			}
		case URL_ACTION_REDIRECT:
			return(wxHTML_REDIRECT);
	}
	return(wxHTML_OPEN);
}

void MyHTML::Clicked(wxHtmlLinkEvent &ev)
{
	output_callback(ev.GetLinkInfo().GetHref().mb_str());
}

void MyHTML::OnKeyDown(wxKeyEvent& event)
{
	int key=event.GetKeyCode();
	char send[2]={0,0};

	if(key >= ' ' && key <= '~')
		send[0]=key;
	else {
		switch(key) {
			case WXK_BACK:
			case WXK_TAB:
			case WXK_RETURN:
			case WXK_ESCAPE:
			case WXK_SPACE:
			case WXK_DELETE:
				send[0]=key;
		}
	}
	if(send[0])
		output_callback(send);
	event.Skip();
}

void MyHTML::OnUpdate(wxCommandEvent &event)
{
	pthread_mutex_lock(&update_mutex);
	switch(update_type) {
		case HTML_WIN_UPDATE_REPLACE:
			frame->Show();
			frame->Raise();
			htmlWindow->SetPage(update_str);
			htmlWindow->Raise();
			htmlWindow->SetFocus();
			sem_post(&shown);
			break;
		case HTML_WIN_UPDATE_ADD:
			htmlWindow->AppendToPage(update_str);
			sem_post(&shown);
			break;
	}
	update_str=wxT("");
	update_type=HTML_WIN_UPDATE_NONE;
	pthread_mutex_unlock(&update_mutex);
}

void MyHTML::UnHide(void)
{
	int width,height,xpos,ypos;

	if(!frame->IsShown()) {
		frame->GetPosition(&xpos, &ypos);
		frame->GetSize(&width, &height);
		if(xpos != window_xpos 
				|| ypos != window_ypos
				|| width != window_width
				|| height != window_height)
			frame->SetSize(window_xpos, window_ypos, window_width, window_height, wxSIZE_AUTO);
		frame->Show(true);
	}
}

void MyHTML::OnState(wxCommandEvent &event)
{
	if(wxTheApp) {
		switch(html_window_requested_state) {
			case HTML_WIN_STATE_RAISED:
				UnHide();
				if(frame->IsIconized())
					frame->Iconize(false);
#ifdef _WIN32
				frame->Hide();
#endif
				frame->Show();
				frame->Raise();
				htmlWindow->Show();
				htmlWindow->Raise();
				htmlWindow->SetFocus();
				break;
			case HTML_WIN_STATE_LOWERED:
				UnHide();
				if(frame->IsIconized())
					frame->Iconize(false);
				frame->Lower();
				break;
			case HTML_WIN_STATE_ICONIZED:
				UnHide();
				frame->Lower();
				if(!frame->IsIconized())
					frame->Iconize(true);
				break;
			case HTML_WIN_STATE_HIDDEN:
				frame->Lower();
				if(frame->IsShown())
					frame->Show(false);
				break;
		}
	}
	sem_post(&state_changed);
}

#define HTML_ID		wxID_HIGHEST

DECLARE_EVENT_TYPE(update_event, -1)
DECLARE_EVENT_TYPE(state_event, -1)
DEFINE_EVENT_TYPE(update_event)
DEFINE_EVENT_TYPE(state_event)

BEGIN_EVENT_TABLE(MyHTML, wxHtmlWindow)
  EVT_KEY_DOWN(MyHTML::OnKeyDown)
  EVT_HTML_LINK_CLICKED(HTML_ID, MyHTML::Clicked)
  EVT_COMMAND(HTML_ID, update_event, MyHTML::OnUpdate)
  EVT_COMMAND(HTML_ID, state_event, MyHTML::OnState)
END_EVENT_TABLE()

void send_html_event(int evt)
{
	wxCommandEvent event( evt, HTML_ID );
	htmlWindow->GetEventHandler()->AddPendingEvent( event );
}

class MyFrame: public wxFrame
{
public:
	MyFrame(wxWindow * parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style);
private:
	void OnCloseWindow(wxCloseEvent& event);

	DECLARE_EVENT_TABLE()
};

MyFrame::MyFrame(wxWindow * parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style)
: wxFrame(parent, id, title, pos, size, style)
{
}

void MyFrame::OnCloseWindow(wxCloseEvent& event)
{
	if(frame->IsShown())
		Show(false);
}

BEGIN_EVENT_TABLE(MyFrame, wxFrame)
  EVT_CLOSE(MyFrame::OnCloseWindow)
END_EVENT_TABLE()

class MyApp: public wxApp
{
    virtual bool OnInit();
};

bool MyApp::OnInit()
{
    frame = new MyFrame((wxFrame *)NULL
			, -1
			, wxT("SyncTERM HTML")
			, wxPoint(window_xpos,window_ypos)
			, wxSize(window_width,window_height)
			, wxMINIMIZE_BOX | wxRESIZE_BORDER | wxCAPTION | wxCLOSE_BOX | wxSYSTEM_MENU /* wxMAXIMIZE_BOX | wxSYSTEM_MENU | wxCLIP_CHILDREN */
	);
	htmlWindow = new MyHTML(frame, HTML_ID);
	htmlWindow->SetRelatedFrame(frame,wxT("SyncTERM HTML : %s"));
	wxFileSystem::AddHandler(new wxInternetFSHandler);
	wxInitAllImageHandlers();
    frame->Show();
    SetTopWindow( frame );
	sem_post(&appstarted);
    return true;
}

IMPLEMENT_APP_NO_MAIN(MyApp)

void html_thread(void *args)
{
	int argc=1;
	char *argv[2];

	SetThreadName("HTML Thread");
	html_thread_running=true;
	argv[0]=(char *)"wxHTML";
	argv[1]=(char *)"--sync";
	wxEntry(argc, argv);
	if(wxTheApp) {
		wxTheApp->OnExit();
		wxTheApp->CleanUp();
	}
	else
		sem_post(&appstarted);
	html_thread_running=false;
}

extern "C" {

	int run_html(int width, int height, int xpos, int ypos, void(*callback)(const char *), int(*ucallback)(const char *, char *, size_t, char *, size_t))
	{
		output_callback=callback;
		url_callback=ucallback;
		window_xpos=xpos==-1?wxDefaultCoord:xpos;
		window_ypos=ypos==-1?wxDefaultCoord:ypos;
		window_width=width==-1?640:width;
		window_height=height==-1?200:height;

		if(!html_thread_running) {
			sem_init(&appstarted, 0, 0);
			sem_init(&state_changed, 0, 0);
			sem_init(&shown, 0, 0);
			pthread_mutex_init(&update_mutex, NULL);
			_beginthread(html_thread, 0, NULL);
			sem_wait(&appstarted);
			sem_destroy(&appstarted);
		}
		return(!wxTheApp);
	}

	void hide_html(void)
	{
		html_window_requested_state=HTML_WIN_STATE_HIDDEN;
		send_html_event(state_event);
		sem_wait(&state_changed);
	}

	void iconize_html(void)
	{
		html_window_requested_state=HTML_WIN_STATE_ICONIZED;
		send_html_event(state_event);
		sem_wait(&state_changed);
	}

	void raise_html(void)
	{
		html_window_requested_state=HTML_WIN_STATE_RAISED;
		send_html_event(state_event);
		sem_wait(&state_changed);
	}
	
	void lower_html(void)
	{
		html_window_requested_state=HTML_WIN_STATE_LOWERED;
		send_html_event(state_event);
		sem_wait(&state_changed);
	}

	void html_commit(void)
	{
		pthread_mutex_lock(&update_mutex);
		if(update_type!=HTML_WIN_UPDATE_NONE) {
			send_html_event(update_event);
			pthread_mutex_unlock(&update_mutex);
			sem_wait(&shown);
		}
		else
			pthread_mutex_unlock(&update_mutex);
	}

	void add_html(const char *buf)
	{
		wxString wx_page(buf,wxConvUTF8);
		pthread_mutex_lock(&update_mutex);
		update_str+=wx_page;
		update_type=HTML_WIN_UPDATE_ADD;
		pthread_mutex_unlock(&update_mutex);
	}

	void add_html_char(char ch)
	{
		char str[2];

		str[0]=ch;
		str[1]=0;
		add_html(str);
	}

	void show_html(const char *page)
	{
		if(wxTheApp) {
			pthread_mutex_lock(&update_mutex);
			wxString wx_page(page, wxConvUTF8);
			update_str=wx_page;
			update_type=HTML_WIN_UPDATE_REPLACE;
			pthread_mutex_unlock(&update_mutex);
			html_commit();
		}
	}
}
