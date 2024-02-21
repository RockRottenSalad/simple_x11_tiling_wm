
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

// Each window needs a frame/border
// The client type exists to pair the window with the border
// And also to create a linked list to traverse the clients
struct client_t
{
    Window window, frame;
    bool fullscreen;
    
    struct client_t *next;
};
typedef struct client_t client_t;

// Window manager info
// Important often needed variables
// are stored inside this struct
typedef struct
{
    Display *dpy;
    Window root;
    bool running;

    client_t *focus;
    client_t *head;
    int masters, clients;
} wm_t;
static wm_t wm;

// Functions for handling X11 events
void handle_map_request(XEvent *ev);
void handle_unmap_notify(XEvent *ev);
void handle_destroy(XEvent *ev);
void handle_configure_request(XEvent *ev);
void handle_key_press(XEvent *ev);
void handle_motion(XEvent *ev);

// Functions related to tiling windows
void default_tiling_layout(void);
void focus_next(void);
void focus_prev(void);
void update_masters(int change);

// General utils functions
client_t* client_from_frame(Window frame, client_t **ret_prev);
client_t* client_from_window(Window window, client_t **ret_prev);
void close_client(client_t *client);
void exec(const char* program)
{
    if(fork() == 0)
    {
        setsid();
        execlp(program, "");
    }
}

// Look up table
// This is a table which contains function pointers
// Each event corresponds to an index in this array.
// X11 events are used as the index
// Everything in between is filled with zeroes
// to make sure there's no uninitialized gearbage 
typedef void (*event_lookup_f)(XEvent *ev);
static
event_lookup_f event_lookup_table[] = 
{
    [0 ... KeyPress-1] = 0,
    [KeyPress] = handle_key_press,
    [KeyPress+1 ... MotionNotify-1] = 0,
    [MotionNotify] = handle_motion, // 6
    [MotionNotify+1 ... DestroyNotify-1] = 0,
    [DestroyNotify] = handle_destroy, // 17
    [UnmapNotify] = handle_unmap_notify, // 18
    [UnmapNotify+1 ... MapRequest-1] = 0,
    [MapRequest] = handle_map_request, // 20
    [MapRequest+1 ... ConfigureRequest-1] = 0,
    [ConfigureRequest] = handle_map_request, // 23
};


// To map an X window means to make it visible
// This handles a request to make a new window
// visible. the applicaiton requesting the mapping
// The window won't have a frame, so we need to frame it
void handle_map_request(XEvent *ev)
{
    XMapRequestEvent *event = &ev->xmaprequest;
    Window frame = {0};
    XWindowAttributes window_attrs = {0};
    client_t *new_client = {0}, *prev_head = wm.head;

    for(client_t* client = wm.head; client != NULL; client = client->next)
    {
        if(event->window == client->window)
        {
            LOG("Remapping");
            XMapWindow(wm.dpy, event->window);
            return;
        }
    }

    XGetWindowAttributes(wm.dpy, event->window, &window_attrs);

    // Frame/border for the window
    frame = XCreateSimpleWindow(wm.dpy, wm.root,
                                window_attrs.x, window_attrs.y,
                                window_attrs.width, window_attrs.height, 
                                border_width, border_color, background);

    // Make the frame the parent of the window.
    // Moving the frame will now also move the window
   XReparentWindow(wm.dpy, event->window, frame, 0, 0); 
   LOG("Mapping window: %zu", event->window);
   XMapWindow(wm.dpy, event->window); // Make the window(s) visible
   XMapWindow(wm.dpy, frame);

   XSelectInput(wm.dpy, frame, SubstructureNotifyMask | SubstructureRedirectMask);
   XSync(wm.dpy, True);

   new_client = (client_t*)malloc(sizeof(client_t));

   // New clients become the new head of the linked list
   // The previous head is pushed 1 position back
   new_client->window = event->window;
   new_client->frame = frame;
   wm.head = new_client;
   new_client->next = prev_head;
   
   wm.clients++;
   wm.focus = wm.head; // New window automatically gains focus

   default_tiling_layout();
}

// TODO
void handle_unmap_notify(XEvent *ev)
{
    XUnmapEvent *event = &ev->xunmap;
    (void)event;
    LOG("Unmap");
//    XUnmapWindow(wm.dpy, event->window);
}

// TODO use XSendEvent instead of XDestroy and rely on unmap notify
void handle_destroy(XEvent *ev)
{
    LOG("DESTROY");
    XDestroyWindowEvent *event = &ev->xdestroywindow;

    client_t* prev = {0};
    client_t* client = client_from_window(event->window, &prev);
    if(client == NULL)
        return;

 //   XUnmapWindow(wm.dpy, client->frame);
//    XUnmapWindow(wm.dpy, client->window);

    if(wm.head->frame == client->frame)
    {
        client_t* new_head = client->next;
        free(client);
        wm.head = new_head;
    }else{
        prev->next = client->next;
        free(client);
    }
    wm.focus = prev;
    XSetInputFocus(wm.dpy, prev->window, RevertToPointerRoot, CurrentTime);
    wm.clients--;
    default_tiling_layout();
}

// NOTE: useless right now, might be work making fuctional
// if going to add floating window support
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

//    XConfigureWindow(wm.dpy, event.window, event.value_mask, &changes);

}

