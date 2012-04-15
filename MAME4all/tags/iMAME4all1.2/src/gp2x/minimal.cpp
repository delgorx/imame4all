/*

  GP2X minimal library v0.A by rlyeh, (c) 2005. emulnation.info@rlyeh (swap it!)

  Thanks to Squidge, Robster, snaff, Reesy and NK, for the help & previous work! :-)

  License
  =======

  Free for non-commercial projects (it would be nice receiving a mail from you).
  Other cases, ask me first.

  GamePark Holdings is not allowed to use this library and/or use parts from it.

*/

#include "minimal.h"

unsigned long 			gp2x_dev[3];
static volatile unsigned long 	*gp2x_memregl;
volatile unsigned short 	*gp2x_memregs;
static unsigned long 		gp2x_ticks_per_second;
unsigned char 			*gp2x_screen8;
unsigned short 			*gp2x_screen15;
unsigned short 			*gp2x_logvram15[2];
unsigned long 			gp2x_physvram[2];
unsigned int			gp2x_nflip;
volatile unsigned short 	gp2x_palette[512];
int				gp2x_sound_rate=22050;
int				gp2x_sound_stereo=0;
int 				gp2x_pal_50hz=0;
int				gp2x_ram_tweaks=0;
int				gp2x_clock=200;
int 				rotate_controls=0;

unsigned char  			*gp2x_dualcore_ram;
unsigned long  			 gp2x_dualcore_ram_size;

#define MAX_SAMPLE_RATE (44100*2)

void gp2x_video_flip(void)
{
#ifdef MMUHACK
	//flushcache(gp2x_screen8,gp2x_screen8+(640*480*2),0);
#endif
  	unsigned long address=gp2x_physvram[gp2x_nflip];
	gp2x_nflip=gp2x_nflip?0:1;
  	gp2x_screen15=gp2x_logvram15[gp2x_nflip]; 
  	gp2x_screen8=(unsigned char *)gp2x_screen15; 
   	gp2x_memregs[0x290E>>1]=(unsigned short)(address & 0xFFFF);
  	gp2x_memregs[0x2910>>1]=(unsigned short)(address >> 16);
  	gp2x_memregs[0x2912>>1]=(unsigned short)(address & 0xFFFF);
  	gp2x_memregs[0x2914>>1]=(unsigned short)(address >> 16);
}


void gp2x_video_flip_single(void)
{
#ifdef MMUHACK
	//flushcache(gp2x_screen8,gp2x_screen8+(640*480*2),0);
#endif
  	unsigned long address=gp2x_physvram[0];
  	gp2x_nflip=0;
  	gp2x_screen15=gp2x_logvram15[gp2x_nflip]; 
  	gp2x_screen8=(unsigned char *)gp2x_screen15; 
  	gp2x_memregs[0x290E>>1]=(unsigned short)(address & 0xFFFF);
  	gp2x_memregs[0x2910>>1]=(unsigned short)(address >> 16);
  	gp2x_memregs[0x2912>>1]=(unsigned short)(address & 0xFFFF);
  	gp2x_memregs[0x2914>>1]=(unsigned short)(address >> 16);
}


void gp2x_video_setpalette(void)
{
  	unsigned short *g=(unsigned short *)gp2x_palette;
	unsigned short *memreg=(short unsigned int *)&gp2x_memregs[0x295A>>1];
  	int i=512;
  	gp2x_memregs[0x2958>>1]=0;                                                     
  	do { *memreg=*g++; } while(--i);
}

/*
int master_volume;
*/

