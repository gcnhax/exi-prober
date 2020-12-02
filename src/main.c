#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <ogc/exi.h>

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

#define ESC_BG_HAPPY "\x1b[42;1m\x1b[37;1m"
#define ESC_FG_HAPPY "\x1b[32;1m"
#define ESC_BG_SAD "\x1b[41;1m\x1b[37;1m"
#define ESC_RST "\x1b[39m\x1b[40m"

#define WRITESIZE

static u8 dma_buf[64] __attribute__((aligned(32)));

//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------

	// Initialise the video system
	VIDEO_Init();

	// This function initialises the attached controllers
	WPAD_Init();

	// Obtain the preferred video mode from the system
	// This will correspond to the settings in the Wii menu
	rmode = VIDEO_GetPreferredMode(NULL);

	// Allocate memory for the display in the uncached region
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

	// Initialise the console, required for printf
	console_init(xfb,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);

	// Set up the video registers with the chosen mode
	VIDEO_Configure(rmode);

	// Tell the video hardware where our display memory is
	VIDEO_SetNextFramebuffer(xfb);

	// Make the display visible
	VIDEO_SetBlack(FALSE);

	// Flush the video register changes to the hardware
	VIDEO_Flush();

	// Wait for Video setup to complete
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();


	// The console understands VT terminal escape codes
	// This positions the cursor on row 2, column 0
	// we can use variables for this with format codes too
	// e.g. printf ("\x1b[%d;%dH", row, column );
	printf("\x1b[2;0H");


	printf("exi-prober!\n");

#ifdef PROBE_CHANNELS
	for (size_t chan = 0; chan < 3; chan++) {
		printf("scanning channel %d:", chan);
		printf("  probing channel... ");
		while (EXI_ProbeEx(chan) == 0) /* spin */;
		printf(ESC_BG_HAPPY"[ok]"ESC_RST"\n");

		for (size_t dev = 0; dev < 3; dev++) {
			printf("  querying id of device %d\n", dev);

			uint32_t id;
			int errno;
			if ((errno = EXI_GetID(chan, dev, &id)) == 1) {
				printf("    "ESC_FG_HAPPY"-> %#x"ESC_RST"\n", id);
			} else {
				printf("    "ESC_BG_SAD"!! errno: %d"ESC_RST"\n", errno);
			}
		}
	}
#endif

#ifdef GET_IPL_ADDRS
	printf("grabbing addrs from IPL\n");
	while (EXI_ProbeEx(0) == 0) ;
	for (int i = 0; i<16; i++) {
		EXI_Lock(/*chan*/ 0, /*dev*/ 1, /*unlock cb*/ NULL);
		EXI_Select(0, 1, EXI_SPEED1MHZ);
		u32 cmd = i << 6;
		EXI_Imm(0, &cmd, 4, EXI_WRITE, NULL);
		EXI_Sync(0);
		u32 data;
		EXI_Imm(0, &data, 4, EXI_READ, NULL);
		EXI_Sync(0);
		EXI_Deselect(0);
		EXI_Unlock(0);

		printf("%04x | %#x\n", i, data);
	}
#endif

#ifdef WRITESIZE
	printf("test: write 0, but with different sizes.\n");

	printf("4 byte write, 4 byte read returns: ");
	while (EXI_ProbeEx(0) == 0) ;

	{
		EXI_Lock(/*chan*/ 0, /*dev*/ 1, /*unlock cb*/ NULL);
		EXI_Select(0, 1, EXI_SPEED1MHZ);
		u32 cmd = 0;
		EXI_Imm(0, &cmd, 4, EXI_WRITE, NULL);
		EXI_Sync(0);
		u32 data;
		EXI_Imm(0, &data, 4, EXI_READ, NULL);
		EXI_Sync(0);
		EXI_Deselect(0);
		EXI_Unlock(0);

		printf("%#x\n", data);
	}

	printf("2 byte write, 4 byte read returns: ");
	{
		EXI_Lock(/*chan*/ 0, /*dev*/ 1, /*unlock cb*/ NULL);
		EXI_Select(0, 1, EXI_SPEED1MHZ);
		u32 cmd = 0;
		EXI_Imm(0, &cmd, 2, EXI_WRITE, NULL);
		EXI_Sync(0);
		u32 data;
		EXI_Imm(0, &data, 4, EXI_READ, NULL);
		EXI_Sync(0);
		EXI_Deselect(0);
		EXI_Unlock(0);

		printf("%#x\n", data);
	}

#endif

#ifdef IPL_DMA
	printf("test: dma to IPL region\n");
	u32 offset = 0;
	
	while (EXI_ProbeEx(0) == 0) ;

	EXI_Lock(/*chan*/ 0, /*dev*/ 1, /*unlock cb*/ NULL);
	EXI_Select(0, 1, EXI_SPEED8MHZ);
	u32 cmd = (offset << 6);
	EXI_Imm(0, &cmd, 4, EXI_WRITE, NULL);
	EXI_Sync(0);
	EXI_Dma(0, &dma_buf, 64, EXI_READ, NULL);
	EXI_Sync(0);
	EXI_Deselect(0);
	EXI_Unlock(0);

	for (int i = 0; i<16; i++) {
		printf("%04x | %#x\n", i, dma_buf[i]);
	}

#endif

	printf("\n\ndone! :3c\n");


	while(1) {

		// Call WPAD_ScanPads each loop, this reads the latest controller states
		WPAD_ScanPads();

		// WPAD_ButtonsDown tells us which buttons were pressed in this loop
		// this is a "one shot" state which will not fire again until the button has been released
		u32 pressed = WPAD_ButtonsDown(0);

		// We return to the launcher application via exit
		if ( pressed & WPAD_BUTTON_HOME ) exit(0);

		// Wait for the next frame
		VIDEO_WaitVSync();
	}

	return 0;
}
