//
// file name: hworld.cpp
//
//   purpose: wxWidgets "Hello world"
//

// For compilers that support precompilation, includes "wx/wx.h".

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

static wxString	addr;
static wxString	update_str;

enum update_type {
	 HTML_WIN_UPDATE_NONE
	,HTML_WIN_UPDATE_ADD
	,HTML_WIN_UPDATE_REPLACE
};

static enum update_type		update_type=HTML_WIN_UPDATE_NONE;

static wxFrame		*frame;
static wxHtmlWindow *htmlWindow;
static wxTimer		*update_timer;
static wxTimer		*state_timer;

static int			window_width=640;
static int			window_height=400;
static int			window_xpos=50;
static int			window_ypos=50;
static void(*output_callback)(const char *);

static bool			html_thread_running=false;

enum html_window_state {
	 HTML_WIN_STATE_RAISED
	,HTML_WIN_STATE_ICONIZED
	,HTML_WIN_STATE_HIDDEN
};

static enum html_window_state	html_window_requested_state=HTML_WIN_STATE_RAISED;

class MyUpdateTimer: public wxTimer
{
protected:
	void Notify(void);
};

void MyUpdateTimer::Notify(void)
{
	int width,height,xpos,ypos;

	pthread_mutex_lock(&update_mutex);
	switch(update_type) {
		case HTML_WIN_UPDATE_REPLACE:
			frame->Show();
			frame->Raise();
			frame->GetPosition(&xpos, &ypos);
			frame->GetSize(&width, &height);
			if(xpos != window_xpos 
					|| ypos != window_ypos
					|| width != window_width
					|| height != window_height)
				frame->SetSize(window_xpos, window_ypos, window_width, window_height, wxSIZE_AUTO);
			htmlWindow->SetPage(update_str);
			sem_post(&shown);
			break;
		case HTML_WIN_UPDATE_ADD:
			htmlWindow->AppendToPage(update_str);
			break;
	}
	update_str=wxT("");
	update_type=HTML_WIN_UPDATE_NONE;
	pthread_mutex_unlock(&update_mutex);
}

class MyStateTimer: public wxTimer
{
protected:
	void Notify(void);
};

void MyStateTimer::Notify(void)
{
	if(wxTheApp) {
		switch(html_window_requested_state) {
			case HTML_WIN_STATE_RAISED:
				if(!frame->IsShown())
					frame->Show();
				if(frame->IsIconized())
					frame->Iconize(false);
				frame->Raise();
				break;
			case HTML_WIN_STATE_ICONIZED:
				if(!frame->IsShown())
					frame->Show();
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

class MyHTML: public wxHtmlWindow
{
public:

	MyHTML(wxFrame *parent, int id);
	void Clicked(wxHtmlLinkEvent &ev);

protected:
	wxHtmlOpeningStatus OnOpeningURL(wxHtmlURLType type,const wxString& url, wxString *redirect) const;
	DECLARE_EVENT_TABLE();
};

MyHTML::MyHTML(wxFrame *parent, int id) : wxHtmlWindow(parent, id)
{
}

wxHtmlOpeningStatus MyHTML::OnOpeningURL(wxHtmlURLType type,const wxString& url, wxString *redirect) const
{
	/* If the URL does not contain :// we need to fix it up and redirect */
	if(!url.Matches(wxT("*://*"))) {
		redirect->Empty();
		redirect->Append(wxT("http://"));
		redirect->Append(addr);
		if(!url.StartsWith(wxT("/")))
			redirect->Append(wxT("/"));
		redirect->Append(url);
		return wxHTML_REDIRECT;
	}
	return wxHTML_OPEN;
}

void MyHTML::Clicked(wxHtmlLinkEvent &ev)
{
	output_callback(ev.GetLinkInfo().GetHref().mb_str());
}

#define HTML_ID		wxID_HIGHEST

BEGIN_EVENT_TABLE(MyHTML, wxHtmlWindow)
  EVT_HTML_LINK_CLICKED(HTML_ID, MyHTML::Clicked)
END_EVENT_TABLE()

class MyFrame: public wxFrame
{
public:
	MyFrame(wxWindow * parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style);
private:
	void MyFrame::OnCloseWindow(wxCloseEvent& event);

	DECLARE_EVENT_TABLE();
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
			, wxCLOSE_BOX | wxMINIMIZE_BOX | wxCAPTION | wxCLIP_CHILDREN
	);
	wxFileSystem::AddHandler(new wxInternetFSHandler);
	wxInitAllImageHandlers();
	htmlWindow = new MyHTML(frame, HTML_ID);
	htmlWindow->SetRelatedFrame(frame,wxT("SyncTERM HTML : %s"));
    frame->Show();
	frame->Iconize();
    SetTopWindow( frame );
	update_timer = new MyUpdateTimer();
	state_timer = new MyStateTimer();
	state_timer->Start(1, true);
	sem_post(&appstarted);
    return true;
}

IMPLEMENT_APP_NO_MAIN(MyApp)

void html_thread(void *args)
{
	int argc=1;
	char *argv[2];

	html_thread_running=true;
	argv[0]="wxHTML";
	argv[1]="--sync";
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

	int run_html()
	{
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
		state_timer->Start(1, true);
		sem_wait(&state_changed);
	}

	void iconize_html(void)
	{
		html_window_requested_state=HTML_WIN_STATE_ICONIZED;
		state_timer->Start(1, true);
		sem_wait(&state_changed);
	}

	void raise_html(void)
	{
		html_window_requested_state=HTML_WIN_STATE_RAISED;
		state_timer->Start(1, true);
		sem_wait(&state_changed);
	}
	
	void add_html(const char *buf)
	{
		wxString wx_page(buf,wxConvUTF8);
		pthread_mutex_lock(&update_mutex);
		update_str+=wx_page;
		if(update_type==HTML_WIN_UPDATE_NONE)
			update_timer->Start(10, true);
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
	
	void show_html(const char *address, int width, int height, int xpos, int ypos, void(*callback)(const char *), const char *page)
	{
		window_xpos=xpos==-1?wxDefaultCoord:xpos;
		window_ypos=ypos==-1?wxDefaultCoord:ypos;
		window_width=width==-1?640:width;
		window_height=height==-1?200:height;
		output_callback=callback;

		if(!run_html()) {
			wxString wx_page(page,wxConvUTF8);
			wxString newaddr(address,wxConvUTF8);
			addr=newaddr;
			pthread_mutex_lock(&update_mutex);
			update_str=wx_page;
			update_type=HTML_WIN_UPDATE_REPLACE;
			pthread_mutex_unlock(&update_mutex);
			update_timer->Start(1, true);
			sem_wait(&shown);
		}
	}
}