unsigned long gp2x_joystick_read(int n)
{
	extern int master_volume; 
  	unsigned long res=0;
	if (n==0)
	{
	  	unsigned long value=(gp2x_memregs[0x1198>>1] & 0x00FF);
	  	if(value==0xFD) value=0xFA;
	  	if(value==0xF7) value=0xEB;
	  	if(value==0xDF) value=0xAF;
	  	if(value==0x7F) value=0xBE;
	  	res=~((gp2x_memregs[0x1184>>1] & 0xFF00) | value | (gp2x_memregs[0x1186>>1] << 16));
	
	  	/* GP2X F200 Push Button */
	  	if ((res & GP2X_UP) && (res & GP2X_DOWN) && (res & GP2X_LEFT) && (res & GP2X_RIGHT))
	  		res |= GP2X_PUSH;
	}
	
	if (num_of_joys>n)
	{
	  	/* Check USB Joypad */
		res |= gp2x_usbjoy_check(joys[n]);
	}

  	if (!rotate_controls) {
  		if ( (res & GP2X_VOL_UP) &&  (res & GP2X_VOL_DOWN)) gp2x_sound_volume(100,100);
  		if ( (res & GP2X_VOL_UP) && !(res & GP2X_VOL_DOWN)) gp2x_sound_volume(master_volume+1,master_volume+1);
  		if (!(res & GP2X_VOL_UP) &&  (res & GP2X_VOL_DOWN)) gp2x_sound_volume(master_volume-1,master_volume-1);
  	}
  	else
  	{
  		if ( (res & GP2X_VOL_UP) &&  (res & GP2X_VOL_DOWN) && (res & GP2X_START)) gp2x_sound_volume(100,100);
  		if ( (res & GP2X_VOL_UP) && !(res & GP2X_VOL_DOWN) && (res & GP2X_START)) gp2x_sound_volume(master_volume+1,master_volume+1);
  		if (!(res & GP2X_VOL_UP) &&  (res & GP2X_VOL_DOWN) && (res & GP2X_START)) gp2x_sound_volume(master_volume-1,master_volume-1);

  		if (res & GP2X_VOL_DOWN)
  		{
  			res|=GP2X_B;
  		}
  		if (res & GP2X_VOL_UP)
  		{
  			res|=GP2X_X;
  		}
  	}
  	
	return res;
}

unsigned long gp2x_joystick_press (int n)
{
	unsigned long ExKey=0;
	while(gp2x_joystick_read(n)&0x8c0ff55) { gp2x_timer_delay(150); }
	while(!(ExKey=gp2x_joystick_read(n)&0x8c0ff55)) { }
	return ExKey;
}

void gp2x_sound_volume(int l, int r)
{
	extern int master_volume; 
 	l=l<0?0:l; l=l>319?319:l; r=r<0?0:r; r=r>319?319:r;
 	if (l>0)
 		master_volume=l;
 	l=(((l*0x50)/100)<<8)|((r*0x50)/100); /*0x5A, 0x60*/
 	ioctl(gp2x_dev[2], SOUND_MIXER_WRITE_PCM, &l); /*SOUND_MIXER_WRITE_VOLUME*/
}


void gp2x_timer_delay(unsigned long ticks)
{
	unsigned long ini=gp2x_timer_read();
	while (gp2x_timer_read()-ini<ticks);
}


unsigned long gp2x_timer_read(void)
{
 	return gp2x_memregl[0x0A00>>2]/gp2x_ticks_per_second;
}

unsigned long gp2x_timer_read_real(void)
{
 	return gp2x_memregl[0x0A00>>2];
}

unsigned long gp2x_timer_read_scale(void)
{
 	return gp2x_ticks_per_second;
}

void gp2x_timer_profile(void)
{
	static unsigned long i=0;
	if (!i) i=gp2x_memregl[0x0A00>>2];
	else {
		printf("%u\n",gp2x_memregl[0x0A00>>2]-i);
		i=0;	
	}
}

