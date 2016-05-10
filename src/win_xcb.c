#include <xcb/xcb.h>
#include <xcb/xcb_event.h>
#include <xcb/xcb_atom.h>
#include <sys/types.h>
#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <xcb/xcb_icccm.h>
#include "x11fs.h"

//Our connection to xcb and our screen
static xcb_connection_t *conn;
static xcb_screen_t *scrn;
static xcb_window_t clip_wid;

//Setup our connection to the X server and get the first screen
//TODO: Check how this works with multimonitor setups
X11FS_STATUS xcb_init()
{
	conn = xcb_connect(NULL, NULL);
	if(xcb_connection_has_error(conn)){
		warnx("Cannot open display: %s", getenv("DISPLAY"));
		return X11FS_FAILURE;
	}
  
	scrn = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
  
  if(!scrn){
		warnx("Cannot retrieve screen information");
		return X11FS_FAILURE;
	}
	return X11FS_SUCCESS;
}

//End our connection
void xcb_cleanup(){
	if(conn)
		xcb_disconnect(conn);
}

//check if a window exists
bool exists(int wid)
{
	xcb_get_window_attributes_cookie_t attr_c = xcb_get_window_attributes(conn, wid);
	xcb_get_window_attributes_reply_t *attr_r = xcb_get_window_attributes_reply(conn, attr_c, NULL);

	if(!attr_r)
		return false;

	free(attr_r);
	return true;
}

//List every open window
int *list_windows()
{
	//Get the window tree for the root window
	xcb_query_tree_cookie_t tree_c = xcb_query_tree(conn, scrn->root);
	xcb_query_tree_reply_t *tree_r = xcb_query_tree_reply(conn, tree_c, NULL);

	if(!tree_r)
		warnx("Couldn't find the root window's");

	//Get the array of windows
	xcb_window_t *xcb_win_list = xcb_query_tree_children(tree_r);
	if(!xcb_win_list)
		warnx("Couldn't find the root window's children");

	int *win_list = malloc(sizeof(int)*(tree_r->children_len+1));
	int i;
	for (i=0; i<tree_r->children_len; i++) {
		 win_list[i] = xcb_win_list[i];
	}

	free(tree_r);

	//Null terminate our list
	win_list[i]=0;
	return win_list;
}

static xcb_atom_t xcb_atom_get(xcb_connection_t *conn, char *name)
{
	xcb_intern_atom_cookie_t cookie = xcb_intern_atom(conn ,0, strlen(name), name);
	xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(conn, cookie, NULL);
	return !reply ? XCB_ATOM_STRING : reply->atom;
}

void close_window(int wid)
{
	xcb_icccm_get_wm_protocols_reply_t reply;
	bool supports_delete = false;
	if (xcb_icccm_get_wm_protocols_reply(conn, xcb_icccm_get_wm_protocols(conn, wid, xcb_atom_get(conn, "WM_PROTOCOLS")), &reply, NULL)) {
		for (int i = 0; i != reply.atoms_len; ++i){
			if(reply.atoms[i] == xcb_atom_get(conn, "WM_DELETE_WINDOW")){
				supports_delete=true;
				break;
			}
		}
	}
	if(supports_delete){
		xcb_client_message_event_t ev;
		ev.response_type = XCB_CLIENT_MESSAGE;
		ev.sequence = 0;
		ev.format = 32;
		ev.window = wid;
		ev.type = xcb_atom_get(conn, "WM_PROTOCOLS");
		ev.data.data32[0] = xcb_atom_get(conn, "WM_DELETE_WINDOW");
		ev.data.data32[1] = XCB_CURRENT_TIME;

		xcb_send_event(conn, 0, wid, XCB_EVENT_MASK_NO_EVENT, (char *)&ev);
	}else
		xcb_kill_client(conn, wid);
	xcb_flush(conn);
}

