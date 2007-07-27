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
static sem_t		hidden;

static wxString	addr;
static wxString	currPage;
static bool		newpage=false;

static wxFrame		*frame;
static wxHtmlWindow *htmlWindow;
static wxTimer		*update_timer;
static wxTimer		*destroy_timer;

static int			window_width=640;
static int			window_height=400;
static int			window_xpos=50;
static int			window_ypos=50;
static void(*output_callback)(const char *);

static bool		html_thread_running=false;
static bool 		html_running=false;

class MyUpdateTimer: public wxTimer
{
protected:
	void Notify(void);
};

void MyUpdateTimer::Notify(void)
{
	if(newpage) {
		int width,height,xpos,ypos;
	
		frame->Show(true);
		html_running = true;
		frame->Raise();
		frame->GetPosition(&xpos, &ypos);
		frame->GetSize(&width, &height);
		if(xpos != window_xpos 
				|| ypos != window_ypos
				|| width != window_width
				|| height != window_height)
			frame->SetSize(window_xpos, window_ypos, window_width, window_height, wxSIZE_FORCE);
		htmlWindow->SetPage(currPage);
		newpage=false;
		sem_post(&shown);
	}
}

class MyDestroyTimer: public wxTimer
{
protected:
	void Notify(void);
};

void MyDestroyTimer::Notify(void)
{
	frame->Show(false);
	html_running = false;
	sem_post(&hidden);
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
	wxString redir=*redirect;

	/* If the URL does not contain :// we need to fix it up and redirect */
	if(!url.Matches(wxT("*://*"))) {
		redir=wxT("");
		redir+=wxT("http://")+addr;
		if(!url.StartsWith(wxT("/")))
			redir+=wxT("/");
		redir+=url;
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


class MyApp: public wxApp
{
    virtual bool OnInit();
};

bool MyApp::OnInit()
{
    frame = new wxFrame((wxFrame *)NULL
			, -1
			, wxT("SyncTERM HTML")
			, wxPoint(window_xpos,window_ypos)
			, wxSize(window_width,window_height)
			, wxMINIMIZE_BOX | wxCAPTION | wxCLIP_CHILDREN
	);
	wxFileSystem::AddHandler(new wxInternetFSHandler);
	wxInitAllImageHandlers();
	htmlWindow = new MyHTML(frame, HTML_ID);
	htmlWindow->SetRelatedFrame(frame,wxT("SyncTERM HTML : %s"));
    frame->Show( true );
	frame->Lower();
    SetTopWindow( frame );
	update_timer = new MyUpdateTimer();
	destroy_timer = new MyDestroyTimer();
	destroy_timer->Start(1, true);
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
			sem_init(&hidden, 0, 0);
			sem_init(&shown, 0, 0);
			_beginthread(html_thread, 0, NULL);
			sem_wait(&appstarted);
			sem_destroy(&appstarted);
		}
		return(!wxTheApp);
	}

	void hide_html(void)
	{
		if(html_running) {
			destroy_timer->Start(1, true);
			sem_wait(&hidden);
		}
	}

	void show_html(const char *address, int width, int height, int xpos, int ypos, void(*callback)(const char *), const char *page)
	{
		window_xpos=xpos;
		window_ypos=ypos;
		window_width=width;
		window_height=height;
		output_callback=callback;

		if(!run_html()) {
			wxString wx_page(page,wxConvUTF8);
			currPage=wx_page;
			wxString newaddr(address,wxConvUTF8);
			addr=newaddr;
			newpage=true;
			update_timer->Start(1, true);
			sem_wait(&shown);
		}
	}
}