static pthread_t gp2x_sound_thread=0;								// Thread for gp2x_sound_thread_play()
static volatile int gp2x_sound_thread_exit=0;							// Flag to end gp2x_sound_thread_play() thread
static volatile int gp2x_sound_buffer=0;							// Current sound buffer
static short gp2x_sound_buffers_total[(MAX_SAMPLE_RATE*16)/30];					// Sound buffer
static void *gp2x_sound_buffers[16] = {								// Sound buffers
	(void *)(gp2x_sound_buffers_total+((MAX_SAMPLE_RATE*0)/30)),
	(void *)(gp2x_sound_buffers_total+((MAX_SAMPLE_RATE*1)/30)),
	(void *)(gp2x_sound_buffers_total+((MAX_SAMPLE_RATE*2)/30)),
	(void *)(gp2x_sound_buffers_total+((MAX_SAMPLE_RATE*3)/30)),
	(void *)(gp2x_sound_buffers_total+((MAX_SAMPLE_RATE*4)/30)),
	(void *)(gp2x_sound_buffers_total+((MAX_SAMPLE_RATE*5)/30)),
	(void *)(gp2x_sound_buffers_total+((MAX_SAMPLE_RATE*6)/30)),
	(void *)(gp2x_sound_buffers_total+((MAX_SAMPLE_RATE*7)/30)),
	(void *)(gp2x_sound_buffers_total+((MAX_SAMPLE_RATE*8)/30)),
	(void *)(gp2x_sound_buffers_total+((MAX_SAMPLE_RATE*9)/30)),
	(void *)(gp2x_sound_buffers_total+((MAX_SAMPLE_RATE*10)/30)),
	(void *)(gp2x_sound_buffers_total+((MAX_SAMPLE_RATE*11)/30)),
	(void *)(gp2x_sound_buffers_total+((MAX_SAMPLE_RATE*12)/30)),
	(void *)(gp2x_sound_buffers_total+((MAX_SAMPLE_RATE*13)/30)),
	(void *)(gp2x_sound_buffers_total+((MAX_SAMPLE_RATE*14)/30)),
	(void *)(gp2x_sound_buffers_total+((MAX_SAMPLE_RATE*15)/30))
};
static volatile int sndlen=(MAX_SAMPLE_RATE*2)/60;						// Current sound buffer length

void gp2x_sound_play(void *buff, int len)
{
	int nbuff=(gp2x_sound_buffer+1)&15;							// Sound buffer to write
	memcpy(gp2x_sound_buffers[nbuff],buff,len);					// Write the sound buffer
	gp2x_sound_buffer=nbuff;								// Update the current sound buffer
	sndlen=len;										// Update the sound buffer length
}

void gp2x_sound_thread_mute(void)
{
	memset(gp2x_sound_buffers_total,0,(MAX_SAMPLE_RATE*16*2)/30);
	sndlen=(gp2x_sound_rate*2)/60;
}

static void *gp2x_sound_thread_play(void *none)
{
	int nbuff=gp2x_sound_buffer;								// Number of the sound buffer to play
	do {
		write(gp2x_dev[1], gp2x_sound_buffers[nbuff], sndlen); 				// Play the sound buffer
		ioctl(gp2x_dev[1], SOUND_PCM_SYNC, 0);						// Synchronize Audio
		nbuff=(nbuff+(nbuff!=gp2x_sound_buffer))&15;					// Update the sound buffer to play
	} while(!gp2x_sound_thread_exit);							// Until the end of the sound thread
	pthread_exit(0);
}

void gp2x_sound_thread_start(void)
{
	gp2x_sound_thread=0;
	gp2x_sound_thread_exit=0;
	gp2x_sound_buffer=0;
	gp2x_sound_set_stereo(gp2x_sound_stereo);
	gp2x_sound_set_rate(gp2x_sound_rate);
	sndlen=(gp2x_sound_rate*2)/60;
	gp2x_sound_thread_mute();
	pthread_create( &gp2x_sound_thread, NULL, gp2x_sound_thread_play, NULL);
}

void gp2x_sound_thread_stop(void)
{
	gp2x_sound_thread_exit=1;
	gp2x_timer_delay(500);
	gp2x_sound_thread=0;
	gp2x_sound_thread_mute();
}

void gp2x_sound_set_rate(int rate)
{
	ioctl(gp2x_dev[1], SNDCTL_DSP_SPEED,  &rate);
}

void gp2x_sound_set_stereo(int stereo)
{
  	ioctl(gp2x_dev[1], SNDCTL_DSP_STEREO, &stereo);
}

