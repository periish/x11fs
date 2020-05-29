#include "win_oper.h"

#define WRITEONLY 0200
#define READONLY  0400
#define READWRITE 0600
#define RWX       0700

#define D(p) { p, S_IFDIR | RWX, false, true,  NULL, NULL },
#define F(p, m, i, r, w) { p, S_IFREG | m, i, false, r, w },

// represents a single file, contains pointers to the functions to call to read and write for that file
struct x11fs_file{
	const char *path;
	int mode;
	bool direct_io;
	bool dir;
	char *(*read)(int wid);
	void (*write)(int wid, const char *buf);
};

// file layout
static const struct x11fs_file x11fs_files[] = {
	// path                     perm       io     onread                onwrite
	D( "/"                                                                                    )
	D( "/root"                                                                                )
	D( "/root/geometry"                                                                       )
	F( "/root/geometry/width",  READONLY,  false, root_width_read,      NULL                  )
	F( "/root/geometry/height", READONLY,  false, root_height_read,     NULL                  )
	D( "/0x*"                                                                                 )
	D( "/0x*/border"                                                                          )
	F( "/0x*/border/color",     WRITEONLY, false, NULL,                 border_color_write    )
	F( "/0x*/border/width",     READWRITE, false, border_width_read,    border_width_write    )
	D( "/0x*/geometry"                                                                        )
	F( "/0x*/geometry/width",   READWRITE, false, geometry_width_read,  geometry_width_write  )
	F( "/0x*/geometry/height",  READWRITE, false, geometry_height_read, geometry_height_write )
	F( "/0x*/geometry/x",       READWRITE, false, geometry_x_read,      geometry_x_write      )
	F( "/0x*/geometry/y",       READWRITE, false, geometry_y_read,      geometry_y_write      )
	F( "/0x*/mapped",           READWRITE, false, mapped_read,          mapped_write          )
	F( "/0x*/ignored",          READWRITE, false, ignored_read,         ignored_write         )
	F( "/0x*/stack",            WRITEONLY, false, NULL,                 stack_write           )
	F( "/0x*/title",            READONLY,  false, title_read,           NULL                  )
	F( "/0x*/class",            READONLY,  false, class_read,           NULL                  )
	F( "/focused",              READWRITE, false, focused_read,         focused_write         )
	F( "/event",                READONLY,  true,  event_read,           NULL                  )
};