//Get the focused window
int focused()
{
	//Ask xcb for the focused window
	xcb_get_input_focus_cookie_t focus_c;
	xcb_get_input_focus_reply_t *focus_r;

	focus_c = xcb_get_input_focus(conn);
	focus_r = xcb_get_input_focus_reply(conn, focus_c, NULL);

	//Couldn't find the focused window
	if(!focus_r)
		return -1;

	int focused = focus_r->focus;
	if(focused==scrn->root)
		focused=0;
	free(focus_r);
	return focused;
}

//Change the focus
void focus(int wid)
{
	xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, wid, XCB_CURRENT_TIME);
	xcb_flush(conn);
}

//Get the properties of a window (title, class etc)
static xcb_get_property_reply_t *get_prop(int wid, xcb_atom_t property, xcb_atom_t type)
{
	xcb_get_property_cookie_t prop_c = xcb_get_property(conn, 0, wid, property, type, 0L, 32L);
	return xcb_get_property_reply(conn, prop_c, NULL);
}

//Get the geometry of a window
static xcb_get_geometry_reply_t *get_geom(int wid)
{
	xcb_get_geometry_cookie_t geom_c = xcb_get_geometry(conn, wid);
	return xcb_get_geometry_reply(conn, geom_c, NULL);
}

//Get the attributes of a window (mapped, ignored etc)
static xcb_get_window_attributes_reply_t *get_attr(int wid)
{
	xcb_get_window_attributes_cookie_t attr_c = xcb_get_window_attributes(conn, wid);
	return xcb_get_window_attributes_reply(conn, attr_c, NULL);
}

//Bunch of functions to get and set window properties etc.
//All should be fairly self explanatory

#define DEFINE_NORM_SETTER(name, fn, prop) \
void set_##name(int wid, int arg) {\
	uint32_t values[] = {arg};\
	fn(conn, wid, prop, values);\
	xcb_flush(conn);\
}

DEFINE_NORM_SETTER(border_width, xcb_configure_window,         XCB_CONFIG_WINDOW_BORDER_WIDTH);
DEFINE_NORM_SETTER(border_color, xcb_change_window_attributes, XCB_CW_BORDER_PIXEL);
DEFINE_NORM_SETTER(ignored,      xcb_change_window_attributes, XCB_CW_OVERRIDE_REDIRECT);
DEFINE_NORM_SETTER(width,        xcb_configure_window,         XCB_CONFIG_WINDOW_WIDTH);
DEFINE_NORM_SETTER(height,       xcb_configure_window,         XCB_CONFIG_WINDOW_HEIGHT);
DEFINE_NORM_SETTER(x,            xcb_configure_window,         XCB_CONFIG_WINDOW_X);
DEFINE_NORM_SETTER(y,            xcb_configure_window,         XCB_CONFIG_WINDOW_Y);
DEFINE_NORM_SETTER(stack_mode,   xcb_configure_window,         XCB_CONFIG_WINDOW_STACK_MODE);
DEFINE_NORM_SETTER(subscription, xcb_change_window_attributes, XCB_CW_EVENT_MASK);

#define DEFINE_GEOM_GETTER(name) \
int get_##name(int wid)\
{\
	if(wid==-1)\
		wid=scrn->root;\
	xcb_get_geometry_reply_t *geom_r = get_geom(wid);\
	if(!geom_r)\
	return -1;\
	\
	int name = geom_r->name;\
	free(geom_r);\
	return name;\
}

DEFINE_GEOM_GETTER(width);
DEFINE_GEOM_GETTER(height);
DEFINE_GEOM_GETTER(x);
DEFINE_GEOM_GETTER(y);
DEFINE_GEOM_GETTER(border_width);

int get_mapped(int wid)
{
    xcb_get_window_attributes_reply_t *attr_r = get_attr(wid);
    if(!attr_r)
        return -1;

    int map_state = attr_r->map_state;
    free(attr_r);
    return map_state == XCB_MAP_STATE_VIEWABLE;
}

void set_mapped(int wid, int mapstate)
{
    if(mapstate)
        xcb_map_window(conn, wid);
    else
        xcb_unmap_window(conn, wid);
    xcb_flush(conn);
}

