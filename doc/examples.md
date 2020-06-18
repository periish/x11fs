Examples
========

*Each of these scripts and configs are contained in `/doc/examples`, as well as in here.*

This setup assumes you are using `startx`, with `~/.xinitrc` available.

`sxhkd` is reccomended. 

Here's an example `sxhkdrc`:
```
# Open a terminal.

super + Return
    urxvt

# Move windows

super + {h,l}
    read -r WID < $X11FS/focused; \
    read -r POS < $X11FS/$WID/geometry/x; \
    echo "$(( POS {+,-} 6 ))" > $X11FS/$WID/geometry/x
super + {j,k}
    read -r WID < $X11FS/focused; \
    read -r POS < $X11FS/$WID/geometry/y; \
    echo "$(( POS {+,-} 6 ))" > $X11FS/$WID/geometry/y

# Resize windows 

super + shift + {h,l}
    read -r WID < $X11FS/focused; \
    read -r POS < $X11FS/$WID/geometry/height; \
    echo "$(( POS {+,-} 6 ))" > $X11FS/$WID/geometry/height
super + shift + {j,k}
    read -r WID < $X11FS/focused; \
    read -r POS < $X11FS/$WID/geometry/width; \
    echo "$(( POS {+,-} 6 ))" > $X11FS/$WID/geometry/width

# Kill focused window
super + q
    read -r WID < $X11FS/focused; \
    rm -r $X11FS/$WID
super + Esacpe
    pkill focuser
```

You'll need a script to focus windows, as well. This should go in your $PATH.
`focuser`
```sh
#!/bin/sh

is_ignored () {
    read -r status < $X11FS/$1/ignored
    case $status in
        true)  return 0 ;;
        false) return 1 ;;
    esac
}

cat $X11FS/event | while read -r ev wid; do
    case $ev in
        CREATE|MAP|ENTER) is_ignored "$wid" || echo "$wid" > $X11FS/focused
    esac
done
```

Finally, you can put this in your `~/.xinitrc`:
```sh
#!/bin/sh 

export X11FS=<your mointpoint here>
x11fs $X11FS
sxhkd &
focuser
fusermount3 -u $X11FS
```

Now you can start X with `startx`, spawn a terminal with super + Return, and move and resize windows with super (+ shift) +  hjkl. 
You can extend upon x11fs in many ways. Have fun with it!
