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

#include "cyg-profile.h"


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
int keyasciififoplc, keyasciififoend;
unsigned char keyasciififo[KEYFIFOSIZ];
static char *keyNames[256];


int mousex=0,mousey=0,mouseb=0;
static char mouseacquired=0,moustat=0;
char quitevent=0, appactive=1;

char joynumaxes=0, joynumbuttons=0;
int joyaxis[2], joyb=0;
void (*joypresscallback)(int,int) = 0;

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
static void keyboard_draw();
static void keyboard_input(uint32_t keys);
static void keyboard_init();

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
	BG_PALETTE_SUB[2] = RGB15(0,0,0);//by default font will be rendered with color 255
	BG_PALETTE_SUB[3] = RGB15(25,25,25);//by default font will be rendered with color 255
	BG_PALETTE_SUB[4] = RGB15(15,15,215);//by default font will be rendered with color 255

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

	keyboard_init();
#if ENABLE_CYGPROFILE
	cygprofile_begin();
#endif
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

#if ENABLE_CYGPROFILE
	cygprofile_end();
#endif

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
	return 1;
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
//#define __MEASURE__

#ifdef __MEASURE__
static uint64_t last_frame_ticks = 0;
static int fps[256];
static int current_frame = 0;
#endif

void showframe(void)
{
	if(frameplace == 0) {
		return;
	}
#ifdef __MEASURE__
	uint8_t *frm = (uint8_t *)frameplace;
	uint64_t frame_ticks = ds_time();
	//printf("showframe %lld\n", frame_ticks - last_frame_ticks);
	fps[current_frame] = (int )((frame_ticks - last_frame_ticks) >> 2);
	//printf("showframe %d %d\n", current_frame, fps);
	for(int i=0;i<256;i++) {
		int f = 191 - fps[i];
		if(f < 0) {
			f = 0;
		} else if(f > 191) {
			f = 191;
		}
		frm[i + (55 * 256)] = 256 - 8; 
		frm[i + (147 * 256)] = 256 - 8; 
		frm[i + (f * 256)] = ((i == current_frame) ? 251 : 252); 
	}
	current_frame++;
	current_frame &= 0xff;
	last_frame_ticks = frame_ticks;
	swiWaitForVBlank();
#endif
	copy_buffer(surface,frameplace);
	keyboard_draw();
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
	{ KEY_UP, 0x1a },
	{ KEY_DOWN, 0x1b },
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
		if (OSD_HandleChar(key)) {
			if (((keyasciififoend+1)&(KEYFIFOSIZ-1)) != keyasciififoplc) {
				keyasciififo[keyasciififoend] = key;
				keyasciififoend = ((keyasciififoend+1)&(KEYFIFOSIZ-1));
			}
		}
		SetKey(key, 1);
		if (keypresscallback) {
			keypresscallback(key, 1);
		}
	}
	else if ((keys_down & ds_key) == 0 && (last_down & ds_key) == ds_key)
	{
		SetKey(key, 0);
		if (keypresscallback) {
			keypresscallback(key, 0);
		}
	}
}

/* conflicts with nds types - so copied here */

typedef enum
   {
   joybutton_A,
   joybutton_B,
   joybutton_X,
   joybutton_Y,
   joybutton_Back,
   joybutton_Guide,
   joybutton_Start,
   joybutton_LeftStick,
   joybutton_RightStick,
   joybutton_LeftShoulder,
   joybutton_RightShoulder,
   joybutton_DpadUp,
   joybutton_DpadDown,
   joybutton_DpadLeft,
   joybutton_DpadRight
   } joybutton;

static uint32_t ds_button_map[12] = {
	joybutton_A,
	joybutton_B,
	joybutton_Back,
	joybutton_Start,
	joybutton_DpadRight,
	joybutton_DpadLeft,
	joybutton_DpadUp,
	joybutton_DpadDown,
	joybutton_RightShoulder,
	joybutton_LeftShoulder,
	joybutton_X,
	joybutton_Y
};

