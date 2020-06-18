Differences
===========

This fork contains various differences to the [original](https://github.com/sdhand/x11fs). 
All differences will be tracked in this document.

fuse3
-----
This fork uses the FUSE3 library rather than FUSE2. They are not compatible.

config.h
--------
The directory structure and various other values are stored in `src/config.h`, rather than `src/x11fs.h`.

events
------
Events are in the format `EVENT WID`, as opposed to `EVENT: WID` in the original fork.

size, position, all
-------------------
Each window has a file that can control x and y at the same time. The same goes for width and height.
Additionally, there is a file that can control all 4.


