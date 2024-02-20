
#include<stdio.h>
#include<stdlib.h>
#include<X11/Xlib.h>
#include<X11/keysym.h>
#include<X11/XKBlib.h>
#include<unistd.h>
#include<stdbool.h>

#include "config.h"

#define LOG(...)\
    fprintf(stderr, __VA_ARGS__);\
    fprintf(stderr, "\n");

// Functions for handling X11 events
void handle_map_request(const XEvent *ev);
void handle_configure_request(const XEvent *ev);
void handle_key_press(const XEvent *ev);

// Tiling

// Utils
void exec(const char* program)
{
    if(fork() == 0)
    {
        setsid();
        execlp(program, "");
    }
}

// LOOK UP TABLE
// This is a function look up table which contains function pointers
// Each event corresponds to an index in this array.
// And at each index, the corresponding function can be found
typedef void (*event_lookup_f)(const XEvent *ev);
static
event_lookup_f event_lookup_table[] = 
{
    [MapRequest] = handle_map_request,
    [ConfigureRequest] = handle_map_request,
    [KeyPress] = handle_key_press,
};

typedef struct
{
    Window window, frame;
    bool fullscreen;
} client_t;

// WINDOW MANAGER INFORMATION
// Important often needed variables
// are stored inside this struct
typedef struct
{
    Display *dpy;
    Window root;
    bool running;

    Window win, frame;
} wm_t;
static wm_t wm;


// To map an X window means to make it visible
// This handles a request to make a new window
// visible. the applicaiton requesting the mapping
// won't have a frame, so we need to frame it
void handle_map_request(const XEvent *ev)
{
    XMapRequestEvent event = ev->xmaprequest;
    Window frame = {0};
    XWindowAttributes window_attrs = {0};

    XGetWindowAttributes(wm.dpy, event.window, &window_attrs);

    frame = XCreateSimpleWindow(wm.dpy, wm.root,
                                window_attrs.x, window_attrs.y,
                                window_attrs.width+100, window_attrs.height+100, 
                                border_width, border_color, background);

   XReparentWindow(wm.dpy, event.window, frame, 0, 0); 
   XMapWindow(wm.dpy, frame);
   XMapWindow(wm.dpy, event.window);

   wm.frame = frame;
   wm.win = event.window;
}

void handle_configure_request(const XEvent *ev)
{
    XConfigureRequestEvent event = ev->xconfigurerequest;
    XWindowChanges changes = {0};

    changes.x = event.x;
    changes.y = event.y;

    changes.width = event.width;
    changes.height = event.height;

    changes.sibling = event.above;
    changes.stack_mode = event.detail;

    XConfigureWindow(wm.dpy, event.window, event.value_mask, &changes);

}

void handle_key_press(const XEvent *ev)
{
    LOG("Key press");
    XKeyEvent event = ev->xkey;
    KeySym keysym;

    keysym = XkbKeycodeToKeysym(wm.dpy, event.keycode, 0,
            event.state & ShiftMask ? 1 : 0);

    switch(keysym)
    {
        case XK_t:
            exec("/usr/bin/xterm");
            break;
        case XK_q:
            wm.running = false;
            break;
        default:
            break;
    }
}


int main(void)
{
    wm.dpy = XOpenDisplay(NULL);
    wm.root = DefaultRootWindow(wm.dpy);
    wm.running = true;
    XFlush(wm.dpy);

    XSelectInput(wm.dpy, wm.root, SubstructureNotifyMask | SubstructureRedirectMask);
    XGrabKey(wm.dpy, AnyKey, Mod1Mask, wm.root, True, GrabModeAsync, GrabModeAsync);

    XEvent ev = {0};
    XSync(wm.dpy, False);
    while(wm.running)
    {
        XNextEvent(wm.dpy, &ev);
        // call the function in the look up table
        // and provide a pointer to the event as the arg.
        if(event_lookup_table[ev.type] != 0)
            event_lookup_table[ev.type](&ev);
    }

    return 0;
}