const char *getjoyname(int what, int num) {
	static const char * buttonnames[32] = {
	/* 00 */"A",
	/* 01 */"B",
	/* 02 */"X",
	/* 03 */"Y",
	/* 04 */"Select",
	/* 05 */"UNUSED",
	/* 06 */"Start",
	/* 07 */"L-Stick",
	/* 08 */"R-Stick",
	/* 09 */"L-Shoulder",
	/* 10 */"R-Shoulder",
	/* 11 */"DPad Up",
	/* 12 */"DPad Down",
	/* 13 */"DPad Left",
	/* 14 */"DPad Right",
	/* 15 */"Alt-A",
	/* 16 */"Alt-B",
	/* 17 */"Alt-X",
	/* 18 */"Alt-Y",
	/* 19 */"Alt-Select",
	/* 20 */"Alt-UNUSED",
	/* 21 */"Alt-Start",
	/* 22 */"Alt-L-Thumb",
	/* 23 */"Alt-R-Thumb",
	/* 24 */"Alt-L-Shoulder",
	/* 25 */"Alt-R-Shoulder",
	/* 26 */"Alt-DPad Up",
	/* 27 */"Alt-DPad Down",
	/* 28 */"Alt-DPad Left",
	/* 29 */"Alt-DPad Right"
	};
	switch(what) {
		case 1: // button
			if ((unsigned)num > (unsigned)32) return NULL;
			return buttonnames[num];

		default:
			return NULL;
	}
}

static void updatejoystick(unsigned int dsKeys) {
	joyb = 0;
	int alt = 0;
	if((dsKeys & KEY_X) != 0) {
		alt = 15;
	}
	for(int i=0;i<12;i++) {
		if((dsKeys & (1L<<i)) != 0) {
			joyb |= (1L << (ds_button_map[i] + alt));
		}
	}
	//printf("joyb: %08x\n", joyb);
}

int handleevents(void) {

	ds_key_t *ds_key;
	unsigned int dsKeys = keysCurrent();
	unsigned int ds_keys_down = dsKeys;
	unsigned int ds_alt_down = 0;

	//in alt keys
	/* if((dsKeys & KEY_X) ==  KEY_X) {
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
	} */
	
	ds_keys_down_last = ds_keys_down;
	ds_alt_down_last = ds_alt_down;


	updatejoystick(dsKeys);

	keyboard_input(dsKeys);
	
	sampletimer();
	startwin_idle(NULL);
	wm_idle(NULL);
}

int initinput(void)
{
	memset(keystatus, 0, sizeof(keystatus));
	moustat=1;
	mouseacquired = 0;

	inputdevices |= 4;
	joynumbuttons = 32;
	joynumaxes = 0;

	for(int i=0;i<256;i++) {
		keyNames[i] = KB_ScanCodeToString(i);
	}
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
	
	touchPosition touch;
	touchRead(&touch);
	
	mousex = touch.px;
	mousey = touch.py;
	
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
	int i;
	for (i=0;i<256;i++) {
		//if (!keystatus[i]) continue;
		//if (OSD_HandleKey(i, 0) != 0) {
			OSD_HandleKey(i, 0);
			SetKey(i, 0);
			if (keypresscallback) keypresscallback(i, 0);
		//}
	}

	mouseb = 0;

	if (joypresscallback) {
		for (i=0;i<32;i++)
			if (joyb & (1<<i)) joypresscallback(i+1, 0);
	}
	joyb = 0;
}

void setkeypresscallback(void (*callback)(int, int)) {
    keypresscallback = callback;
}

void setjoypresscallback(void (*callback)(int, int)) {
	joypresscallback = callback;
}

unsigned char bgetchar(void)
{
	unsigned char c;
	if (keyasciififoplc == keyasciififoend) return 0;
	c = keyasciififo[keyasciififoplc];
	keyasciififoplc = ((keyasciififoplc+1)&(KEYFIFOSIZ-1));
	return c;
}

int bkbhit(void)
{
	return (keyasciififoplc != keyasciififoend);
}

