#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/stat.h>

#include "build.h"
#include "sdlayer.h"
#include "cache1d.h"
#include "pragmas.h"
#include "a.h"
#include "osd.h"

int xres=-1, yres=-1, bpp=0, fullscreen=0, bytesperline, imageSize;
static unsigned char *frame;
intptr_t frameplace=0;
char modechange=1;
char offscreenrendering=0;
char videomodereset = 0;
extern int gammabrightness;
extern float curgamma;

int inputdevices=0;

char keystatus[256];
void (*keypresscallback)(int,int) = 0;

int mousex=0,mousey=0,mouseb=0;
static char mouseacquired=0,moustat=0;
char quitevent=0, appactive=1;

char joynumaxes=0, joynumbuttons=0;
int joyaxis[2], joyb=0;

static uint64_t timerfreq=0;
static uint32_t timerlastsample=0;
static uint32_t timerticspersec=0;
static void (*usertimercallback)(void) = NULL;

int   _buildargc = 1;
const char **_buildargv = NULL;

static int ds_bg_main = 0;
static int ds_bg_sub = 0;
static u16 d_8to16table[256];
static uint8_t* surface = 0;

uint64_t ds_time();

void nds_init() {
	int dswidth = 320;
	int dsheight = 200;

	defaultExceptionHandler();

	atexit(uninitsystem);

    soundMicOff();
    soundEnable();

	lcdMainOnTop();

	videoSetMode(MODE_3_2D | DISPLAY_BG3_ACTIVE);
	vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
	vramSetBankB(VRAM_B_MAIN_BG_0x06020000);
	vramSetBankD(VRAM_D_MAIN_BG_0x06040000);

	ds_bg_main = bgInit(3, BgType_Bmp8, BG_BMP8_256x256, 0, 0);
    //REG_BG3PA = ((dswidth / 256) << 8) | (dswidth % 256) ;
    //REG_BG3PB = 0;
    //REG_BG3PC = 0;
    //REG_BG3PD = ((dsheight / 192) << 8) | ((dsheight % 192) + (dsheight % 192) / 3) ;
    //REG_BG3X = 0;
    //REG_BG3Y = 0; 
	surface=(uint8*)(0x06000000);

	//bgSetPriority(1, 0);
	//bgSetPriority(3, 1);

	videoSetModeSub(MODE_3_2D);
	vramSetBankD(VRAM_C_SUB_BG_0x06200000);
	consoleInit(0, 2, BgType_Text4bpp, BgSize_T_256x256, 26, 3, false, true);
	ds_bg_sub = bgInitSub(3, BgType_Bmp8, BgSize_B8_256x256, 0, 0);

	bgSetPriority(2+4, 2);
	bgSetPriority(3+4, 1);

	bgUpdate();

	BG_PALETTE_SUB[0] = 0;
	BG_PALETTE_SUB[1] = RGB15(31,31,31);//by default font will be rendered with color 255
	BG_PALETTE_SUB[255] = RGB15(31,31,31);//by default font will be rendered with color 255

	/*int amulscale16(int x, int y);
	int x = mulscale16((8191L<<16),(8191L<<16));
	int y = amulscale16((8191L<<16),(8191L<<16));

	printf("%08x %08x\n", x, y); */


  if (!fatInitDefault())  
	{
		iprintf("Unable to initialize media device!");
        do {
            printf("=");
            swiWaitForVBlank();
        } while(1);
		return -1;
	}
	swiWaitForVBlank();
	swiWaitForVBlank();
}

int main(int argc, char *argv[]) {

	_buildargc = argc;
	_buildargv = (const char **)argv;

	nds_init();

	startwin_open();
	baselayer_init();

    int r = app_main(_buildargc, (char const * const*)_buildargv);

	startwin_close();

    return r;
}

static void shutdownvideo(void)
{
	if (frame) {
		free(frame);
		frame = NULL;
	}
}

void uninitsystem(void)
{
	uninitinput();
	uninitmouse();
	uninittimer();

	shutdownvideo();

	puts("uninitsystem\n");
    puts("\npress a button ...");
    do {
        swiWaitForVBlank();
    } while(keysCurrent() == 0);
    do {
        swiWaitForVBlank();
    } while(keysCurrent() != 0);
    puts("done\n");
}

int initsystem(void)
{

	return 0;
}

void initputs(const char *str)
{
	startwin_puts(str);
	startwin_idle(NULL);
	wm_idle(NULL);
}