int get_ignored(int wid)
{
    xcb_get_window_attributes_reply_t *attr_r = get_attr(wid);
    if(!attr_r)
        return -1;

    int or = attr_r->override_redirect;
    free(attr_r);
    return or;
}

char *get_title(int wid)
{
    xcb_get_property_reply_t *prop_r = get_prop(wid, XCB_ATOM_WM_NAME, XCB_ATOM_STRING);
    if(!prop_r)
        return NULL;

    char *title = (char *) xcb_get_property_value(prop_r);
    int len = xcb_get_property_value_length(prop_r);
    char *title_string=malloc(len+1);
    sprintf(title_string, "%.*s", len, title);
    free(prop_r);
    return title_string;
}

//Get an array of the classes of the window
char **get_class(int wid)
{
    char **classes = malloc(sizeof(char*)*2);
    xcb_get_property_reply_t *prop_r = get_prop(wid, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING);
    if(!prop_r) {
        free(classes);
        return NULL;
    }

    char *class;
    class=(char *) xcb_get_property_value(prop_r);
    classes[0]=strdup(class);
    classes[1]=strdup(class+strlen(class)+1);

    free(prop_r);
    return classes;
}

#define subscribe(wid) set_subscription(wid, XCB_EVENT_MASK_ENTER_WINDOW|XCB_EVENT_MASK_LEAVE_WINDOW)
#define unsubscribe(wid) set_subscription(wid, XCB_EVENT_MASK_NO_EVENT)

//Get events for a window
char *get_events(){
	//Subscribe to events from all windows
	uint32_t values[] = {XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY};
	xcb_change_window_attributes(conn, scrn->root, XCB_CW_EVENT_MASK, values);
	int *windows=list_windows();
	int wid;
	while((wid=*(windows++))){
		if(!get_ignored(wid))
			subscribe(wid);
	}

	char *event_string;
	bool done = false;
	while(!done){
		xcb_generic_event_t *event = xcb_wait_for_event(conn);
		int wid;
		const char * ev_id = NULL;
		switch (event->response_type & ~0x80){
			case XCB_CREATE_NOTIFY:
				ev_id = "CREATE";
				wid = ((xcb_create_notify_event_t * )event)->window;
				break;

			case XCB_DESTROY_NOTIFY:
				ev_id = "DESTROY";
				wid = ((xcb_destroy_notify_event_t * )event)->window;
				break;

			case XCB_ENTER_NOTIFY:
				ev_id = "ENTER";
				wid = ((xcb_enter_notify_event_t * )event)->event;
				break;

			case XCB_LEAVE_NOTIFY:
				ev_id = "LEAVE";
				wid = ((xcb_leave_notify_event_t * )event)->event;
				break;

			case XCB_MAP_NOTIFY:
				ev_id = "MAP";
				wid = ((xcb_map_notify_event_t * )event)->window;
				break;

			case XCB_UNMAP_NOTIFY:
				ev_id = "UNMAP";
				wid = ((xcb_unmap_notify_event_t * )event)->window;
				break;
		}

		if ( ev_id ) {
			event_string = malloc(snprintf(NULL, 0, "%s: 0x%08x\n", ev_id, wid) + 1);
			sprintf(event_string, "%s: 0x%08x\n", ev_id, wid);
			done = true;
		}
	}
	//Unsubscribe from events
	unsubscribe(scrn->root);
	while((wid=*(windows++))){
		unsubscribe(wid);
	}

	return event_string;
}

xcb_window_t create_clip_window() {
  clip_wid = xcb_generate_id(conn);
  uint32_t values[3] = { scrn->black_pixel, 1, XCB_EVENT_MASK_PROPERTY_CHANGE };
  uint32_t mask = 0; mask |= XCB_CW_BACK_PIXEL; mask |= XCB_CW_OVERRIDE_REDIRECT; mask |= XCB_CW_EVENT_MASK;
  
  xcb_create_window(conn, scrn->root_depth, clip_wid, scrn->root, 0, 0, 1, 1, 0, XCB_COPY_FROM_PARENT, scrn->root_visual, mask, values);
  
  xcb_map_window(conn, clip_wid);
  return clip_wid;
}

