
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

struct client_t
{
    Window window, frame;
    bool fullscreen;
    
    struct client_t *next;
};

typedef struct client_t client_t;

// WINDOW MANAGER INFORMATION
// Important often needed variables
// are stored inside this struct
typedef struct
{
    Display *dpy;
    Window root;
    bool running;

    client_t client;
    client_t *head;
    int masters, clients;
} wm_t;
static wm_t wm;

// Functions for handling X11 events
void handle_map_request(XEvent *ev);
void handle_unmap_notify(XEvent *ev);
void handle_configure_request(XEvent *ev);
void handle_key_press(XEvent *ev);

// Tiling
void default_tiling_layout(void);

// Utils
client_t* client_from_frame(Window frame);
client_t* client_from_window(Window window);
void close_client(client_t *client);
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
typedef void (*event_lookup_f)(XEvent *ev);
static
event_lookup_f event_lookup_table[] = 
{
    [MapRequest] = handle_map_request,
    [UnmapNotify] = handle_unmap_notify,
    [ConfigureRequest] = handle_map_request,
    [KeyPress] = handle_key_press,
};


// To map an X window means to make it visible
// This handles a request to make a new window
// visible. the applicaiton requesting the mapping
// won't have a frame, so we need to frame it
void handle_map_request(XEvent *ev)
{
    XMapRequestEvent *event = &ev->xmaprequest;
    Window frame = {0};
    XWindowAttributes window_attrs = {0};
    client_t *new_client = {0}, *prev_head = wm.head;

    for(client_t* client = wm.head; client != NULL; client = client->next)
    {
        if(event->window == client->window)
            return;
    }

    XGetWindowAttributes(wm.dpy, event->window, &window_attrs);

    frame = XCreateSimpleWindow(wm.dpy, wm.root,
                                window_attrs.x, window_attrs.y,
                                window_attrs.width, window_attrs.height, 
                                border_width, border_color, background);

   XReparentWindow(wm.dpy, event->window, frame, 0, 0); 
   XMapWindow(wm.dpy, frame);
   XMapWindow(wm.dpy, event->window);

   XSelectInput(wm.dpy, frame, SubstructureNotifyMask | SubstructureRedirectMask);
   XSync(wm.dpy, True);

   new_client = (client_t*)malloc(sizeof(client_t));

   new_client->window = event->window;
   new_client->frame = frame;
   wm.head = new_client;
   new_client->next = prev_head;
   wm.clients++;

   default_tiling_layout();
}

void handle_unmap_notify(XEvent *ev)
{
    XUnmapEvent *event = &ev->xunmap;

}

void handle_configure_request(XEvent *ev)
{
    XConfigureRequestEvent event = ev->xconfigurerequest;
    XWindowChanges changes = {0};
   // (void)changes;

    changes.x = event.x;
    changes.y = event.y;

    changes.width = event.width;
    changes.height = event.height;
    
    changes.sibling = event.above;
    changes.stack_mode = event.detail;

    XConfigureWindow(wm.dpy, event.window, event.value_mask, &changes);

}

void handle_key_press(XEvent *ev)
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
            {
            }
            break;
        case XK_x:
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
    wm.head = NULL;
    wm.masters = 1;
    wm.clients = 0;
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

void default_tiling_layout(void)
{
    client_t* client = wm.head;

    for(int i = 0; i < wm.masters && client != NULL; i++)
    {
        XMoveResizeWindow(wm.dpy, client->frame,
                0, 0, screen_width/ (wm.clients == 1 ? 1 : 2), screen_height/wm.masters);
        XMoveResizeWindow(wm.dpy, client->window,
                0, 0, screen_width/(wm.clients == 1 ? 1 : 2), screen_height/wm.masters);
        client = client->next;
    }

    for(int i = 0; i < wm.clients - wm.masters && client != NULL; i++)
    {
        XMoveResizeWindow(wm.dpy, client->frame,
                screen_width/2, (screen_height/(wm.clients-wm.masters))*i,
                screen_width/2, screen_height/(wm.clients-wm.masters));
        XMoveResizeWindow(wm.dpy, client->window,
                screen_width/2, (screen_height/(wm.clients-wm.masters))*i,
                screen_width/2, screen_height/(wm.clients-wm.masters));
        client = client->next;
    }

//    XResizeWindow(wm.dpy, wm.client.frame, screen_width, screen_height);
 //   XResizeWindow(wm.dpy, wm.client.window, screen_width, screen_height);
}


client_t* client_from_frame(Window frame)
{
    for(client_t *client = wm.head; client != NULL; client = client->next)
    {
        if(frame == client->frame)
            return client;
    }

    return NULL;
}

client_t* client_from_window(Window window)
{

    for(client_t *client = wm.head; client != NULL; client = client->next)
    {
        if(window == client->window)
            return client;
    }

    return NULL;
}

void close_client(client_t *client)
{
    XEvent event;
    event.xclient.type = ClientMessage;
    event.xclient.window = wm.head->frame;
    event.xclient.message_type = XInternAtom(wm.dpy, "WM_PROTOCOLS", True);
    event.xclient.format = 32;
    event.xclient.data.l[0] = XInternAtom(wm.dpy, "WM_DELETE_WINDOW", False);
    event.xclient.data.l[1] = CurrentTime;

    XSendEvent(wm.dpy, wm.head->frame, False, NoEventMask, &event);


}