static int sortmodes(const struct validmode_t *a, const struct validmode_t *b)
{
	int x;

	if ((x = a->fs   - b->fs)   != 0) return x;
	if ((x = a->bpp  - b->bpp)  != 0) return x;
	if ((x = a->xdim - b->xdim) != 0) return x;
	if ((x = a->ydim - b->ydim) != 0) return x;

	return 0;
}

static char modeschecked=0;
void getvalidmodes(void)
{
    if(modeschecked) {
        return;
    }

	static int defaultres[][2] = {
		{256,192},{0,0}
	};

	validmodecnt=0;

	buildputs("Detecting video modes:\n");

#define ADDMODE(x,y,c,f) if (validmodecnt<MAXVALIDMODES) { \
	int mn; \
	for(mn=0;mn<validmodecnt;mn++) \
		if (validmode[mn].xdim==x && validmode[mn].ydim==y && \
			validmode[mn].bpp==c  && validmode[mn].fs==f) break; \
	if (mn==validmodecnt) { \
		validmode[validmodecnt].xdim=x; \
		validmode[validmodecnt].ydim=y; \
		validmode[validmodecnt].bpp=c; \
		validmode[validmodecnt].fs=f; \
		validmodecnt++; \
		buildprintf("  - %dx%d %d-bit %s\n", x, y, c, (f&1)?"fullscreen":"windowed"); \
	} \
}

#define CHECKL(w,h) if ((w < maxx) && (h < maxy))
#define CHECKLE(w,h) if ((w <= maxx) && (h <= maxy))

	int maxx = 256;
	int maxy = 192;

	// Fullscreen 8-bit modes: upsamples to the desktop mode
	for (int i=0; defaultres[i][0]; i++) {
		CHECKLE(defaultres[i][0],defaultres[i][1])
			ADDMODE(defaultres[i][0],defaultres[i][1],8,1)
	}

#undef CHECK
#undef ADDMODE

	qsort((void*)validmode, validmodecnt, sizeof(struct validmode_t), (int(*)(const void*,const void*))sortmodes);

    modeschecked = 1;
}

//
// resetvideomode() -- resets the video system
//
void resetvideomode(void)
{
	modeschecked = 0;
}

int setvideomode(int x, int y, int c, int fs)
{
	int regrab = 0;
    int i, j;
    // Round up to a multiple of 4.

	if ((fs == fullscreen) && (x == xres) && (y == yres) && (c == bpp) &&
		!videomodereset) {
		OSD_ResizeDisplay(xres,yres);
		return 0;
	}

	if (checkvideomode(&x,&y,c,fs,0) < 0) return -1;	// Will return if GL mode not available.

	if (mouseacquired) {
		regrab = 1;
		grabmouse(0);
	}

	shutdownvideo();

	buildprintf("Setting video mode %dx%d (%d-bpp %s)\n", x,y,c,
		(fs & 1) ? "fullscreen" : "windowed");

	if(c != 8) {
		return -1;
	}
    int pitch = x;

    frame = (unsigned char *) malloc(pitch * y);
    if (!frame) {
        buildputs("Unable to allocate framebuffer\n");
        return -1;
    }

    frameplace = (intptr_t) frame;
    bytesperline = pitch;
    imageSize = bytesperline * y;
    numpages = 1;

    setvlinebpl(bytesperline);
    for (i = j = 0; i <= y; i++) {
        ylookup[i] = j;
        j += bytesperline;
    }

	xres = x;
	yres = y;
	bpp = c;
	fullscreen = fs;
	modechange = 1;
	videomodereset = 0;

	// setpalettefade will set the palette according to whether gamma worked
	setpalettefade(palfadergb.r, palfadergb.g, palfadergb.b, palfadedelta);

	if (regrab) grabmouse(1);

	startwin_close();

    return 0;
}

int checkvideomode(int *x, int *y, int c, int fs, int forced)
{
	int i, nearest=-1, dx, dy, odx=9999, ody=9999;

	getvalidmodes();

	if (c > 8) return -1;

	// fix up the passed resolution values to be multiples of 8
	// and at least 320x200 or at most MAXXDIMxMAXYDIM
	if (*x < 256) *x = 256;
	if (*y < 192) *y = 192;
	if (*x > MAXXDIM) *x = MAXXDIM;
	if (*y > MAXYDIM) *y = MAXYDIM;
	*x &= 0xfffffff8l;

	for (i=0; i<validmodecnt; i++) {
		if (validmode[i].bpp != c) continue;
		if (validmode[i].fs != fs) continue;
		dx = klabs(validmode[i].xdim - *x);
		dy = klabs(validmode[i].ydim - *y);
		if (!(dx | dy)) {   // perfect match
			nearest = i;
			break;
		}
		if ((dx <= odx) && (dy <= ody)) {
			nearest = i;
			odx = dx; ody = dy;
		}
	}

	if (nearest < 0) {
		// no mode that will match (eg. if no fullscreen modes)
		return -1;
	}

	*x = validmode[nearest].xdim;
	*y = validmode[nearest].ydim;

	return nearest;     // JBF 20031206: Returns the mode number
}

