
#ifndef __XCOLOR_H__
#define __XCOLOR_H__

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <stdint.h>
#include <arpa/inet.h>
#include <uuid/uuid.h>

#include <stdlib.h>
#include <string.h>


typedef struct {
	uuid_t uuid;      /* UUID of the profile		        */
	uint32_t length;  /* number of bytes following		        */
} XcolorProfile;

typedef struct {
	uint32_t region;  /* XserverRegion				*/
	uuid_t uuid;      /* UUID of the associated profile	        */
} XcolorRegion;

int XcolorProfileUpload(Display *dpy, XcolorProfile *profile);
int XcolorProfileDelete(Display *dpy, XcolorProfile *profile);

int XcolorRegionInsert(Display *dpy, Window win, unsigned long pos, XcolorRegion *region, unsigned long nRegions);
XcolorRegion *XcolorRegionFetch(Display *dpy, Window win, unsigned long *nRegions);
int XcolorRegionDelete(Display *dpy, Window win, unsigned long start, unsigned long count);

#endif /* __XCOLOR_H__ */