#include <xcb/xcb.h>
#include <xcb/xcb_event.h>
#include <xcb/xcb_atom.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <err.h>
#include <errno.h>
#include <syslog.h>
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
	syslog(LOG_ERR, "Couldn't find the root windows in %s\n", __func__);
	//Get the array of windows
	xcb_window_t *xcb_win_list = xcb_query_tree_children(tree_r);
	if(!xcb_win_list)
		syslog(LOG_ERR, "Couldn't find the root windows children in: %s\n", __func__);

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
	if(!reply) {
		syslog(LOG_ERR, "Unable to get xcb_atom %s in %s\n", name, __func__);
	return XCB_ATOM_STRING;
	}
	return reply->atom;
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
  
  if (!xcb_flush(conn))
    syslog(LOG_ERR, "failed to flush connection on %s in %s\n", wid, __func__);
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
	if(!focus_r) {
		syslog(LOG_ERR, "Failed to find the focused window in %s\n", __func__);
	return -1;
  }

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
  if(!xcb_flush(conn))
    syslog(LOG_ERR, "failed to flush connection on %s in %s\n", wid, __func__);

}

//Get the properties of a window (title, class etc)
static xcb_get_property_reply_t *get_prop(int wid, xcb_atom_t property, xcb_atom_t type)
{
  xcb_get_property_cookie_t prop_c;
  prop_c = xcb_get_property(conn, 0, wid, property, type, 0L, 32L);
  
	xcb_get_property_reply_t *reply;
  reply = xcb_get_property_reply(conn, prop_c, NULL);
  
  if(!xcb_get_property_value_length(reply))
    syslog(LOG_ERR, "Failed to get property in %s\n", __func__);
  return reply;
}

//Get the geometry of a window
static xcb_get_geometry_reply_t *get_geom(int wid)
{
	xcb_get_geometry_cookie_t geom_c;
  geom_c = xcb_get_geometry(conn, wid);
  
  xcb_get_geometry_reply_t *reply;
  if(!(reply = xcb_get_geometry_reply(conn, geom_c, NULL)))
    syslog(LOG_ERR, "Failed to get geometry in %s\n", __func__);

  return reply;
}

//Get the attributes of a window (mapped, ignored etc)
static xcb_get_window_attributes_reply_t *get_attr(int wid)
{
	xcb_get_window_attributes_cookie_t attr_c;
  attr_c = xcb_get_window_attributes(conn, wid);
  
  xcb_get_window_attributes_reply_t *reply;
  if(!(reply = xcb_get_window_attributes_reply(conn, attr_c, NULL)))
    syslog(LOG_ERR, "Failed to get attribute in %s\n", __func__);
  return reply;
}
//Bunch of functions to get and set window properties etc.
//All should be fairly self explanatory

#define DEFINE_NORM_SETTER(name, fn, prop) \
void set_##name(int wid, int arg) {\
	uint32_t values[] = {arg};\
	fn(conn, wid, prop, values);\
	if(!xcb_flush(conn))\
    syslog(LOG_ERR, "Failed to flush connection in %s\n", __func__);\
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
  if(!attr_r) {
    syslog(LOG_ERR, "Failed to get %s attribute in %s: \n", wid, __func__);  
    return -1;
  }

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
    /* TODO: Handle INCR target types */

    xcb_generic_event_t *e = xcb_wait_for_event(conn); 
    
    if(XCB_EVENT_RESPONSE_TYPE(e) != XCB_SELECTION_NOTIFY) {
      free(e);
      continue;
    }
    /* Returned from convert selection, if property is none == not UTF8 data, no owner, or server had too little space */
    xcb_selection_notify_event_t * event = (xcb_selection_notify_event_t *) e;
  
    /* Wait for the convert selection event, handle, then delete */
    if (event->property) {
      xcb_get_property_reply_t * reply;
      xcb_get_property_cookie_t cookie;
      cookie = xcb_get_property_unchecked(conn, 0, event->requestor, event->property, XCB_GET_PROPERTY_TYPE_ANY, 0, UINT32_MAX);
      reply = xcb_get_property_reply(conn, cookie, 0);   
      void * value = xcb_get_property_value(reply);
      
      xcb_delete_property(conn, event->requestor, event->property);
      if (reply && value) {
        data = malloc(reply->value_len+1);
        snprintf(data, reply->value_len+1, "%s", (char * )value);
      }

      free(e);
      free(reply);
      xcb_flush(conn);
      xcb_destroy_window(conn, clip_wid);
      return data; 
    } else {
      xcb_destroy_window(conn, clip_wid);
      return data;
    }
  }
  return data;
}

