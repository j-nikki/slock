/* See LICENSE file for license details. */
#define _XOPEN_SOURCE 500
#define LENGTH(X)     (sizeof X / sizeof X[0])

#include <X11/XF86keysym.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xinerama.h>
#include <X11/extensions/Xrandr.h>
#include <X11/keysym.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <functional>
#include <grp.h>
#include <linux/oom.h>
#include <pwd.h>
#include <shadow.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

#include "arg.h"
#include "image.h"
#include "threads.h"
#include "timer.h"
#include "util.h"

#define STRINGIFY(X)     STRINGIFY_exp(X)
#define STRINGIFY_exp(X) #X

char *argv0;

enum { BACKGROUND, INIT, INPUT, FAILED, NUMCOLS };

#include "../config.h"

timer g_t(false);

struct lock {
    shmimage shim;
    unsigned long colors[NUMCOLS];
    Window root, win;
    Pixmap pmap;
    Pixmap bgmap;
    Drawable drawable;
    GC gc;
    XineramaScreenInfo *xsi;
    int screen;
    int nxsi;
};

struct xrandr {
    int active;
    int evbase;
    int errbase;
};

[[noreturn]] static void die(const char *errstr, ...)
{
    va_list ap;

    va_start(ap, errstr);
    vfprintf(stderr, errstr, ap);
    va_end(ap);
    exit(1);
}

static void dontkillme(void)
{
    FILE *f;
    const char oomfile[] = "/proc/self/oom_score_adj";

    if (!(f = fopen(oomfile, "w"))) {
        if (errno == ENOENT) return;
        die("slock: fopen %s: %s\n", oomfile, strerror(errno));
    }
    fprintf(f, "%d", OOM_SCORE_ADJ_MIN);
    if (fclose(f)) {
        if (errno == EACCES)
            die("slock: unable to disable OOM killer. "
                "Make sure to suid or sgid slock.\n");
        else
            die("slock: fclose %s: %s\n", oomfile, strerror(errno));
    }
}

static const char *gethash(void)
{
    const char *hash;
    struct passwd *pw;

    /* Check if the current user has a password entry */
    errno = 0;
    if (!(pw = getpwuid(getuid()))) {
        if (errno)
            die("slock: getpwuid: %s\n", strerror(errno));
        else
            die("slock: cannot retrieve password entry\n");
    }
    hash = pw->pw_passwd;

    if (!strcmp(hash, "x")) {
        struct spwd *sp;
        if (!(sp = getspnam(pw->pw_name)))
            die("slock: getspnam: cannot retrieve shadow entry. "
                "Make sure to suid or sgid slock.\n");
        hash = sp->sp_pwdp;
    }

    return hash;
}

enum { SHADOW_OFF = 5 };

static void resizerectangles(XRectangle *rs, int xoff, int yoff, int mw, int mh)
{
    for (int i = 0; i < (int)LENGTH(rectangles); i++) {
        rs[i].x =
            (rectangles[i].x * logosize) + xoff + ((mw) / 2) - (logow / 2 * logosize) + SHADOW_OFF;
        rs[i].y =
            (rectangles[i].y * logosize) + yoff + ((mh) / 2) - (logoh / 2 * logosize) + SHADOW_OFF;
        rs[i].width  = rectangles[i].width * logosize;
        rs[i].height = rectangles[i].height * logosize;
    }
}

static void drawlogo(Display *dpy, struct lock *lock, int color)
{
    XRectangle rs[LENGTH(rectangles)];
    const int black = XBlackPixel(dpy, lock->screen);
    for (int i = 0; i < lock->nxsi; ++i) {
        resizerectangles(rs, lock->xsi[i].x_org, lock->xsi[i].y_org, lock->xsi[i].width,
                         lock->xsi[i].height);
        XSetForeground(dpy, lock->gc, black);
        XFillRectangles(dpy, lock->win, lock->gc, rs, LENGTH(rectangles));
        for (int j = 0; j < (int)LENGTH(rectangles); ++j) {
            rs[j].x -= SHADOW_OFF;
            rs[j].y -= SHADOW_OFF;
        }
        XSetForeground(dpy, lock->gc, lock->colors[color]);
        XFillRectangles(dpy, lock->win, lock->gc, rs, LENGTH(rectangles));
    }
    XSync(dpy, False);
}

