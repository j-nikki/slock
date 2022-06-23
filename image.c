// code from here:
// https://stackoverflow.com/a/38298349

#include "image.h"

#include <X11/Xutil.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/shm.h>

void initimage(struct shmimage *image)
{
    image->ximage          = NULL;
    image->shminfo.shmaddr = (char *)-1;
}

void destroyimage(Display *dsp, struct shmimage *image)
{
    if (image->ximage) {
        XShmDetach(dsp, &image->shminfo);
        XDestroyImage(image->ximage);
        image->ximage = NULL;
    }

    if (image->shminfo.shmaddr != (char *)-1) {
        shmdt(image->shminfo.shmaddr);
        image->shminfo.shmaddr = (char *)-1;
    }
}

int createimage(Display *dsp, struct shmimage *image, int width, int height)
{
    // Create a shared memory area
    image->shminfo.shmid = shmget(IPC_PRIVATE, width * height * BPP, IPC_CREAT | 0606);
    if (image->shminfo.shmid == -1) {
        perror("slock");
        return false;
    }

    // Map the shared memory segment into the address space of this process
    image->shminfo.shmaddr = (char *)shmat(image->shminfo.shmid, 0, 0);
    if (image->shminfo.shmaddr == (char *)-1) {
        perror("slock");
        return false;
    }

    image->data             = (unsigned int *)image->shminfo.shmaddr;
    image->shminfo.readOnly = false;

    // Mark the shared memory segment for removal
    // It will be removed even if this program crashes
    shmctl(image->shminfo.shmid, IPC_RMID, 0);

    // Allocate the memory needed for the XImage structure
    image->ximage =
        XShmCreateImage(dsp, XDefaultVisual(dsp, XDefaultScreen(dsp)),
                        DefaultDepth(dsp, XDefaultScreen(dsp)), ZPixmap, 0, &image->shminfo, 0, 0);
    if (!image->ximage) {
        destroyimage(dsp, image);
        fprintf(stderr, "slock: could not allocate the XImage structure\n");
        return false;
    }

    image->ximage->data   = (char *)image->data;
    image->ximage->width  = width;
    image->ximage->height = height;

    // Ask the X server to attach the shared memory segment and sync
    XShmAttach(dsp, &image->shminfo);
    XSync(dsp, false);
    return true;
}