void set_clip_selection(int wid, char * selection, const char * buf) {
  (void) wid;
   
  /* kill any old processes */
  wait(0);
  
  /* atoms we'll need */
  xcb_atom_t utf = xcb_atom_get(conn, "UTF8_STRING");
  xcb_atom_t str = xcb_atom_get(conn, "STRING");
  xcb_atom_t xsel = xcb_atom_get(conn, selection);
  xcb_atom_t text = xcb_atom_get(conn, "TEXT");
  xcb_atom_t timestamp = xcb_atom_get(conn, "TIMESTAMP");
  xcb_atom_t intatom = xcb_atom_get(conn, "INTEGER");  
  
  /* set up window */
  xcb_window_t clip_wid = create_clip_window();

  /* Set up our data */
  char * data = malloc(sizeof(buf)+1);
  memcpy(data, buf, sizeof(buf)+1);

  /* request ownership */
  xcb_get_selection_owner_reply_t *owner;
  xcb_set_selection_owner(conn, clip_wid, xsel, XCB_CURRENT_TIME);
  owner = xcb_get_selection_owner_reply(conn, xcb_get_selection_owner(conn, xsel), NULL);
  if (!owner || owner->owner != clip_wid)
    syslog(LOG_ERR, "Failed to set ownership in %s\n", __func__);
  if (owner) free(owner);
  
  xcb_flush(conn);

  pid_t clip_pid = fork();
  
  /* If error, exit would unmount the fuse system */
  if (clip_pid == 0) {
    
    /* Unleash the daemon! */
	  while(1) {
      xcb_generic_event_t *e = xcb_wait_for_event(conn); 
       
      /* Request for our data */
      if(XCB_EVENT_RESPONSE_TYPE(e) == XCB_SELECTION_REQUEST) {
        xcb_selection_request_event_t * ev = (xcb_selection_request_event_t*)e;
        free(e);

        syslog(LOG_ERR, "We are here in XCB_SELECTION_REQUEST target %s\n", ev->target);
        
        if (ev->target == utf) {
          xcb_change_property(conn, XCB_PROP_MODE_REPLACE, ev->requestor, ev->property, utf, 8, sizeof(data), data);
        } else if (ev->target == str) {
          xcb_change_property(conn, XCB_PROP_MODE_REPLACE, ev->requestor, ev->property, str, 8, sizeof(data), data);
        }  else if (ev->target == text) {
          xcb_change_property(conn, XCB_PROP_MODE_REPLACE, ev->requestor, ev->property,  str, 8, sizeof(data), data);
        } else if (ev->target == timestamp) { 
          xcb_change_property(conn, XCB_PROP_MODE_REPLACE, ev->requestor, ev->property, intatom, 32, 1, (const char *)ev->time);
        }

        xcb_flush(conn);
        free(ev);
      }

      /* Request for ownership */
      if(XCB_EVENT_RESPONSE_TYPE(e) == XCB_SELECTION_CLEAR) {
        xcb_selection_clear_event_t *ev = (xcb_selection_clear_event_t*)e;
        free(e);
        syslog(LOG_ERR, "We are here XCB_SELECTION_CLEAR\n");
        if (ev->selection == xsel) {
          break;
        }
      }

      if(XCB_EVENT_RESPONSE_TYPE(e) == XCB_PROPERTY_NOTIFY) {
        xcb_property_notify_event_t *ev = (xcb_property_notify_event_t*)e;
        free(e);
        if (ev->state == XCB_PROPERTY_DELETE) { 
          syslog(LOG_ERR, "This is a delete event\n");
          xcb_change_property(conn, XCB_PROP_MODE_REPLACE, ev->window, ev->atom, utf, 8, 0, (const void *)0); 
          xcb_flush(conn);
          free(ev);
        }
      }
      
      if(XCB_EVENT_RESPONSE_TYPE(e) == XCB_SELECTION_NOTIFY) {
        syslog(LOG_ERR, "This is a selection notify event\n");
        free(e);
      }
      
      free(e);
    }

    /* Clean up */
    xcb_destroy_window(conn, clip_wid);
    _exit(0);
  }
}