static void readpw(Display *dpy, struct xrandr *rr, struct lock **locks, int nscreens,
                   const char *hash)
{
    XRRScreenChangeNotifyEvent *rre;
    char buf[32], passwd[256], *inputhash;
    int num, screen, running, failure, oldc;
    unsigned int len, color;
    KeySym ksym;
    XEvent ev;

    len     = 0;
    running = 1;
    failure = 0;
    oldc    = INIT;

    while (running && !XNextEvent(dpy, &ev)) {
        if (ev.type == KeyPress) {
            explicit_bzero(&buf, sizeof(buf));
            num = XLookupString(&ev.xkey, buf, sizeof(buf), &ksym, 0);
            if (IsKeypadKey(ksym)) {
                if (ksym == XK_KP_Enter)
                    ksym = XK_Return;
                else if (ksym >= XK_KP_0 && ksym <= XK_KP_9)
                    ksym = (ksym - XK_KP_0) + XK_0;
            }
            if (IsFunctionKey(ksym) || IsKeypadKey(ksym) || IsMiscFunctionKey(ksym) ||
                IsPFKey(ksym) || IsPrivateKeypadKey(ksym))
                continue;
            switch (ksym) {
            case XF86XK_AudioForward:
            case XF86XK_AudioRepeat:
            case XF86XK_AudioRandomPlay:
            case XF86XK_Standby:
            case XF86XK_AudioLowerVolume:
            case XF86XK_AudioMute:
            case XF86XK_AudioRaiseVolume:
            case XF86XK_AudioPlay:
            case XF86XK_AudioStop:
            case XF86XK_AudioPrev:
            case XF86XK_AudioNext:
                (XSendEvent(dpy, locks[0]->root, True, KeyPressMask, &ev) != 0 ||
                 XSendEvent(dpy, DefaultRootWindow(dpy), True, KeyPressMask, &ev));
                break;
            case XK_Return:
                passwd[len] = '\0';
                errno       = 0;
                if (!(inputhash = crypt(passwd, hash)))
                    fprintf(stderr, "slock: crypt: %s\n", strerror(errno));
                else
                    running = !!strcmp(inputhash, hash);
                if (running) {
                    XBell(dpy, 100);
                    failure = 1;
                }
                explicit_bzero(&passwd, sizeof(passwd));
                len = 0;
                break;
            case XK_Escape:
                explicit_bzero(&passwd, sizeof(passwd));
                len = 0;
                break;
            case XK_BackSpace:
                if (len) passwd[--len] = '\0';
                break;
            case XK_Control_L:
                explicit_bzero(&passwd, sizeof(passwd));
                len = 0;
                system("loginctl suspend -i");
                break;
            default:
                if (num && !iscntrl((int)buf[0]) && (len + num < sizeof(passwd))) {
                    memcpy(passwd + len, buf, num);
                    len += num;
                }
                break;
            }
            color = len ? INPUT : ((failure || failonclear) ? FAILED : INIT);
            if (running && oldc != (int)color) {
                for (screen = 0; screen < nscreens; screen++) {
                    if (locks[screen]->bgmap)
                        XSetWindowBackgroundPixmap(dpy, locks[screen]->win, locks[screen]->bgmap);
                    else
                        XSetWindowBackground(dpy, locks[screen]->win, locks[screen]->colors[0]);
                    // XClearWindow(dpy, locks[screen]->win);
                    drawlogo(dpy, locks[screen], color);
                }
                oldc = color;
            }
        } else if (rr->active && ev.type == rr->evbase + RRScreenChangeNotify) {
            rre = (XRRScreenChangeNotifyEvent *)&ev;
            for (screen = 0; screen < nscreens; screen++) {
                if (locks[screen]->win == rre->window) {
                    if (rre->rotation == RR_Rotate_90 || rre->rotation == RR_Rotate_270)
                        XResizeWindow(dpy, locks[screen]->win, rre->height, rre->width);
                    else
                        XResizeWindow(dpy, locks[screen]->win, rre->width, rre->height);
                    XClearWindow(dpy, locks[screen]->win);
                    break;
                }
            }
        } else {
            for (screen = 0; screen < nscreens; screen++)
                XRaiseWindow(dpy, locks[screen]->win);
        }
    }
}

static struct lock *lockscreen(Display *dpy, struct xrandr *rr, int screen)
{
    char curs[] = {0, 0, 0, 0, 0, 0, 0, 0};
    int i, ptgrab, kbgrab;
    struct lock *lock;
    XColor color, dummy;
    XSetWindowAttributes wa;
    Cursor invisible;

    if (dpy == NULL || screen < 0 || !(lock = (::lock *)malloc(sizeof(struct lock)))) return NULL;

    lock->screen = screen;
    lock->root   = RootWindow(dpy, lock->screen);

    if (!XShmQueryExtension(dpy)) die("slock: XSHM extension not supported\n");
    // const auto scr = XDefaultScreen(dpy);
    initimage(&lock->shim);
    const int imw = XDisplayWidth(dpy, 0);
    const int imh = XDisplayHeight(dpy, 0);
    if (!createimage(dpy, &lock->shim, imw, imh)) die("slock: createimage failed\n");
    lock->bgmap = XCreatePixmap(dpy, lock->root, imw, imh, DefaultDepth(dpy, screen));

