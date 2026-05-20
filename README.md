# screenshooter_for_X11
Screenshooter for X11 with GUI - GTK

# Compiling:

gcc main.c -o screenshot_app `pkg-config --cflags --libs gtk+-3.0 gdk-x11-3.0 x11` -lXfixes


# Running:
./screenshot_app

# Description
This is a small but usefull program for capturing a screen(X11), actual window or a region of the screen to a file.
There are formats like: jpg, png, bmp, webp, avif
There Delay before capturing in seconds.
And user can choose an options with mouse pointer and/or window border.
On bottom is a Save to path entry row.

Author
Stanislav Petrek

Thank you
