x11fs structure
===============

This document describes the structure of the x11fs filesystem.

```
/0x???????                       A directory representing a window. The directory name is the window ID.
/0x????????/border               A directory containing files relating to the window's border.
/0x????????/border/color         A write only file representing the window's border color.
/0x????????/border/width         A r/w file representing the window's border width.
/0x????????/geometry             A directory containing files relating to the window's geometry.
/0x????????/geometry/height      A r/w file representing the height of a window.
/0x????????/geometry/width       A r/w file representing the width of a window.
/0x????????/geometry/size        A r/w file representing both the height and width of a window.
/0x????????/geometry/x           A r/w file representing the x position of a window.
/0x????????/geometry/y           A r/w file representing the y position of a window.
/0x????????/geometry/position    A r/w file representing the x and y position of a window.
/0x????????/geometry/all         A r/w file representing the size and position of a window.
/0x????????/class                A r/o file containing the window's class.
/0x????????/ignored              A r/w file representing the window's ignored (overide_redirect) state.
/0x????????/mapped               A r/w file representing the window's mapping state (if it is visible or not).
/0x????????/stack                A write only file representing the window's stacking order.
/0x????????/title                A r/o file representing the window's title.
/root                            A directory representing the root window.
/root/geometry                   A directory representing the root window's geometry. 
/root/geometry/width             A r/o file containing the width of the root window (the screen width).
/root/geometry/height            A r/o file containing the height of the root window (the screen height).
/event                           A r/o file stream containing events.
/focused                         A r/w file representing the ID of the currently focused window.
```

Events
------
The `/event` file is a file stream - when read, it will never reach EOF. Thus, you can `cat` it for scripting purposes. 
The events come in the format `EVENT 0x??????`.
EVENT can be one of the following:
```
MAP       When a window is mapped(unhidden).
UNMAP     When a window is mapped(hidden).
CREATE    When a window is created.
DESTROY   When a window is destroyed.
ENTER     When the cursor enters a window.
```

Formats
-------
Certain files expect a format, else they'll fall back to a default value.
`/0x????????/geometry/size` expects values in the format `width height`.
`/0x????????/geometry/position` expects coordinates in the format `x y`. 
`/0x????????/geometry/all` expects values in the format `width height x y`.
Each of these files also outputs in this format.
`/0x????????/border/color` expects hex codes without a `#`. 