    g_t("get bg",
        [&] { XShmGetImage(dpy, XDefaultRootWindow(dpy), lock->shim.ximage, 0, 0, AllPlanes); });
    g_t("img fx", [&] {
        const auto idata = lock->shim.data;
        const int hpert  = imh / nthr;
        if (hpert * nthr != imh || imw / pixelSize * pixelSize != imw) {
            fprintf(stderr, "slock: warning: pixelSize not factor of gcd(w, h/#t) (%d, %d/%u)\n",
                    imw, imh, nthr);
        }
        threads{[&](const int thr) {
            const int h0 = hpert * thr;
            const int h1 = h0 + hpert;
            for (int y = h0; y < h1; y += pixelSize) {
                const int h = pixelSize;
                for (int x = 0; x < imw; x += pixelSize) {
                    const int w = pixelSize;
                    int red = 0, green = 0, blue = 0;
                    for (int j = y; j < y + h; ++j) {
                        for (int i = x; i < x + w; ++i) {
                            const auto px = idata[j * imw + i];
                            red += (px >> 16) & 0xff;
                            green += (px >> 8) & 0xff;
                            blue += px & 0xff;
                        }
                    }

                    const int mc  = (red + green + blue) / 3;
                    red           = (red * colFact + mc * mcFact) / (colDsor * w * h);
                    green         = (green * colFact + mc * mcFact) / (colDsor * w * h);
                    blue          = (blue * colFact + mc * mcFact) / (colDsor * w * h);

                    const auto px = blue | (green << 8) | (red << 16) | 0xff000000;
                    for (int j = y; j < y + h; ++j) {
                        for (int i = x; i < x + w; ++i) {
                            idata[j * imw + i] = px;
                        }
                    }
                }
            }
        }};
    });

    for (i = 0; i < NUMCOLS; i++) {
        XAllocNamedColor(dpy, DefaultColormap(dpy, lock->screen), colorname[i], &color, &dummy);
        lock->colors[i] = color.pixel;
    }

    if (!(lock->xsi = XineramaQueryScreens(dpy, &lock->nxsi))) {
        fprintf(stderr, "slock: call to XineramaQueryScreens failed\n");
        return NULL;
    }
    lock->drawable = XCreatePixmap(dpy, lock->root, imw, imh, DefaultDepth(dpy, screen));
    lock->gc       = XCreateGC(dpy, lock->root, 0, NULL);
    XSetLineAttributes(dpy, lock->gc, 1, LineSolid, CapButt, JoinMiter);

    /* init */
    wa.override_redirect = 1;
    wa.background_pixel  = lock->colors[BACKGROUND];
    g_t("set bg", [&] {
        lock->win = XCreateWindow(
            dpy, lock->root, 0, 0, imw, imh, 0, DefaultDepth(dpy, lock->screen), CopyFromParent,
            DefaultVisual(dpy, lock->screen), CWOverrideRedirect | CWBackPixel, &wa);
        XShmPutImage(dpy, lock->bgmap, DefaultGC(dpy, lock->screen), lock->shim.ximage, 0, 0, 0, 0,
                     imw, imh, False);
        if (lock->bgmap) XSetWindowBackgroundPixmap(dpy, lock->win, lock->bgmap);
        lock->pmap = XCreateBitmapFromData(dpy, lock->win, curs, 8, 8);
    });
    invisible = XCreatePixmapCursor(dpy, lock->pmap, lock->pmap, &color, &color, 0, 0);
    XDefineCursor(dpy, lock->win, invisible);

    /* Try to grab mouse pointer *and* keyboard for 600ms, else fail the lock */
    for (i = 0, ptgrab = kbgrab = -1; i < 6; i++) {
        if (ptgrab != GrabSuccess) {
            ptgrab = XGrabPointer(dpy, lock->root, False,
                                  ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                                  GrabModeAsync, GrabModeAsync, None, invisible, CurrentTime);
        }
        if (kbgrab != GrabSuccess) {
            kbgrab =
                XGrabKeyboard(dpy, lock->root, True, GrabModeAsync, GrabModeAsync, CurrentTime);
        }

        /* input is grabbed: we can lock the screen */
        if (ptgrab == GrabSuccess && kbgrab == GrabSuccess) {
            XMapRaised(dpy, lock->win);
            if (rr->active) XRRSelectInput(dpy, lock->win, RRScreenChangeNotifyMask);

            XSelectInput(dpy, lock->root, SubstructureNotifyMask);
            drawlogo(dpy, lock, INIT);
            return lock;
        }

        /* retry on AlreadyGrabbed but fail on other errors */
        if ((ptgrab != AlreadyGrabbed && ptgrab != GrabSuccess) ||
            (kbgrab != AlreadyGrabbed && kbgrab != GrabSuccess))
            break;

        usleep(100000);
    }

