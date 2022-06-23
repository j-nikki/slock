#pragma once

#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>

#define BPP 4

#ifdef __cplusplus
extern "C" {
#endif

struct shmimage {
    XShmSegmentInfo shminfo;
    XImage *ximage;
    unsigned int *data; // will point to the image's BGRA packed pixels
};
void initimage(struct shmimage *image);
void destroyimage(Display *dsp, struct shmimage *image);
int createimage(Display *dsp, struct shmimage *image, int width, int height);

#ifdef __cplusplus
}
#endif