// NOTE: Keys must be pressed with modifier key(Left Alt)
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
            exec("/usr/bin/xterm"); // Spawns xterm
            break;
        case XK_q:
            close_client(wm.focus);
            break;
        case XK_j:
            focus_next();
            break;
        case XK_i:
            update_masters(1); // Increase master window count by 1
            break;
        case XK_d:
            update_masters(-1);
            break;
        case XK_x:
            wm.running = false;
            break;
        default:
            break;
    }
}

// NOTE: practical for debugging
void handle_motion(XEvent *ev)
{
    XMotionEvent *event = &ev->xmotion;

    if(wm.head != NULL)
        XMoveWindow(wm.dpy, wm.head->window, event->x_root, event->y_root);
}


int main(void)
{
    // Basic WM setup
    wm.dpy = XOpenDisplay(NULL);
    wm.root = DefaultRootWindow(wm.dpy);
    wm.running = true;
    wm.head = NULL;
    wm.masters = 1;
    wm.clients = 0;
    XFlush(wm.dpy);

    // WM needs to intercept all events coming to the X server from applications
    XSelectInput(wm.dpy, wm.root, SubstructureNotifyMask | SubstructureRedirectMask);

    // Grabbing keyboard and mouse inputs
    XGrabKey(wm.dpy, AnyKey, Mod1Mask, wm.root, True, GrabModeAsync, GrabModeAsync);
    XGrabButton(wm.dpy, AnyButton, Mod1Mask, wm.root, True, PointerMotionMask, GrabModeAsync, GrabModeAsync, wm.root, None);

    XEvent ev = {0};
    XSync(wm.dpy, False); // Sync for good measure
    while(wm.running)
    {
        XNextEvent(wm.dpy, &ev);
        // call the function in the look up table
        // and provide a pointer to the event as the arg.
        // The last index in the array is 24 and all elements
        // that contain a function pointer are non-zero
        if(ev.type < 24 && event_lookup_table[ev.type] != 0)
            event_lookup_table[ev.type](&ev); // Call the function and provide pointer to the XEvent as arg
    }

    return 0;
}

// Will organize windows into the default master-slave layout
// The master window will be on the left and take up 50% of the screen
// The slave windows will stack on top of eachother on the right
// It is possible to increase the number of masters by incrementing wm.masters
void default_tiling_layout(void)
{
    client_t* client = wm.head;

    int master_width = 2;
    
    // If master is the only window or there are as many masters as there
    // are clients, then the master window(s) should take up the whole width
    // of the screen
    if(wm.clients == wm.masters)
        master_width = 1;

    for(int i = 0; i < wm.masters && client != NULL; i++)
    {
        XMoveResizeWindow(wm.dpy, client->frame,
                0, (screen_height/wm.masters)*i,
                screen_width/master_width, screen_height/wm.masters);
        XMoveResizeWindow(wm.dpy, client->window,
                0, 0, // Never move the window, only move the frame, learned that the hard way
                screen_width/master_width, screen_height/wm.masters);
        client = client->next;
    }

    for(int i = 0; i < wm.clients - wm.masters && client != NULL; i++)
    {
        // Height of each slave = screen height / total amount of slaves
        XMoveResizeWindow(wm.dpy, client->frame,
                screen_width/2, (screen_height/(wm.clients-wm.masters))*i,
                screen_width/2, screen_height/(wm.clients-wm.masters));
        XMoveResizeWindow(wm.dpy, client->window,
                0, 0,
                screen_width/2, screen_height/(wm.clients-wm.masters));
        client = client->next;
    }
}

void focus_next(void)
{
    // If at bottom of stack, go back to top
    if(wm.focus->next == NULL)
        wm.focus = wm.head;
    else
        wm.focus = wm.focus->next;

    XSetInputFocus(wm.dpy, wm.focus->window, RevertToPointerRoot, CurrentTime);
}
// TODO
void focus_prev(void)
{

//    XSetInputFocus(wm.dpy, wm.focus->window, RevertToPointerRoot, CurrentTime);
}

// Increment or decrement the total amont of masters
void update_masters(int change)
{
    if(wm.masters + change < 1)
        return;
    else if(wm.masters + change > wm.clients)
        return;

    wm.masters += change;

    default_tiling_layout();
}

// Client from frame or from window simply returns a pointer to
// a client(window - frame pair) from either the frame or the window
client_t* client_from_frame(Window frame, client_t **ret_prev)
{
    client_t* prev = NULL;
    for(client_t *client = wm.head; client != NULL; client = client->next)
    {
        if(frame == client->frame)
        {
            if(ret_prev != NULL)
                *ret_prev = prev;
            return client;
        }
        prev = client;
    }

    return NULL;
}

client_t* client_from_window(Window window, client_t **ret_prev)
{

    client_t* prev = NULL;
    for(client_t *client = wm.head; client != NULL; client = client->next)
    {
        if(window == client->window)
        {
            if(ret_prev != NULL)
                *ret_prev = prev;
            return client;
        }
        prev = client;
    }

    return NULL;
}

// TODO: Use XSendEvent instead of XDestroyWindow
void close_client(client_t *client)
{
    LOG("Close client");
    XEvent event;
    event.xclient.type = ClientMessage;
    event.xclient.window = wm.head->frame;
    event.xclient.message_type = XInternAtom(wm.dpy, "WM_PROTOCOLS", True);
    event.xclient.format = 32;
    event.xclient.data.l[0] = XInternAtom(wm.dpy, "WM_DELETE_WINDOW", False);
    event.xclient.data.l[1] = CurrentTime;
    (void)event;
    
    XDestroyWindow(wm.dpy, client->frame);

//    XSendEvent(wm.dpy, wm.head->frame, False, NoEventMask, &event);

}