    /* we couldn't grab all input: fail out */
    if (ptgrab != GrabSuccess)
        fprintf(stderr, "slock: unable to grab mouse pointer for screen %d\n", screen);
    if (kbgrab != GrabSuccess)
        fprintf(stderr, "slock: unable to grab keyboard for screen %d\n", screen);
    return NULL;
}

static void usage(void) { die("usage: slock [-v] [-t] [cmd [arg ...]]\n"); }

#define READ  0
#define WRITE 1

pid_t popen2(const char *command, int *infp, int *outfp)
{
    int p_stdin[2], p_stdout[2];
    pid_t pid;

    if (pipe(p_stdin) != 0 || pipe(p_stdout) != 0) return -1;

    pid = fork();

    if (pid < 0)
        return pid;
    else if (pid == 0) {
        close(p_stdin[WRITE]);
        dup2(p_stdin[READ], READ);
        close(p_stdout[READ]);
        dup2(p_stdout[WRITE], WRITE);

        execl("/bin/sh", "sh", "-c", command, NULL);
        perror("execl");
        exit(1);
    }

    if (infp == NULL)
        close(p_stdin[WRITE]);
    else
        *infp = p_stdin[WRITE];

    if (outfp == NULL)
        close(p_stdout[READ]);
    else
        *outfp = p_stdout[READ];

    return pid;
}

int main(int argc, char **argv)
{
    struct xrandr rr;
    struct lock **locks;
    struct passwd *pwd;
    struct group *grp;
    uid_t duid;
    gid_t dgid;
    const char *hash;
    Display *dpy;
    int s, nlocks, nscreens;

    ARGBEGIN
    {
    case 'v': fprintf(stderr, "slock-" STRINGIFY(VERSION) "\n"); return 0;
    case 't': g_t.enable(); break;
    default: usage();
    }
    ARGEND

    /* validate drop-user and -group */
    errno = 0;
    if (!(pwd = getpwnam(user)))
        die("slock: getpwnam %s: %s\n", user, errno ? strerror(errno) : "user entry not found");
    duid  = pwd->pw_uid;
    errno = 0;
    if (!(grp = getgrnam(group)))
        die("slock: getgrnam %s: %s\n", group, errno ? strerror(errno) : "group entry not found");
    dgid = grp->gr_gid;

    dontkillme();

    hash  = gethash();
    errno = 0;
    if (!crypt("", hash)) die("slock: crypt: %s\n", strerror(errno));

    if (!(dpy = XOpenDisplay(NULL))) die("slock: cannot open display\n");

    /* drop privileges */
    if (setgroups(0, NULL) < 0) die("slock: setgroups: %s\n", strerror(errno));
    if (setgid(dgid) < 0) die("slock: setgid: %s\n", strerror(errno));
    if (setuid(duid) < 0) die("slock: setuid: %s\n", strerror(errno));

    /* check for Xrandr support */
    rr.active = XRRQueryExtension(dpy, &rr.evbase, &rr.errbase);

    /* get number of screens in display "dpy" and blank them */
    nscreens  = ScreenCount(dpy);
    if (!(locks = (lock **)calloc(nscreens, sizeof(struct lock *)))) die("slock: out of memory\n");
    for (nlocks = 0, s = 0; s < nscreens; s++) {
        if ((locks[s] = lockscreen(dpy, &rr, s)) != NULL)
            nlocks++;
        else
            break;
    }
    XSync(dpy, 0);

    /* did we manage to lock everything? */
    if (nlocks != nscreens) return 1;

    /* run post-lock command */
    if (argc > 0) {
        switch (fork()) {
        case -1: die("slock: fork failed: %s\n", strerror(errno));
        case 0:
            if (close(ConnectionNumber(dpy)) < 0) die("slock: close: %s\n", strerror(errno));
            execvp(argv[0], argv);
            fprintf(stderr, "slock: execvp %s: %s\n", argv[0], strerror(errno));
            _exit(1);
        }
    }

    g_t.finish();

    /* everything is now blank. Wait for the correct password */
    readpw(dpy, &rr, locks, nscreens, hash);

    for (nlocks = 0, s = 0; s < nscreens; s++) {
        XFreePixmap(dpy, locks[s]->drawable);
        XFreeGC(dpy, locks[s]->gc);
    }

    XSync(dpy, 0);
    XCloseDisplay(dpy);
    return 0;
}