void bflushchars(void)
{
	keyasciififoplc = keyasciififoend = 0;
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

const char *getkeyname(int num)
{
	if ((unsigned)num >= 256) return NULL;
	return keyNames[num];
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

typedef struct {
	int x, y, length;
	int type;
	//char *text, *shift_text;
	uint8_t sc_code[20];
} sregion_t;

#define KEY_WIDTH(_n) (5*(_n)+6)

#define XSTR(_a) STR(__a)
#define STR(__a) #__a

#define SC_TICK 0x29
#define SC_1 0x02
#define SC_2 0x03
#define SC_3 0x04
#define SC_4 0x05
#define SC_5 0x06
#define SC_6 0x07
#define SC_7 0x08
#define SC_8 0x09
#define SC_9 0x0a
#define SC_0 0x0b
#define SC_MINUS 0xc
#define SC_EQUAL 0xd
#define SC_BKSP 0xe
#define SC_TAB 0xf
#define SC_Q 0x10
#define SC_W 0x11
#define SC_E 0x12
#define SC_R 0x13
#define SC_T 0x14
#define SC_Y 0x15
#define SC_U 0x16
#define SC_I 0x17
#define SC_O 0x18
#define SC_P 0x19
#define SC_LBRACKET 0x1a
#define SC_RBRACKET 0x1b
#define SC_BSLASH 0x2b
#define SC_CAPS 0x3a
#define SC_A 0x1e
#define SC_S 0x1f
#define SC_D 0x20
#define SC_F 0x21
#define SC_G 0x22
#define SC_H 0x23
#define SC_J 0x24
#define SC_K 0x25
#define SC_L 0x26
#define SC_SEMICOLON 0x27
#define SC_QUOTE 0x28
#define SC_ENTER 0x1c
#define SC_LSHIFT 0x2a
#define SC_RSHIFT 0x9d
#define SC_Z 0x2c
#define SC_X 0x2d
#define SC_C 0x2e
#define SC_V 0x2f
#define SC_B 0x30
#define SC_N 0x31
#define SC_M 0x32
#define SC_COMMA 0x33
#define SC_PERIOD 0x34
#define SC_FSLASH 0x35
#define SC_RSHIFT 0x36
#define SC_LCTRL 0x1d
#define SC_LALT 0x38
#define SC_SPACE 0x39
#define SC_RALT 0xb8
#define SC_RCTRL 0x9d
#define SC_LEFT 0xcb
#define SC_UP 0xc8
#define SC_DOWN 0xd0
#define SC_RIGHT 0xcd
#define SC_ESCAPE 0x1
#define SC_F1 0x3b
#define SC_F2 0x3c
#define SC_F3 0x3d
#define SC_F4 0x3e
#define SC_F5 0x3f
#define SC_F6 0x40
#define SC_F7 0x41
#define SC_F8 0x42
#define SC_F9 0x43
#define SC_F10 0x44
#define SC_F11 0x57
#define SC_F12 0x58

static char sctoasc[2][256] = {
	{
//      0    1    2    3    4    5    6    7    8    9    a    b    c    d    e    f
	0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 8,   9,   // 0x00
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 13,  0,   'a', 's', // 0x10
	'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'','`', 0,   '\\','z', 'x', 'c', 'v', // 0x20
	'b', 'n', 'm', ',', '.', '/', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   // 0x30
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   '-', 0,   0,   0,   '+', 0,   // 0x40
	0,   0,   0,   '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0x50
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0x60
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0x70
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0x80
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   13,  0,   0,   0,   // 0x90
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0xa0
	0,   0,   0,   0,   0,   '/', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0xb0
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0xc0
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0xd0
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0xe0
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0    // 0xf0
	},
	{
//      0    1    2    3    4    5    6    7    8    9    a    b    c    d    e    f
	0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 8,   9,   // 0x00
	'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 13,  0,   'A', 'S', // 0x10
	'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,   '|', 'Z', 'X', 'C', 'V', // 0x20
	'B', 'N', 'M', '<', '>', '?', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   // 0x30
	0,   0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1', // 0x40
	'2', '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0x50
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0x60
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0x70
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0x80
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   13,  0,   0,   0,   // 0x90
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0xa0
	0,   0,   0,   0,   0,   '/', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0xb0
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0xc0
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0xd0
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0xe0
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0    // 0xf0
	}
};


static sregion_t key_array[] = {
	{
		0, 0*16, 0,
		SC_ESCAPE, 
		//"`1234567890-=",
		//"~!@#$%^&*()_+",
		{'E', 's', 'c', 0 }
	},
	{
		KEY_WIDTH(3)+1, 0*16, 0,
		SC_F1, 
		//"`1234567890-=",
		//"~!@#$%^&*()_+",
		{'F', '1', 0 }
	},
	{
		KEY_WIDTH(3)+1 + 1*(KEY_WIDTH(2)+1), 0*16, 0,
		SC_F2, 
		//"`1234567890-=",
		//"~!@#$%^&*()_+",
		{'F', '2', 0 }
	},
	{
		KEY_WIDTH(3)+1 + 2*(KEY_WIDTH(2)+1), 0*16, 0,
		SC_F3, 
		//"`1234567890-=",
		//"~!@#$%^&*()_+",
		{'F', '3', 0 }
	},
	{
		KEY_WIDTH(3)+1 + 3*(KEY_WIDTH(2)+1), 0*16, 0,
		SC_F4, 
		//"`1234567890-=",
		//"~!@#$%^&*()_+",
		{'F', '4', 0 }
	},
	{
		KEY_WIDTH(3)+1 + 4*(KEY_WIDTH(2)+1), 0*16, 0,
		SC_F5, 
		//"`1234567890-=",
		//"~!@#$%^&*()_+",
		{'F', '5', 0 }
	},
	{
		KEY_WIDTH(3)+1 + 5*(KEY_WIDTH(2)+1), 0*16, 0,
		SC_F6, 
		//"`1234567890-=",
		//"~!@#$%^&*()_+",
		{'F', '6', 0 }
	},
	{
		KEY_WIDTH(3)+1 + 6*(KEY_WIDTH(2)+1), 0*16, 0,
		SC_F7, 
		//"`1234567890-=",
		//"~!@#$%^&*()_+",
		{'F', '7', 0 }
	},
	{
		KEY_WIDTH(3)+1 + 7*(KEY_WIDTH(2)+1), 0*16, 0,
		SC_F8, 
		//"`1234567890-=",
		//"~!@#$%^&*()_+",
		{'F', '8', 0 }
	},
	{
		KEY_WIDTH(3)+1 + 8*(KEY_WIDTH(2)+1), 0*16, 0,
		SC_F9, 
		//"`1234567890-=",
		//"~!@#$%^&*()_+",
		{'F', '9', 0 }
	},
	{
		KEY_WIDTH(3)+1 + 9*(KEY_WIDTH(2)+1), 0*16, 0,
		SC_F10, 
		//"`1234567890-=",
		//"~!@#$%^&*()_+",
		{'F', '1', '0', 0 }
	},
	{
		KEY_WIDTH(3)+1 + 9*(KEY_WIDTH(2)+1) + 1* (KEY_WIDTH(3)+1), 0*16, 0,
		SC_F11, 
		//"`1234567890-=",
		//"~!@#$%^&*()_+",
		{'F', '1', '1', 0 }
	},
	{
		KEY_WIDTH(3)+1 + 9*(KEY_WIDTH(2)+1) + 2* (KEY_WIDTH(3)+1), 0*16, 0,
		SC_F12, 
		//"`1234567890-=",
		//"~!@#$%^&*()_+",
		{'F', '1', '2', 0 }
	},
	{
		0, 1*16, 0,
		0, 
		//"`1234567890-=",
		//"~!@#$%^&*()_+",
		{SC_TICK, SC_1, SC_2, SC_3, SC_4, SC_5, SC_6, SC_7, SC_8, SC_9, SC_0, SC_MINUS, SC_EQUAL,0}
	},
	{
		13*16, 1*16, 0,
		SC_BKSP, 
		{ 'B', 'k', 's', 'p', 0 }
	},
	{
		0, 2*16, 0,
		SC_TAB, 
		{ 'T', 'a', 'b', 0 }
	},
	{
		KEY_WIDTH(3)+1, 2*16, 0,
		0, 
		{SC_Q, SC_W, SC_E, SC_R, SC_T, SC_Y, SC_U, SC_I, SC_O, SC_P, SC_LBRACKET, SC_RBRACKET, SC_BSLASH,0}
	},
	{
		0, 3*16,  0,
		SC_CAPS, 
		{ 'C', 'A', 'P', 'S', 0 }
	},
	{
		KEY_WIDTH(4)+1, 3*16, 0,
		0, 
		{SC_A, SC_S, SC_D, SC_F, SC_G, SC_H, SC_J, SC_K, SC_L, SC_SEMICOLON, SC_QUOTE,0}
	},
	{
		KEY_WIDTH(4)+1 + (11*16), 3*16,  0,
		SC_ENTER, 
		{ 'E', 'n', 't', 'e', 'r', 0 }
	},
	{
		0, 4*16,  0,
		SC_LSHIFT, 
		{ 'S', 'h', 'i', 'f', 't', 0 }
	},
	{
		256 - 2*16, 4*16, 0,
		0, 
		{SC_UP}
	},
	{
		KEY_WIDTH(5) + 1, 4*16, 0,
		0, 
		{SC_Z, SC_X, SC_C, SC_V, SC_B, SC_N, SC_M, SC_COMMA, SC_PERIOD, SC_FSLASH,0}
	},
	{
		KEY_WIDTH(5) + 1 + 10*16, 4*16,  0,
		SC_RSHIFT, 
		{ 'S', 'h', 'i', 'f', 't', 0 }
	},
	{
		0, 5*16, 0,
		SC_LCTRL, 
		{ 'C', 't', 'r', 'l', 0 }
	},
	{
		KEY_WIDTH(4) + 1, 5*16, 0,
		SC_LALT, 
		{ 'A', 'l', 't', 0 }
	},
	{
		KEY_WIDTH(4) + 1 + KEY_WIDTH(3) + 1, 5*16, 0,
		SC_SPACE, 
		{ ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'S', 'P', 'A', 'C', 'E', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 0 }
	},
	{
		KEY_WIDTH(4) + 1 + KEY_WIDTH(3) + 1 + KEY_WIDTH(19) + 1, 5*16, 0,
		SC_RALT, 
		{ 'A', 'l', 't', 0 }
	},
	{
		KEY_WIDTH(4) + 1 + KEY_WIDTH(3) + 1 + KEY_WIDTH(19) + 1 + KEY_WIDTH(3) + 1, 5*16, 0,
		SC_RCTRL, 
		{ 'C', 't', 'r', 'l',  0 }
	},
	{
		256 - 3*16, 5*16, 0,
		0, 
		{SC_LEFT, SC_DOWN, SC_RIGHT,0}
	}
};

static sregion_t *key_caps = 0;
static sregion_t *key_shift = 0;

uint8_t key_arrows[4][8] = {
	{ 0x00, 0x10, 0x38, 0x7C, 0x10, 0x10, 0x10, 0x00 }, //up
	{ 0x00, 0x10, 0x10, 0x10, 0x7C, 0x38, 0x10, 0x00 }, //down
	{ 0x00, 0x00, 0x10, 0x30, 0x7E, 0x30, 0x10, 0x00 }, //right
	{ 0x00, 0x00, 0x08, 0x0C, 0x7E, 0x0C, 0x08, 0x00 } //left
};

//#include "font8x8_basic.h"
#include "spleen5x8.h"

static int key_array_count = sizeof(key_array)/sizeof(sregion_t);
static uint8_t *bottom_screen = 0;
#define KEY_COLOR 3
#define KEY_COLOR_PRESS 4
#define BACKGROUND_COLOR 2

static sregion_t *key_touching = 0;
static int key_touching_position = 0;

static sregion_t *key_touching_last = 0;
static int key_touching_position_last = 0;

static int keyboard_dirty = 1;
static int keyboard_caps = 0;
static int keyboard_shift = 0;

static void keyboard_refresh();

static int region_to_key(sregion_t *region, int position) {
	if(region == 0) {
		return 0;
	}
	if(region->type == 0) {
		//printf("key up: %d %d %c %s\n", position, region->sc_code[position], region->text[position], region->text);
		return region->sc_code[position];
	}
	return region->type;
}

static int sc_to_ascii(int c) {
	int ch = sctoasc[keyboard_shift][c&0xff];
	if(keyboard_caps) {
		ch = toupper(ch);
	}
	return ch;
}

static void key_draw_pressed(sregion_t *touching, int touching_position, uint8_t c);

static void keyboard_input(uint32_t keys) {
	touchPosition	touch  = { 0,0,0,0 };
	int x, y, i, len;
	int shift = 0;
	sregion_t *touching = 0;
	int touching_position = 0;

	if((keys & KEY_TOUCH) != 0) {
		touchRead(&touch);
		x = touch.px;
		y = touch.py;

		//printf("touching: %d %d\n",x,y);

		//only look for first touch
		if(key_touching == 0) {
			for(i=0;i<key_array_count;i++) {
				sregion_t *region = &key_array[i];
				if(y < region->y || y >= (region->y+16))
				{
					continue;
				}
				if(x < region->x) {
					continue;
				}
				//char *text = shift ? region->shift_text : region->text;
				//len = strlen(text);
				len = region->length;
				int width = region->type == 0 ? (len*16) : KEY_WIDTH(len);
				if(x > (region->x + width)) {
					//printf("skip: %s %d %d\n", region->x, width, text);
					continue;
				}
				if(region->type == 0) {
					touching_position = (x - region->x)/16;
				} else {
					touching_position = 0;
				}
				touching = region;
				//printf("region: %d %d %d %s\n", region->x, region->y, len, region->text);
				break;
			}
		} else {
			touching = key_touching;
			touching_position = key_touching_position;
		}
	}
	//saved the last value
	key_touching_last = key_touching;
	key_touching_position_last = key_touching_position;
	//save currently touching
	key_touching = touching;
	key_touching_position = touching_position;
	if(key_touching != key_touching_last || key_touching_position != key_touching_position_last) {
		if(key_touching_last) {
			//send a key uo event for the last key
			int key =region_to_key(key_touching_last, key_touching_position_last);
			//printf("key up: %d %02x\n", key, key);
			SetKey(key, 0);
			if (keypresscallback) {
				keypresscallback(key, 0);
			}
			switch (key) {
			case SC_CAPS:
			case SC_LSHIFT:
				break;
			default:
				if(keyboard_shift) {
					keyboard_shift = 0;
					keyboard_refresh();
				}
				key_draw_pressed(key_touching_last, key_touching_position_last, KEY_COLOR);
				break;
			}
		}
		if(key_touching) {
			int key =region_to_key(key_touching, key_touching_position);
			//printf("key down: %d %02x %d\n", key, key, key_touching_position);
			//send a key down event for the current key
			if (OSD_HandleChar(key)) {
				if (((keyasciififoend+1)&(KEYFIFOSIZ-1)) != keyasciififoplc) {
					keyasciififo[keyasciififoend] = sctoasc[0][key&0xff];
					keyasciififoend = ((keyasciififoend+1)&(KEYFIFOSIZ-1));
				}
			}
			SetKey(key, 1);
			if (keypresscallback) {
				keypresscallback(key, 1);
			}
			switch (key) {
			case SC_LSHIFT:
				keyboard_shift = keyboard_shift == 0 ? 1 : 0;
				keyboard_refresh();
				break;
			case SC_CAPS:
				keyboard_caps = keyboard_caps == 0 ? 1 : 0;
				keyboard_refresh();
				break;
			default:
				key_draw_pressed(key_touching, key_touching_position, KEY_COLOR_PRESS);
			}
		}
	}
}

/*static void bitmap_draw(uint8_t *bitmap, uint8_t c, uint8_t *line) {
	for (int xx = 0; xx < 8; xx++) {
		for (int yy = 0; yy < 8; yy++) {
			int set = bitmap[xx] & 1 << yy;
			line[yy] = set ? BACKGROUND_COLOR : c;
		}
		line += 256;
	}
}*/

static void bitmap_draw(uint8_t *bitmap, uint8_t c, uint8_t *line) {
	for (int yy = 0; yy < 8; yy++) {
		for (int xx = 0; xx < 5; xx++) {
			int set = bitmap[yy] & (0x80 >> xx);
			line[xx] = set ? BACKGROUND_COLOR : c;
		}
		line += 256;
	}
}

static draw_arrow(int num, uint8_t c, uint8_t *line) {
	uint8_t *bitmap = key_arrows[num];
	for (int yy = 0; yy < 8; yy++) {
		for (int xx = 0; xx < 8; xx++) {
			int set = bitmap[yy] & (0x80 >> xx);
			line[xx] = set ? BACKGROUND_COLOR : c;
		}
		line += 256;
	}
}

static void key_draw(int x, int y, char *text, int width, uint8_t c) {
	uint8_t *buf = &bottom_screen[x];
	uint8_t *line = buf + (256 * y);
	int ch;
	uint8_t *bitmap;

	memset(line + 1, c, width - 2);
	line += 256;
	for(int j=0;j<13;j++) {
		memset(line, c, width - 0);
		line += 256;
	}
	memset(line + 1, c, width - 2);
	line += 256;

	while(*text) {	
		line = buf + (256 * (y + 5)) + 2;
		switch(*text) {
			case SC_UP:
				draw_arrow(0, c, line);
				break;
			case SC_DOWN:
				draw_arrow(1, c, line);
				break;
			case SC_LEFT:
				draw_arrow(2, c, line);
				break;
			case SC_RIGHT:
				draw_arrow(3, c, line);
				break;
			default:
				if(width == 15) {
					ch = sc_to_ascii(*text);
				} else {
					ch = *text;
				}
				//bitmap = font8x8_basic[ch & 127];
				bitmap = &spleen5x8_data[(ch - 32) * 8];
				bitmap_draw(bitmap, c, line+1);
				break;
		}

		if(width == 15) {
			break;
		}
		text++;
		buf += 5;
	}
}

static void key_draw_pressed(sregion_t *touching, int touching_position, uint8_t c) {
	int x = touching->x;
	int y = touching->y;
	uint8_t *text = touching->sc_code;
	int len = touching->length;
	int width = 15;
	uint8_t _text[2] = {0,0};
	if(touching->type == 0) {
		text += touching_position;
		_text[0] = *text;
		text = _text;
		x += (touching_position * 16);
	} else {
		width = KEY_WIDTH(len);
	}

	uint8_t *buf = &bottom_screen[x];
	uint8_t *line = buf + (256 * y);
	int ch;
	uint8_t *bitmap;
	
	memset(line + 1, c, width - 2);
	line += 256;
	for(int j=0;j<13;j++) {
		memset(line, c, width - 0);
		line += 256;
	}
	memset(line + 1, c, width - 2);
	line += 256;

	while(*text) {	
		line = buf + (256 * (y + 5)) + 2;
		switch(*text) {
			case SC_UP:
				draw_arrow(0, c, line);
				break;
			case SC_DOWN:
				draw_arrow(1, c, line);
				break;
			case SC_LEFT:
				draw_arrow(2, c, line);
				break;
			case SC_RIGHT:
				draw_arrow(3, c, line);
				break;
			default:
				if(width == 15) {
					ch = sc_to_ascii(*text);
				} else {
					ch = *text;
				}
				//bitmap = font8x8_basic[ch & 127];
				bitmap = &spleen5x8_data[(ch - 32) * 8];
				bitmap_draw(bitmap, c, line);
				break;
		}

		if(width == 15) {
			break;
		}
		text++;
		buf += 5;
	}
	keyboard_dirty = 1;
}

static void region_draw(sregion_t *region, int shift) {
	int x = region->x;
	int y = region->y;
	uint8_t *sc_code = region->sc_code;
	int len = region->length;
	
	if(y < 0 || (y+16) >= 192) {
		return;
	}

	switch(region->type) {
	case 0:
		for(int i = 0;i < len;i++) {
			key_draw(x, y, &sc_code[i],15,KEY_COLOR);
			x += 16;
		}
		break;
	default:
		key_draw(x, y, sc_code, KEY_WIDTH(len), KEY_COLOR);
		break;
	}
}

static void keyboard_refresh() {
	for(int i=0;i < key_array_count; i++) {
		region_draw(&key_array[i], 0);
	}
	key_draw_pressed(key_shift, 0, keyboard_shift == 0 ? KEY_COLOR: KEY_COLOR_PRESS);
	key_draw_pressed(key_caps, 0, keyboard_caps == 0 ? KEY_COLOR: KEY_COLOR_PRESS);
	keyboard_dirty = 1;
}

static void keyboard_draw() {

	uint16_t *dest16 = (uint16_t*)bgGetGfxPtr(ds_bg_sub);

	if(bottom_screen == 0) {
		bottom_screen = malloc(256 * 192);
		if(bottom_screen == 0) {
			printf("bottom_screen == 0");
			exit(-1);
		}
		memset(bottom_screen,BACKGROUND_COLOR,256 * 192/2);
		keyboard_refresh();
	}

	if(keyboard_dirty) {
		while(DMA_CR(1) & DMA_BUSY);
		dmaCopyWordsAsynch(1, bottom_screen, dest16, 256 * 192 );
		keyboard_dirty = 0;
	}

}

static void keyboard_init() {
	for(int i=0;i < key_array_count; i++) {
		key_array[i].length = strlen(key_array[i].sc_code);
		switch(key_array[i].type) {
		case SC_LSHIFT:
			key_shift = &key_array[i];
			break;
		case SC_CAPS:
			key_caps = &key_array[i];
			break;
		}
	}
}