int setpalette(int UNUSED(start), int UNUSED(num), unsigned char * UNUSED(dapal)) {
	//printf("setpalette\n");
	for(int i=0;i<256;i++) {
		BG_PALETTE[i]=RGB15(curpalettefaded[i].r>>3,curpalettefaded[i].g>>3,curpalettefaded[i].b>>3)|BIT(15);
	}
    return 0;
}

int setgamma(float gamma)
{
	//printf("setgamma\n");
	return 0;
}

//
// begindrawing() -- locks the framebuffer for drawing
//
void begindrawing(void)
{
}


//
// enddrawing() -- unlocks the framebuffer
//
void enddrawing(void)
{
}

typedef struct {
	union {
		byte b[2];
		unsigned short s;
	};
} pix16_t;

void copy_buffer(u16 *dest16, byte *buffer) {

#if 1
	while(DMA_CR(0) & DMA_BUSY);
	dmaCopyWordsAsynch(0, buffer, dest16, 256 * 192 );
#else
	pix16_t p16;

	int ystep = (yres << 16) / SCREEN_HEIGHT;
	int xstep = (xres << 16) / SCREEN_WIDTH;
	int x, y, xx, yy;

	yy = (1L << 15);
	for (y = 0; y < SCREEN_HEIGHT; y++)
	{
		byte* src8 = buffer + ((yy >> 16) * xres);
		xx = (1L << 15);
		for (x = 0; x < SCREEN_WIDTH; x += 4)
		{
			p16.b[0] = src8[xx >> 16];
			xx += xstep;
			p16.b[1] = src8[xx >> 16];
			xx += xstep;
			*dest16++ = p16.s;

			p16.b[0] = src8[xx >> 16];
			xx += xstep;
			p16.b[1] = src8[xx >> 16];
			xx += xstep;
			*dest16++ = p16.s;
		}
		yy += ystep;
	}
#endif
}

//
// showframe() -- update the display
//
#ifdef __MEASURE__
static uint64_t last_frame_ticks = 0;
#endif

void showframe(void)
{
	if(frameplace == 0) {
		return;
	}
#ifdef __MEASURE__
	uint64_t frame_ticks = ds_time();
	printf("showframe %lld\n", frame_ticks - last_frame_ticks);
	last_frame_ticks = frame_ticks;
	//swiWaitForVBlank();
#endif
	copy_buffer(surface,frameplace);
}

void wm_setapptitle(const char *name)
{

}

void wm_setwindowtitle(const char *name)
{

}

int wm_msgbox(const char *name, const char *fmt, ...)
{
	char *buf = NULL;
	int rv;
	va_list va;

	va_start(va,fmt);
	rv = vasprintf(&buf,fmt,va);
	va_end(va);

	if (rv < 0) return -1;

	do {
		rv = 0;

#if 0
		if (wmgtk_msgbox(name, buf) >= 0) {
			rv = 1;
			break;
		}
		if (SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, name, buf, sdl_window) >= 0) {
			rv = 1;
			break;
		}
#endif

		puts(buf);
	} while(0);

	free(buf);

	return rv;
}

int wm_ynbox(const char *name, const char *fmt, ...)
{
	char *buf = NULL, ch;
	int rv;
	va_list va;

	//if (!name) {
	//	name = apptitle;
	//}

	va_start(va,fmt);
	rv = vasprintf(&buf,fmt,va);
	va_end(va);

	if (rv < 0) return -1;

#if 0
	SDL_MessageBoxButtonData buttons[2] = {
		{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Yes" },
		{ SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "No" },
	};
	SDL_MessageBoxData msgbox = {
		SDL_MESSAGEBOX_INFORMATION,
		sdl_window,
		name,
		buf,
		SDL_arraysize(buttons),
		buttons,
		NULL
	};
#endif

	do {
		rv = 0;

#if 0
		if ((rv = wmgtk_ynbox(name, buf)) >= 0) {
			break;
		}
		if (SDL_ShowMessageBox(&msgbox, &rv) >= 0) {
			rv = (rv == 1);
			break;
		}
#endif

		puts(buf);
		puts("   (assuming 'No')");
		rv = 0;
	} while(0);

	free(buf);

	return rv;
}

int wm_idle(void *ptr)
{
    (void)ptr;
    return 0;
}

int startwin_open(void) { return 0; }
int startwin_close(void) { return 0; }
int startwin_puts(const char *s) {
	puts(s);
	return 0; 
}
int startwin_idle(void *s) { return 0; }
int startwin_settitle(const char *s) { s=s; return 0; }

#define SetKey(key,state) { \
	keystatus[key] = state; \
}