void gp2x_init(int ticks_per_second, int bpp, int rate, int bits, int stereo, int Hz)
{
  	gp2x_ticks_per_second=7372800/ticks_per_second; 

  	gp2x_dev[0] = open("/dev/mem",   O_RDWR); 
  	gp2x_dev[1] = open("/dev/dsp",   O_WRONLY);
  	gp2x_dev[2] = open("/dev/mixer", O_WRONLY);

       	gp2x_memregl=(unsigned long  *)mmap(0, 0x10000, PROT_READ|PROT_WRITE, MAP_SHARED, gp2x_dev[0], 0xc0000000);
        gp2x_memregs=(unsigned short *)gp2x_memregl;

	gp2x_sound_volume(100,100);
       	gp2x_memregs[0x0F16>>1] = 0x830a; usleep(100000);
       	gp2x_memregs[0x0F58>>1] = 0x100c; usleep(100000); 

	/* Upper memory access */
	gp2x_dualcore_ram_size=0x2000000;
	gp2x_dualcore_ram=(unsigned char  *)mmap(0, gp2x_dualcore_ram_size, PROT_READ|PROT_WRITE, MAP_SHARED, gp2x_dev[0], 0x02000000);
	upper_malloc_init(gp2x_dualcore_ram);

	/* Take video memory */
        gp2x_physvram[0]=0x4000000-640*480*2;
        gp2x_logvram15[0]=(unsigned short *)upper_take(gp2x_physvram[0],640*480*2);
       
        gp2x_physvram[1]=0x4000000-640*480*4;
        gp2x_logvram15[1]=(unsigned short *)upper_take(gp2x_physvram[1],640*480*2);

	gp2x_screen15=gp2x_logvram15[0];
	gp2x_screen8=(unsigned char *)gp2x_screen15;	
	gp2x_nflip=0;

 	ioctl(gp2x_dev[1], SNDCTL_DSP_SETFMT, &bits);
  	ioctl(gp2x_dev[1], SNDCTL_DSP_STEREO, &stereo);
	ioctl(gp2x_dev[1], SNDCTL_DSP_SPEED,  &rate);
	//stereo = 0x80000|7; ioctl(gp2x_dev[1], SNDCTL_DSP_SETFRAGMENT, &stereo);
	//stereo = 0x00100009; ioctl(gp2x_dev[1], SNDCTL_DSP_SETFRAGMENT, &stereo);
	stereo = 0x00100007; ioctl(gp2x_dev[1], SNDCTL_DSP_SETFRAGMENT, &stereo);

	cpuctrl_init(); /* cpuctrl.cpp */
	Disable_940(); /* cpuctrl.cpp */

#ifdef MMUHACK
	/* MMU Tables Hack by Squidge */
	mmuhack(); /* squidgehack.cpp */
#endif
	/* RAM Tweaks */
	set_ram_tweaks();

  	//gp2x_memregs[0x28DA>>1]=(((bpp+1)/8)<<9)|0xAB; /*8/15/16/24bpp...*/
  	//gp2x_memregs[0x290C>>1]=320*((bpp+1)/8); /*line width in bytes*/
	gp2x_set_video_mode(bpp,320,240);

	gp2x_video_color8(0,0,0,0);
	gp2x_video_color8(255,255,255,255);
	gp2x_video_setpalette();
	
	/* USB Joysticks Initialization */
	gp2x_usbjoy_init();

	/* Set GP2X Clock */
	gp2x_set_clock(gp2x_clock);

	/* atexit(gp2x_deinit); */
}

#if defined(__cplusplus)
extern "C" {
#endif
extern int fcloseall(void);
#if defined(__cplusplus)
}
#endif

