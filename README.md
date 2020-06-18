x11fs X window virtual filesystem
=================================

*This is heavily based off wmutils. [Check them out](https://github.com/wmutils).*

x11fs is a tool for manipulating X windows.
It creates a vitual filesystem to represent open windows, similar to what /proc does for processes.
This allows windows to be controlled using any language or tool with simple file IO, in a true unix fashion.


Build
-----

After installing the relevant developement packages for fuse and xcb for your distro (on Ubuntu these are `libxcb1-dev`, `libxcb-icccm4-dev` and `libfuse3-dev`), x11fs can be built using the `make` command.
Installation can be done by invoking `make install`.
On some operating systems, such as FreeBSD, you may need to invoke `gmake` instead of plain `make`.
By default, `make install` will install binaries into /usr/local/bin/, and documentation into /usr/local/share.

Usage
-----

See `/doc` in this repository for example setups and explanations.

Contact
-------

Please join #x11fs on freenode if you have any questions or just want to talk about the project. Please be patient however, there are not many members of the channel so it may be inactive at times.

Thanks to
---------

[Luiz de Milon](https://github.com/kori) for helping to come up with the initial idea.
The creators of [wmutils](https://github.com/wmutils) for providing some inspiration, and some basic xcb code to study.