static unsigned int ds_keys_down_last = 0;
static unsigned int ds_alt_down_last = 0;

typedef struct {
	unsigned int ds_key;
	unsigned short key;
} ds_key_t;

/*
* escape (0x1)
* enter = (0x1c)
 * 0      = Forward (0xc8)
 * 1      = Backward (0xd0)
 * 2      = Turn left (0xcb)
 * 3      = Turn right (0xcd)
 * 4      = Run (0x2a)
 * 5      = Strafe (0x9d)
 * 6      = Fire (0x1d)
 * 7      = Use (0x39)
 * 8      = Stand high (0x1e)
 * 9      = Stand low (0x2c)
 * 10     = Look up (0xd1)
 * 11     = Look down (0xc9)
 * 12     = Strafe left (0x33)
 * 13     = Strafe right (0x34)
 * 14     = 2D/3D switch (0x9c)
 * 15     = View cycle (0x1c)
 * 16     = 2D Zoom in (0xd)
 * 17     = 2D Zoom out (0xc)
 * 18     = Chat (0xf)
*/

static ds_key_t ds_keys[32] = {
	{ KEY_UP, 0xc8 },
	{ KEY_DOWN, 0xd0 },
	{ KEY_LEFT, 0xcb },
	{ KEY_RIGHT, 0xcd },
	{ KEY_L, 0x33 },
	{ KEY_R, 0x34 },
	{ KEY_X, 0 },
	{ KEY_B, 0x1e },
	{ KEY_Y, 0x39 },
	{ KEY_A, 0x1d },
	{ KEY_START, 0x1c },
	{ KEY_SELECT, 0x1 },
	{ KEY_L|KEY_R, 0x2a },
	{0, 0}
};

static ds_key_t ds_alt_keys[32] = {
	{ KEY_UP, 0xc8 },
	{ KEY_DOWN, 0xd0 },
	{ KEY_LEFT, 0x27 },
	{ KEY_RIGHT, 0x28 },
	{ KEY_L, 0xd1 },
	{ KEY_R, 0xc9 },
	{ KEY_X, 0 },
	{ KEY_B, 0x2c },
	{ KEY_Y, 0x39 },
	{ KEY_A, 0x1d },
	{ KEY_START, 0x1c },
	{ KEY_SELECT, 0xf },
	{ KEY_L|KEY_R, 0x2a },
	{0, 0}
};

void CheckDSKey(unsigned int keys_down,unsigned int last_down, unsigned int ds_key, unsigned short key)
{
	if ((keys_down & ds_key) == ds_key && (last_down & ds_key) == 0)
	{
		SetKey(key, 1);
	}
	else if ((keys_down & ds_key) == 0 && (last_down & ds_key) == ds_key)
	{
		SetKey(key, 0);
	}
}

int handleevents(void) {

	ds_key_t *ds_key;
	unsigned int dsKeys = keysCurrent();
	unsigned int ds_keys_down = dsKeys;
	unsigned int ds_alt_down = 0;

	//in alt keys
	if((dsKeys & KEY_X) ==  KEY_X) {
		ds_keys_down = 0;
		ds_alt_down = dsKeys;
	}

	for(ds_key = ds_alt_keys;ds_key->ds_key != 0; ds_key++) {
		if(ds_key->key) {
			CheckDSKey(ds_alt_down, ds_alt_down_last, ds_key->ds_key, ds_key->key);
		}
	}

	for(ds_key = ds_keys;ds_key->ds_key != 0; ds_key++) {
		if(ds_key->key) {
			CheckDSKey(ds_keys_down, ds_keys_down_last, ds_key->ds_key, ds_key->key);
		}
	}
	
	ds_keys_down_last = ds_keys_down;
	ds_alt_down_last = ds_alt_down;
	
	sampletimer();
	startwin_idle(NULL);
	wm_idle(NULL);
}

int initinput(void)
{
	memset(keystatus, 0, sizeof(keystatus));
	moustat=1;
	mouseacquired = 0;
	return 0;
}

void uninitinput(void)
{
	uninitmouse();

}