void gp2x_deinit(void)
{
  	memset(gp2x_screen15, 0, 640*480*2); gp2x_video_flip();
  	memset(gp2x_screen15, 0, 640*480*2); gp2x_video_flip();

	/* USB Joysticks Close */
	gp2x_usbjoy_close();

#ifdef MMUHACK
	//flushcache(gp2x_screen8,gp2x_screen8+(640*480*2),0);
	mmuunhack(); /* squidgehack.cpp */
#endif
	cpuctrl_deinit(); /* cpuctrl.cpp */
	gp2x_memregs[0x28DA>>1]=0x4AB; /*set video mode*/
	gp2x_memregs[0x290C>>1]=640;   
  	//munmap((void *)gp2x_dualcore_ram, gp2x_dualcore_ram_size);
  	munmap((void *)gp2x_memregl, 0x10000);
 	close(gp2x_dev[2]);
 	close(gp2x_dev[1]);
 	close(gp2x_dev[0]);
	fcloseall(); /*close all files*/
}

void gp2x_set_clock(int mhz)
{
	/* set_display_clock_div(16+8*(mhz>=220)); */ // Fixed the LCD timing issue (washed screen). Thx, god_at_hell and Aruseman.
	set_FCLK(mhz);
	set_DCLK_Div(0);
	set_920_Div(0);
}

void gp2x_set_video_mode(int bpp,int width,int height)
{
	float x, y;
	float xscale,yscale;

  	memset(gp2x_screen15, 0, 640*480*2); gp2x_video_flip();
  	memset(gp2x_screen15, 0, 640*480*2); gp2x_video_flip();
	gp2x_video_flip_single();

  	gp2x_memregs[0x28DA>>1]=(((bpp+1)/8)<<9)|0xAB; /*8/15/16/24bpp...*/
	bpp=(gp2x_memregs[0x28DA>>1]>>9)&0x3;  /* bytes per pixel */

	gp2x_pal_50hz=0;
	
	if(gp2x_memregs[0x2800>>1]&0x100) /* TV-Out ON? */
	{
		xscale=489.0; /* X-Scale with TV-Out (PAL or NTSC) */
		if (gp2x_memregs[0x2818>>1]  == 287) /* PAL? */
		{
			yscale=(width*274.0)/320.0; /* Y-Scale with PAL */
			gp2x_pal_50hz=1;
		}
		else if (gp2x_memregs[0x2818>>1]  == 239) /* NTSC? */
			yscale=(width*331.0)/320.0; /* Y-Scale with NTSC */
	}
  	else /* TV-Out OFF? */
  	{
		xscale=1024.0; /* X-Scale for LCD */
		yscale=width; /* Y-Scale for LCD */
	}

	x=(xscale*width)/320.0;
	y=(yscale*bpp*height)/240.0;
	gp2x_memregs[0x2906>>1]=(unsigned short)x; /* scale horizontal */
	gp2x_memregl[0x2908>>2]=(unsigned  long)y; /* scale vertical */
	gp2x_memregs[0x290C>>1]=width*bpp; /* Set Video Width */
}

void set_ram_tweaks(void)
{
	int args[7]={6,4,1,1,1,2,2};
	if (gp2x_ram_tweaks)
	{
		printf("RAM Tweaks Activated\n");
		if((args[0]!=-1) && (args[0]-1 < 16)) set_tRC(args[0]-1);
		if((args[1]!=-1) && (args[1]-1 < 16)) set_tRAS(args[1]-1);
		if((args[2]!=-1) && (args[2]-1 < 16)) set_tWR(args[2]-1);
		if((args[3]!=-1) && (args[3]-1 < 16)) set_tMRD(args[3]-1);
		if((args[4]!=-1) && (args[4]-1 < 16)) set_tRFC(args[4]-1);
		if((args[5]!=-1) && (args[5] < 16)) set_tRP(args[5]-1);
		if((args[6]!=-1) && (args[6]-1 < 16)) set_tRCD(args[6]-1);
	}
}

