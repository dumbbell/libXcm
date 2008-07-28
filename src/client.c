
#define _BSD_SOURCE

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysymdef.h>

#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrandr.h>

#include <cairo.h>
#include <cairo-xlib.h>

#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <Xcolor.h>

static void *readFile(const char *path, unsigned long *nBytes)
{
	int fd = open(path, O_RDONLY);

	struct stat buf;
	fstat(fd, &buf);

	void *ret = malloc(buf.st_size);
	read(fd, ret, buf.st_size);

	close(fd);

	*nBytes = buf.st_size;

	return ret;	
}

int main(int argc, char *argv[])
{
	Display *dpy = XOpenDisplay(NULL);

	int screen = DefaultScreen(dpy);
	Visual *vis = DefaultVisual(dpy, screen);

	Colormap cmap = XCreateColormap(dpy, RootWindow(dpy, screen), vis, AllocNone);

	XSetWindowAttributes attrs;
	attrs.colormap = cmap;
	attrs.border_pixel = 0;
	attrs.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask;

	Window w = XCreateWindow(dpy, XRootWindow(dpy, screen), 200, 200, 300, 300, 5, 24,
				 InputOutput, DefaultVisual(dpy, screen),
				 CWBorderPixel | CWColormap | CWEventMask, &attrs);

	XMapWindow(dpy, w); 

	cairo_surface_t *surface = cairo_xlib_surface_create(dpy, w, DefaultVisual(dpy, screen), 300, 300);
	cairo_t *cr = cairo_create(surface);

	cairo_scale(cr, 300, 300);


	/**
	 * Color management
	 */

	unsigned long nBytes;
	void *blob = readFile("profile.icc", &nBytes);

	/* Create a XcolorProfile object that will be uploaded to the display */
	XcolorProfile *profile = malloc(sizeof(XcolorProfile) + nBytes);

	/* Fake MD5, real code should extract the MD5 from the ICC profile. See
	 * for example oyProfileGetMD5_() */
	for (int i = 0; i < 16; ++i)
		profile->md5[i] = i;

	profile->length = nBytes;
	memcpy(profile + 1, blob, nBytes);

	XcolorProfileUpload(dpy, profile);

	/* upload regions to the window*/
	XRectangle rec[2] = { { 50, 200, 80, 50 }, { 100, 100, 150, 100 } };
	XserverRegion reg = XFixesCreateRegion(dpy, rec, 2);

	XcolorRegion region;
	region.region = reg;
	memcpy(region.md5, profile->md5, 16);

	XcolorRegionInsert(dpy, w, 0, &region, 1);

	/* Here the code fetches the names of all outputs. Later the target can be changed
	 * by a key press. */
	XRRScreenResources *res = XRRGetScreenResources(dpy, XRootWindow(dpy, screen));

	unsigned long activeOutput = 0, nOutputs = res->noutput;
	char *outputName[nOutputs];

	for (unsigned long i = 0; i < nOutputs; ++i) {
		XRROutputInfo *oinfo = XRRGetOutputInfo(dpy, res, res->outputs[i]);
		outputName[i] = strdup(oinfo->name);
		XRRFreeOutputInfo(oinfo);
	}

	XRRFreeScreenResources(res);

	Atom netColorTarget = XInternAtom(dpy, "_NET_COLOR_TARGET", False);
	XChangeProperty(dpy, w, netColorTarget, XA_STRING, 8, PropModeReplace, (unsigned char *) outputName[activeOutput], 4);

	/* When the escape key is pressed, the application cleans up all resources and exits. */
	KeyCode escape = XKeysymToKeycode(dpy, XStringToKeysym("Escape"));

	/* When the delete key is pressed, the application switches the target output. */
        KeyCode delete = XKeysymToKeycode(dpy, XStringToKeysym("Delete"));


	for (;;) {
		XEvent event;
		XNextEvent(dpy, &event);

		if (event.type == Expose) {
			cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
			cairo_paint(cr);

			cairo_set_source_rgb(cr, 0, 0, 0);
			cairo_move_to(cr, 0, 0);
			cairo_line_to(cr, 1, 1);
			cairo_move_to(cr, 1, 0);
			cairo_line_to(cr, 0, 1);
			cairo_set_line_width(cr, 0.2);
			cairo_stroke(cr);

			cairo_rectangle(cr, 0, 0, 0.5, 0.5);
			cairo_set_source_rgba(cr, 1, 0, 0, 0.80);
			cairo_fill(cr);

			cairo_rectangle(cr, 0, 0.5, 0.5, 0.5);
			cairo_set_source_rgba(cr, 0, 1, 0, 0.60);
			cairo_fill(cr);

			cairo_rectangle(cr, 0.5, 0, 0.5, 0.5);
			cairo_set_source_rgba(cr, 0, 0, 1, 0.40);
			cairo_fill(cr);
		} else if (event.type == KeyPress) {
			if (event.xkey.keycode == escape) {
				break;
			} else if (event.xkey.keycode == delete) {
				if (++activeOutput == nOutputs)
					activeOutput = 0;

				XChangeProperty(dpy, w, netColorTarget, XA_STRING, 8, PropModeReplace, (unsigned char *) outputName[activeOutput], strlen(outputName[activeOutput]));

				printf("Changed target output to %s\n", outputName[activeOutput]);
			} else {
				XClientMessageEvent xev;
				static long enable = 0;

				xev.type = ClientMessage;
				xev.window = w;
				xev.message_type = XInternAtom(dpy, "_NET_COLOR_MANAGEMENT", False);
				xev.format = 32;
				
				++enable;
				xev.data.l[0] = 0;
				xev.data.l[1] = enable % 2;

				XSendEvent(dpy, RootWindow(dpy, screen), False, ExposureMask, (XEvent *) &xev);
				
				printf("Sent color manangement request: %li\n", enable % 2);
			}
		}
	}

	/* Delete the profile. The ref-count inside the compiz plugin drops to zero
	 * and the profile resources will be freed. */
	profile->length = 0;
	XcolorProfileDelete(dpy, profile);

	XDestroyWindow(dpy, w);
	XCloseDisplay(dpy);

	return 0;
}

