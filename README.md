# screenshooter_for_X11
Screenshooter for X11 with GUI - GTK
<br>
## Description
This is a small but usefull program for capturing a screen(X11), actual window or a region of the screen to a file.
There are formats like: jpg, png, bmp, webp, avif
There Delay before capturing in seconds.
And user can choose an options with mouse pointer and/or window border.
On bottom is a Save to path entry row.

<br>

## Installing:

### Debian / Ubuntu / Linux Mint
sudo apt update
sudo apt install build-essential pkg-config libgtk-3-dev libx11-dev libxfixes-dev libgdk-pixbuf2.0-bin webp-pixbuf-loader libavif-gdk-pixbuf

### Arch Linux / Manjaro / CachyOS
sudo pacman -Syu
sudo pacman -S base-devel pkg-config gtk3 libx11 libxfixes gdk-pixbuf2 webp-pixbuf-loader libavif

### Fedora / Nobara / Bazzite
sudo dnf check-update
sudo dnf groupinstall "Development Tools"
sudo dnf install pkgconf-pkg-config gtk3-devel libX11-devel libXfixes-devel gdk-pixbuf2 webp-pixbuf-loader libavif

<br>

## Compiling:
gcc main.c -o screenshot_app `pkg-config --cflags --libs gtk+-3.0 gdk-x11-3.0 x11` -lXfixes

## Running:
./screenshot_app

<br>
<br>

### Author
Stanislav Petrek, 19. may 2026

Thank you
