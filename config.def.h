/* user and group to drop privileges to */
static const char *user               = "nobody";
static const char *group              = "nobody";

static const char *colorname[NUMCOLS] = {
    /* [BACKGROUND] = */ "white",
    /* [INIT] = */ "2d2d2d",    /* after initialization */
    /* [INPUT] = */ "#005577",  /* during input */
    /* [FAILED] = */ "#CC3333", /* wrong password */
};

/* treat a cleared input like a wrong password (color) */
static const int failonclear    = 0;

static const int logosize       = 75;
static const int logow          = 12; /* Grid width and height for right center alignment*/
static const int logoh          = 6;

static XRectangle rectangles[9] = {
    {0, 3, 1, 3}, {1, 3, 2, 1}, {0, 5, 8, 1}, {3, 0, 1, 5},  {5, 3, 1, 2},
    {7, 3, 1, 2}, {8, 3, 4, 1}, {9, 4, 1, 2}, {11, 4, 1, 2},
};

/* set pixelation radius */
constexpr inline int pixelSize = 40;
constexpr inline int colFact   = 6;
constexpr inline int mcFact    = 2;
constexpr inline int colDsor   = 16;