static unsigned char fontdata8x8[] =
{
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x3C,0x42,0x99,0xBD,0xBD,0x99,0x42,0x3C,0x3C,0x42,0x81,0x81,0x81,0x81,0x42,0x3C,
	0xFE,0x82,0x8A,0xD2,0xA2,0x82,0xFE,0x00,0xFE,0x82,0x82,0x82,0x82,0x82,0xFE,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x38,0x64,0x74,0x7C,0x38,0x00,0x00,
	0x80,0xC0,0xF0,0xFC,0xF0,0xC0,0x80,0x00,0x01,0x03,0x0F,0x3F,0x0F,0x03,0x01,0x00,
	0x18,0x3C,0x7E,0x18,0x7E,0x3C,0x18,0x00,0xEE,0xEE,0xEE,0xCC,0x00,0xCC,0xCC,0x00,
	0x00,0x00,0x30,0x68,0x78,0x30,0x00,0x00,0x00,0x38,0x64,0x74,0x7C,0x38,0x00,0x00,
	0x3C,0x66,0x7A,0x7A,0x7E,0x7E,0x3C,0x00,0x0E,0x3E,0x3A,0x22,0x26,0x6E,0xE4,0x40,
	0x18,0x3C,0x7E,0x3C,0x3C,0x3C,0x3C,0x00,0x3C,0x3C,0x3C,0x3C,0x7E,0x3C,0x18,0x00,
	0x08,0x7C,0x7E,0x7E,0x7C,0x08,0x00,0x00,0x10,0x3E,0x7E,0x7E,0x3E,0x10,0x00,0x00,
	0x58,0x2A,0xDC,0xC8,0xDC,0x2A,0x58,0x00,0x24,0x66,0xFF,0xFF,0x66,0x24,0x00,0x00,
	0x00,0x10,0x10,0x38,0x38,0x7C,0xFE,0x00,0xFE,0x7C,0x38,0x38,0x10,0x10,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1C,0x1C,0x1C,0x18,0x00,0x18,0x18,0x00,
	0x6C,0x6C,0x24,0x00,0x00,0x00,0x00,0x00,0x00,0x28,0x7C,0x28,0x7C,0x28,0x00,0x00,
	0x10,0x38,0x60,0x38,0x0C,0x78,0x10,0x00,0x40,0xA4,0x48,0x10,0x24,0x4A,0x04,0x00,
	0x18,0x34,0x18,0x3A,0x6C,0x66,0x3A,0x00,0x18,0x18,0x20,0x00,0x00,0x00,0x00,0x00,
	0x30,0x60,0x60,0x60,0x60,0x60,0x30,0x00,0x0C,0x06,0x06,0x06,0x06,0x06,0x0C,0x00,
	0x10,0x54,0x38,0x7C,0x38,0x54,0x10,0x00,0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00,
	0x00,0x00,0x00,0x00,0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00,0x3E,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x04,0x08,0x10,0x20,0x40,0x00,0x00,
	0x38,0x4C,0xC6,0xC6,0xC6,0x64,0x38,0x00,0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00,
	0x7C,0xC6,0x0E,0x3C,0x78,0xE0,0xFE,0x00,0x7E,0x0C,0x18,0x3C,0x06,0xC6,0x7C,0x00,
	0x1C,0x3C,0x6C,0xCC,0xFE,0x0C,0x0C,0x00,0xFC,0xC0,0xFC,0x06,0x06,0xC6,0x7C,0x00,
	0x3C,0x60,0xC0,0xFC,0xC6,0xC6,0x7C,0x00,0xFE,0xC6,0x0C,0x18,0x30,0x30,0x30,0x00,
	0x78,0xC4,0xE4,0x78,0x86,0x86,0x7C,0x00,0x7C,0xC6,0xC6,0x7E,0x06,0x0C,0x78,0x00,
	0x00,0x00,0x18,0x00,0x00,0x18,0x00,0x00,0x00,0x00,0x18,0x00,0x00,0x18,0x18,0x30,
	0x1C,0x38,0x70,0xE0,0x70,0x38,0x1C,0x00,0x00,0x7C,0x00,0x00,0x7C,0x00,0x00,0x00,
	0x70,0x38,0x1C,0x0E,0x1C,0x38,0x70,0x00,0x7C,0xC6,0xC6,0x1C,0x18,0x00,0x18,0x00,
	0x3C,0x42,0x99,0xA1,0xA5,0x99,0x42,0x3C,0x38,0x6C,0xC6,0xC6,0xFE,0xC6,0xC6,0x00,
	0xFC,0xC6,0xC6,0xFC,0xC6,0xC6,0xFC,0x00,0x3C,0x66,0xC0,0xC0,0xC0,0x66,0x3C,0x00,
	0xF8,0xCC,0xC6,0xC6,0xC6,0xCC,0xF8,0x00,0xFE,0xC0,0xC0,0xFC,0xC0,0xC0,0xFE,0x00,
	0xFE,0xC0,0xC0,0xFC,0xC0,0xC0,0xC0,0x00,0x3E,0x60,0xC0,0xCE,0xC6,0x66,0x3E,0x00,
	0xC6,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0x00,0x7E,0x18,0x18,0x18,0x18,0x18,0x7E,0x00,
	0x06,0x06,0x06,0x06,0xC6,0xC6,0x7C,0x00,0xC6,0xCC,0xD8,0xF0,0xF8,0xDC,0xCE,0x00,
	0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00,0xC6,0xEE,0xFE,0xFE,0xD6,0xC6,0xC6,0x00,
	0xC6,0xE6,0xF6,0xFE,0xDE,0xCE,0xC6,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,
	0xFC,0xC6,0xC6,0xC6,0xFC,0xC0,0xC0,0x00,0x7C,0xC6,0xC6,0xC6,0xDE,0xCC,0x7A,0x00,
	0xFC,0xC6,0xC6,0xCE,0xF8,0xDC,0xCE,0x00,0x78,0xCC,0xC0,0x7C,0x06,0xC6,0x7C,0x00,
	0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,
	0xC6,0xC6,0xC6,0xEE,0x7C,0x38,0x10,0x00,0xC6,0xC6,0xD6,0xFE,0xFE,0xEE,0xC6,0x00,
	0xC6,0xEE,0x3C,0x38,0x7C,0xEE,0xC6,0x00,0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00,
	0xFE,0x0E,0x1C,0x38,0x70,0xE0,0xFE,0x00,0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00,
	0x60,0x60,0x30,0x18,0x0C,0x06,0x06,0x00,0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00,
	0x18,0x3C,0x66,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,
	0x30,0x30,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x3C,0x06,0x3E,0x66,0x66,0x3C,0x00,
	0x60,0x7C,0x66,0x66,0x66,0x66,0x7C,0x00,0x00,0x3C,0x66,0x60,0x60,0x66,0x3C,0x00,
	0x06,0x3E,0x66,0x66,0x66,0x66,0x3E,0x00,0x00,0x3C,0x66,0x66,0x7E,0x60,0x3C,0x00,
	0x1C,0x30,0x78,0x30,0x30,0x30,0x30,0x00,0x00,0x3E,0x66,0x66,0x66,0x3E,0x06,0x3C,
	0x60,0x7C,0x76,0x66,0x66,0x66,0x66,0x00,0x18,0x00,0x38,0x18,0x18,0x18,0x18,0x00,
	0x0C,0x00,0x1C,0x0C,0x0C,0x0C,0x0C,0x38,0x60,0x60,0x66,0x6C,0x78,0x6C,0x66,0x00,
	0x38,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0x00,0xEC,0xFE,0xFE,0xFE,0xD6,0xC6,0x00,
	0x00,0x7C,0x76,0x66,0x66,0x66,0x66,0x00,0x00,0x3C,0x66,0x66,0x66,0x66,0x3C,0x00,
	0x00,0x7C,0x66,0x66,0x66,0x7C,0x60,0x60,0x00,0x3E,0x66,0x66,0x66,0x3E,0x06,0x06,
	0x00,0x7E,0x70,0x60,0x60,0x60,0x60,0x00,0x00,0x3C,0x60,0x3C,0x06,0x66,0x3C,0x00,
	0x30,0x78,0x30,0x30,0x30,0x30,0x1C,0x00,0x00,0x66,0x66,0x66,0x66,0x6E,0x3E,0x00,
	0x00,0x66,0x66,0x66,0x66,0x3C,0x18,0x00,0x00,0xC6,0xD6,0xFE,0xFE,0x7C,0x6C,0x00,
	0x00,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00,0x00,0x66,0x66,0x66,0x66,0x3E,0x06,0x3C,
	0x00,0x7E,0x0C,0x18,0x30,0x60,0x7E,0x00,0x0E,0x18,0x0C,0x38,0x0C,0x18,0x0E,0x00,
	0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00,0x70,0x18,0x30,0x1C,0x30,0x18,0x70,0x00,
	0x00,0x00,0x76,0xDC,0x00,0x00,0x00,0x00,0x10,0x28,0x10,0x54,0xAA,0x44,0x00,0x00,
};