char *get_clip_selection(int wid, char * selection) {
  (void) wid;  
  xcb_atom_t xsel = xcb_atom_get(conn, selection); 
  xcb_atom_t utf = xcb_atom_get(conn, "UTF8_STRING");
  xcb_window_t clip_wid = create_clip_window();  

  /* Request data as a UTF8 string for selection passed in */
  xcb_convert_selection(conn, clip_wid, xsel, utf, xcb_atom_get(conn, "XSEL_DATA"), XCB_CURRENT_TIME); 
  xcb_flush(conn);
  
  char * data = 0; 

  while (1) {
    xcb_generic_event_t *e = xcb_wait_for_event(conn); 
   
    if(XCB_EVENT_RESPONSE_TYPE(e) == XCB_SELECTION_REQUEST)
      break;

    if(XCB_EVENT_RESPONSE_TYPE(e) != XCB_SELECTION_NOTIFY) {
      free(e);
      continue;
    }
    /* Returned from convert selection, if property is none == not UTF8 data, no owner, server had too little space */
    xcb_selection_notify_event_t * event = (xcb_selection_notify_event_t *) e;
  
    /* Wait for the convert selection event, handle, then delete */
    if (event->property) {
      xcb_get_property_reply_t * reply;
      xcb_get_property_cookie_t cookie;
      cookie = xcb_get_property_unchecked(conn, 0, event->requestor, event->property, utf, 0, UINT32_MAX);
      reply = xcb_get_property_reply(conn, cookie, 0);   
      void * value = xcb_get_property_value(reply);
      
      xcb_delete_property(conn, event->requestor, event->property);

      if (reply && value) {
        data = malloc(reply->value_len+1);
        snprintf(data, reply->value_len+1, "%s", (char * )value);
      }

      free(e);
      free(reply);
      return data; 
    } else {
      return data;
    }
  }
  return data;
}

void set_clip_selection(int wid, char * selection, const char * buf) {
  (void) buf; (void) wid;
  
  pid_t clip_pid = fork();
  if (clip_pid < 0)
    exit(EXIT_FAILURE);
  if (clip_pid == 0) {

    /* set up window */
    xcb_window_t clip_wid = create_clip_window();
  
    /* request ownership */
    xcb_atom_t xsel = xcb_atom_get(conn, selection);
    xcb_get_selection_owner_reply_t *owner;
    xcb_set_selection_owner(conn, clip_wid, xsel, XCB_CURRENT_TIME);
    owner = xcb_get_selection_owner_reply(conn, xcb_get_selection_owner(conn, xsel), NULL);
    if (!owner || owner->owner != clip_wid)
      warnx("Failed to set ownership\n");
    if (owner) free(owner);
  
    /* set the properties, then send event */
  
    /* Unleash the daemon! */
	  // poll for statuses like selection request, selection clear, and such
    // once the clip ownership is changed, we can exit.  
	  while(1) {
     	/* Convert selection indicates that you want the clip from the owner to be yours */
    	/* A convert request goes to the owner, and they have to handle the event sending the data requested back */
      
  	  /* The returning data will be in a selection notify event */
  	  /* It'll return type none as a property member if it cannot convert the data */
  	  /* If setselectionowner is called, a selectionclear event will be received indicating that it should clear anything that is highlighted */
		  /* You send a selection notification as a xcb_send_event */
  	  /* Owner of selection should send selectionnotify with xsendevent, with an event_mask of 0 */
  	  /* selection, target, and property members should be set to values received in the selection request event */
  	  /* property set to none if the conversion couldn't be made */
  	  /* Requestors delete data, owners delete all other stuff */
  	  /* Owner should watch the events in requestors `subscribe(wid of requestor)` for notify property events, to make sure that the property was deleted */
  	  /* Selectionclear event is received after something else becomes owner */
  	  /* To voluntarily relinquish ownership, set to none and send event, or close the window */
		  break;
    }
  exit(0);
  }
}
