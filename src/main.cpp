/*
 * a tool to move window to the specified monitor.
 */

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>


#include "config.h"

#include <vector>
#include <string>
#include <exception>
#include <stdexcept>
#include <algorithm>
using namespace std;


#define MAX_PROPERTY_VALUE_LEN 4096

#define p_verbose(...) fprintf(stderr, __VA_ARGS__);

static char* get_property (Display* disp, Window win,
        Atom xa_prop_type, const char* prop_name, unsigned long* size) {
    Atom xa_prop_name;
    Atom xa_ret_type;
    int ret_format;
    unsigned long ret_nitems;
    unsigned long ret_bytes_after;
    unsigned long tmp_size;
    unsigned char *ret_prop;
    char* ret;

    xa_prop_name = XInternAtom(disp, prop_name, False);

    /*
     * MAX_PROPERTY_VALUE_LEN / 4 explanation (XGetWindowProperty manpage):
     *
     * long_length = Specifies the length in 32-bit multiples of the
     *               data to be retrieved.
     */
    Status st;
    st = XGetWindowProperty(disp, win, xa_prop_name, 0, MAX_PROPERTY_VALUE_LEN / 4, False,
            xa_prop_type, &xa_ret_type, &ret_format,
            &ret_nitems, &ret_bytes_after, &ret_prop);
    if (st != Success) {
        //p_verbose("Cannot get %s property.\n", prop_name);
        return NULL;
    }

    if (xa_ret_type != xa_prop_type) {
        //p_verbose("Invalid type of %s property.\n", prop_name);
        XFree(ret_prop);
        return NULL;
    }

    /* null terminate the result to make string handling easier */
    tmp_size = (ret_format / (32 / sizeof(long))) * ret_nitems;
    ret = (char*)malloc(tmp_size + 1);
    memcpy(ret, ret_prop, tmp_size);
    ret[tmp_size] = '\0';

    if (size) {
        *size = tmp_size;
    }

    XFree(ret_prop);
    return ret;
}


static Window* get_client_list (Display *disp, unsigned long *size) {
    Window* client_list;

    client_list = (Window *)get_property(disp, DefaultRootWindow(disp),
                        XA_WINDOW, "_NET_CLIENT_LIST", size);
    if (client_list == NULL) {
    	client_list = (Window *)get_property(disp, DefaultRootWindow(disp),
    	                        XA_CARDINAL, "_WIN_CLIENT_LIST", size);
        if (client_list == NULL) {
            fputs("Cannot get client list properties. \n"
                  "(_NET_CLIENT_LIST or _WIN_CLIENT_LIST)"
                  "\n", stderr);
            return NULL;
        }
    }

    return client_list;
}


struct WinInfo
{
	Window handle;
	string title;
};

struct MonitorInfo
{
	string name; // the name of monitor
	XRRMonitorInfo* info;
};

/**
 * move win to monitor
 */
void moveWinToMonitor(Display* dpy, WinInfo& wi, MonitorInfo* mi)
{
	int x = mi->info->x + 10;
	int y = mi->info->y + 10;
	//p_verbose("move win: %s to %d %d \n", wi.title.c_str(), x, y);
	XMoveWindow(dpy, wi.handle, x, y);
}

/**
 * find monitor by name, case insentive
 * return -1 when no found
 */
int findMonitorByName(vector<MonitorInfo>& monitors, const char* name)
{
	for(unsigned int i=0;i<monitors.size();i++) {
		if(strcasecmp(monitors[i].name.c_str(), name)==0) {
			return i;
		}
	}
	return -1;
}

/**
 * get window title
 */
string getWindowTitle (Display* disp, Window win)
{
    char* wm_name;
    string ret;

    wm_name = get_property(disp, win, XA_STRING, "WM_NAME", NULL);
    if (wm_name!=NULL) {
    	ret = wm_name;
    	free(wm_name);
    	return ret;
    }

    wm_name = get_property(disp, win,
                XInternAtom(disp, "UTF8_STRING", False), "_NET_WM_NAME", NULL);
	if (wm_name!=NULL) {
		ret = wm_name;
	    free(wm_name);
		return ret;
	}

    return ret;
}

/**
 * get all windows and their title
 */
void getWindows(Display* dpy, vector<WinInfo>& wins, vector<void*>& xfrees)
{
	Window *client_list;
    unsigned long client_list_size;

    client_list = get_client_list(dpy, &client_list_size);
    if (client_list == NULL) {
    	throw runtime_error("can not get window list");
    }

    unsigned int i;
    WinInfo wi;
    for (i = 0; i < client_list_size / sizeof(Window); i++) {
    	wi.handle = client_list[i];
    	wi.title = getWindowTitle(dpy, client_list[i]);

    	// to lower case
	    transform(wi.title.begin(), wi.title.end(), wi.title.begin(), ::tolower);

    	wins.push_back(wi);
    }
}

/**
 * get all active monitors
 */
void getMonitors(Display* dpy, vector<MonitorInfo>& monitors, vector<void*>& xfrees)
{
	XRRMonitorInfo* m;
	int	n=-1;

	Window root = DefaultRootWindow(dpy);
	m = XRRGetMonitors(dpy, root, True, &n);
	if (n == -1) {
		throw runtime_error("get monitors failed");
	}

	MonitorInfo mi;
	char* name;
	for(int i=0;i<n;i++) {
		XRRMonitorInfo* p = &m[i];
		name = XGetAtomName(dpy, p->name);
		mi.name = name;
		mi.info = p;
		XFree(name);

		monitors.push_back(mi);
	}

	xfrees.push_back(m);
}


int main (int argc, char* argv[])
{
	if(argc<2 || (argc+1)%2!=0 ) {
		fprintf(stderr, "Usage: mwtm 'win0Title' monitorName 'win1Title' monitorName ...\n");
		return 2;
	}

	int ret = EXIT_FAILURE;

	Display* dpy = XOpenDisplay(NULL);
	if(dpy==NULL) {
    	p_verbose("Open display error\n");
        return EXIT_FAILURE;
	}

	// some x pointers to free
	vector<void*> xfrees;

	vector<MonitorInfo> monitors;
	vector<WinInfo> wins;

    unsigned int i, j;
	try {
		getMonitors(dpy, monitors, xfrees);
		getWindows(dpy, wins, xfrees);

		string titleL;
		string mnameL;

		int mid;
		for(i=1;i<(unsigned)argc;i=i+2) {
			titleL = argv[i];
			mnameL = argv[i+1];

			// to lower case
		    transform(titleL.begin(), titleL.end(), titleL.begin(), ::tolower);
		    transform(mnameL.begin(), mnameL.end(), mnameL.begin(), ::tolower);

			mid = findMonitorByName(monitors, mnameL.c_str());
			if(mid==-1) {
		    	p_verbose("Can not find the monitor: %s\n", mnameL.c_str() );
				continue;
			}

			for(j=0;j<wins.size();j++) {
				if(strstr(wins[j].title.c_str(), titleL.c_str())!=NULL) {
					moveWinToMonitor(dpy, wins[j], &monitors[mid]);
				}
			}
		}

		ret = EXIT_SUCCESS;

	}
	catch(exception& e) {
    	p_verbose("Error: %s \n", e.what() );
	}

	for(i=0;i<xfrees.size();i++) {
		XFree(xfrees[i]);
	}
	xfrees.clear();

	XCloseDisplay(dpy);

	return ret;
}