int initmouse(void)
{
	grabmouse(1);
	return 0;
}

void uninitmouse(void)
{
	grabmouse(0);
	moustat=0;
	moustat=1;
}

void grabmouse(int a) {
	if (appactive && moustat) {
		if (a != mouseacquired) {
			mouseacquired = a;
		}
	} else {
		mouseacquired = a;
	}
	mousex = mousey = 0;
}

void readmousexy(int *x, int *y)
{
	if (!mouseacquired || !appactive || !moustat) { *x = *y = 0; return; }
	*x = mousex;
	*y = mousey;
	mousex = mousey = 0;
}

void readmousebstatus(int *b)
{
	if (!mouseacquired || !appactive || !moustat) *b = 0;
	else *b = mouseb;
	// clear mousewheel events - the game has them now (in *b)
	// the other mousebuttons are cleared when there's a "button released"
	// event, but for the mousewheel that doesn't work, as it's released immediately
	mouseb &= ~(1<<4 | 1<<5);
}

void releaseallbuttons(void)
{
}

void setkeypresscallback(void (*callback)(int, int)) {
    keypresscallback = callback;
}

unsigned char bgetchar(void)
{
	unsigned char c = 0;
	return c;
}

int bkbhit(void)
{
	return 0;
}

void bflushchars(void)
{

}

uint64_t ds_time()
{
#if 1
	u32 time1 = TIMER1_DATA;
	u32 time2 = TIMER2_DATA;
	u32 time3 = TIMER3_DATA;
	uint64_t ds_tm = time3;
	ds_tm <<= 16;
	ds_tm += time2;
	ds_tm <<= 16;
	ds_tm += time1;
#else
	u32 time1 = TIMER1_DATA;
	u32 time2 = TIMER2_DATA;
	uint64_t ds_tm = time2;
	ds_tm <<= 16;
	ds_tm += time1;
#endif

	return ds_tm;
}

int inittimer(int tickspersecond)
{
	if (timerfreq) return 0;    // already installed

	buildputs("Initialising timer\n");


	TIMER_DATA(0) = 0x10000 - (0x1000000 / 11025) * 2;
	TIMER_CR(0) = TIMER_ENABLE | TIMER_DIV_1;
	TIMER_DATA(1) = 0;
	TIMER_CR(1) = TIMER_ENABLE | TIMER_CASCADE | TIMER_DIV_1;
	TIMER_DATA(2) = 0;
	TIMER_CR(2) = TIMER_ENABLE | TIMER_CASCADE | TIMER_DIV_1;

	timerfreq = 11025;
	timerticspersec = tickspersecond;
	timerlastsample = (uint32_t)(ds_time() * timerticspersec / timerfreq);

	usertimercallback = NULL;

	return 0;
}

void uninittimer(void)
{
	if (!timerfreq) return;

	timerfreq=0;
}

unsigned int getticks(void)
{
	//return (unsigned int)(ds_time()*1000/timerticspersec);
	return (unsigned int)((ds_time()*1000)/11025);
}

void sampletimer(void)
{
	int n;

	if (!timerfreq) return;

	n = (int)(ds_time() * timerticspersec / timerfreq) - timerlastsample;
	if (n>0) {
		totalclock += n;
		timerlastsample += n;
	}

	if (usertimercallback) {
        for (; n>0; n--) {
            usertimercallback();
        }
    }
}

void debugprintf(const char *f, ...)
{
#ifdef DEBUGGINGAIDS
	va_list va;

	va_start(va,f);
	Bvfprintf(stderr, f, va);
	va_end(va);
#endif
	(void)f;
}

const char *getjoyname(int what, int num) {
    return NULL;
}

const char *getkeyname(int num)
{
	if ((unsigned)num >= 256) return NULL;
	return NULL;//keynames[num];
}


int nds_mkdir(char *path, int unused) {
	int ret = 0;
    struct stat statBuf;
    char *_path = malloc(strlen(path)+1);
    strcpy(_path, path);

    char* str;
    char* cw = malloc(1024);

    getcwd(cw, 1024);
    printf("cwd %s\n", cw);
    printf("makepath %s\n", _path);

    str = strtok(_path, "/");

    while (str != NULL) {
        printf("part %s\n", str);
        if (stat(str, &statBuf) == -1) {
            printf("mkdir %s\n", str);
            if (mkdir(str, 0)) {
                printf("failed to create %s\n", str);
				ret = 1;
            }
        }
        chdir(str);
        str = strtok(NULL, "/");
    };

    chdir(cw);
    free(cw);
    free(_path);
    return ret;
}