static void gp2x_text(unsigned char *screen, int x, int y, char *text, int color)
{
	unsigned int i,l;
	screen=screen+x+y*320;

	for (i=0;i<strlen(text);i++) {
		
		for (l=0;l<8;l++) {
			screen[l*320+0]=(fontdata8x8[((text[i])*8)+l]&0x80)?color:screen[l*320+0];
			screen[l*320+1]=(fontdata8x8[((text[i])*8)+l]&0x40)?color:screen[l*320+1];
			screen[l*320+2]=(fontdata8x8[((text[i])*8)+l]&0x20)?color:screen[l*320+2];
			screen[l*320+3]=(fontdata8x8[((text[i])*8)+l]&0x10)?color:screen[l*320+3];
			screen[l*320+4]=(fontdata8x8[((text[i])*8)+l]&0x08)?color:screen[l*320+4];
			screen[l*320+5]=(fontdata8x8[((text[i])*8)+l]&0x04)?color:screen[l*320+5];
			screen[l*320+6]=(fontdata8x8[((text[i])*8)+l]&0x02)?color:screen[l*320+6];
			screen[l*320+7]=(fontdata8x8[((text[i])*8)+l]&0x01)?color:screen[l*320+7];
		}
		screen+=8;
	} 
}

void gp2x_gamelist_text_out(int x, int y, char *eltexto)
{
	char texto[33];
	strncpy(texto,eltexto,32);
	texto[32]=0;
	if (texto[0]!='-')
		gp2x_text(gp2x_screen8,x+1,y+1,texto,0);
	gp2x_text(gp2x_screen8,x,y,texto,255);
}

/* Variadic functions guide found at http://www.unixpapa.com/incnote/variadic.html */
void gp2x_gamelist_text_out_fmt(int x, int y, char* fmt, ...)
{
	char strOut[128];
	va_list marker;
	
	va_start(marker, fmt);
	vsprintf(strOut, fmt, marker);
	va_end(marker);	

	gp2x_gamelist_text_out(x, y, strOut);
}

static int log=0;

void gp2x_printf_init(void)
{
	log=0;
}

static void gp2x_text_log(char *texto)
{
	if (!log)
	{
		memset(gp2x_screen8,0,320*240);
	}
	gp2x_text(gp2x_screen8,0,log,texto,255);
	log+=8;
	if(log>239) log=0;
}

/* Variadic functions guide found at http://www.unixpapa.com/incnote/variadic.html */
void gp2x_printf(char* fmt, ...)
{
	int i,c;
	char strOut[4096];
	char str[41];
	va_list marker;
	
	va_start(marker, fmt);
	vsprintf(strOut, fmt, marker);
	va_end(marker);	

	c=0;
	for (i=0;i<strlen(strOut);i++)
	{
		str[c]=strOut[i];
		if (str[c]=='\n')
		{
			str[c]=0;
			gp2x_text_log(str);
			c=0;
		}
		else if (c==39)
		{
			str[40]=0;
			gp2x_text_log(str);
			c=0;
		}		
		else
		{
			c++;
		}
	}
}