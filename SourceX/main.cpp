#define DEV_FB "/dev"
#define FB_DEV_NAME "fb"

#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <linux/fb.h>
#include <linux/input.h>

#include <SDL.h>
#include <string>

#include "devilution.h"

#if !defined(__APPLE__)
extern "C" const char *__asan_default_options()
{
	return "halt_on_error=0";
}
#endif

struct pollfd evpoll;

int hatX;
int hatY;

struct fb_t {
	uint16_t pixel[8][8];
};

int running = 1;

struct fb_t *fb;

static int is_framebuffer_device(const struct dirent *dir)
{
	return strncmp(FB_DEV_NAME, dir->d_name,
	           strlen(FB_DEV_NAME) - 1)
	    == 0;
}

static int open_fbdev(const char *dev_name)
{
	struct dirent **namelist;
	int i, ndev;
	int fd = -1;
	struct fb_fix_screeninfo fix_info;

	ndev = scandir(DEV_FB, &namelist, is_framebuffer_device, 0);
	if (ndev <= 0)
		return ndev;

	for (i = 0; i < ndev; i++) {
		char fname[64];
		char name[256];

		snprintf(fname, sizeof(fname),
		    "%s/%s", DEV_FB, namelist[i]->d_name);
		fd = open(fname, O_RDWR);
		if (fd < 0)
			continue;
		ioctl(fd, FBIOGET_FSCREENINFO, &fix_info);
		if (strcmp(dev_name, fix_info.id) == 0)
			break;
		close(fd);
		fd = -1;
	}
	for (i = 0; i < ndev; i++)
		free(namelist[i]);

	return fd;
}

uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue)
{
	return ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3);
}

void change_dir(unsigned int code)
{
	switch (code) {
	case KEY_UP:
		hatX += 1;
		break;
	case KEY_RIGHT:
		hatY += 1;
		break;
	case KEY_DOWN:
		hatX -= 1;
		break;
	case KEY_LEFT:
		hatY -= 1;
		break;
	case KEY_ENTER:
		printf("enter\n");
		break;
	}
}

void handle_events(int evfd)
{
	struct input_event ev[64];
	int i, rd;

	rd = read(evfd, ev, sizeof(struct input_event) * 64);
	if (rd < (int)sizeof(struct input_event)) {
		fprintf(stderr, "expected %d bytes, got %d\n",
		    (int)sizeof(struct input_event), rd);
		return;
	}
	for (i = 0; i < rd / sizeof(struct input_event); i++) {
		if (ev->type != EV_KEY)
			continue;
		if (ev->value != 1)
			continue;
		change_dir(ev->code);
	}
}

void hatAction()
{
	int mx, my;
	int it = -1, o = -1, mi = -1;
	dvl::PlayerStruct *p = &dvl::plr[dvl::myplr];
	mx = p->WorldX + hatX;
	my = p->WorldY + hatY;

	if (dvl::dItem[mx][my] > 0) {
		it = dvl::dItem[mx][my] - 1;
		if (dvl::item[it]._iSelFlag != 1 && dvl::item[it]._iSelFlag != 3) {
			it = -1;
		}
	}
	if (dvl::dObject[mx][my] != 0) {
		o = dvl::dObject[mx][my] > 0 ? dvl::dObject[mx][my] - 1 : -(dvl::dObject[mx][my] + 1);
		if (dvl::object[o]._oSelFlag != 1 && dvl::object[o]._oSelFlag != 3) {
			o = -1;
		}
	}
	if (dvl::dMonster[mx][my] != 0 && (dvl::dFlags[mx][my] & dvl::DFLAG_LIT)) {
		mi = dvl::dMonster[mx][my] > 0 ? dvl::dMonster[mx][my] - 1 : -(dvl::dMonster[mx][my] + 1);
		if (dvl::monster[mi]._mhitpoints >> 6 <= 0 && !(dvl::monster[mi].MData->mSelFlag & 1)) {
			mi = -1;
		}
	}

	if (it != -1) {
		dvl::NetSendCmdLocParam1(true, dvl::CMD_GOTOAGETITEM, mx, my, it);
	} else if (o != -1) {
		dvl::NetSendCmdLocParam1(true, dvl::CMD_OPOBJXY, mx, my, o);
	} else if (mi != -1) {
		dvl::NetSendCmdParam1(true, dvl::CMD_ATTACKID, mi);
		//} else if (pcursplr != -1 && !FriendlyMode) {
		//NetSendCmdParam1(true, CMD_ATTACKPID, pcursplr);
	} else {
		dvl::NetSendCmdLoc(true, dvl::CMD_WALKXY, mx, my);
	}
}

void navigate()
{
	hatX = 0;
	hatY = 0;

	while (poll(&evpoll, 1, 0) > 0) {
		handle_events(evpoll.fd);
	}

	if (hatX > 1)
		hatX = 1;

	if (hatX < -1)
		hatX = -1;

	if (hatY > 1)
		hatY = 1;

	if (hatY < -1)
		hatY = -1;

	if (hatX || hatY)
		hatAction();
}

void hatRender()
{
	navigate();
	//memset(fb, 0, 128);
	dvl::PlayerStruct *p = &dvl::plr[dvl::myplr];

	for (int x = 0; x < 7; x++) {
		for (int y = 0; y < 7; y++) {
			int wx = p->WorldX + x - 3;
			int wy = p->WorldY + y - 3;
			dvl::PALETTEENTRY color;
			color.peRed = 0;
			color.peGreen = 0;
			color.peBlue = 0;
			if ((x == 3 && y == 3) || dvl::dPlayer[wx][wy] > 0) {
				color.peRed = 64;
				color.peGreen = 160;
				color.peBlue = 64;
			} else if ((dvl::dFlags[wx][wy] & dvl::DFLAG_VISIBLE) && (dvl::dFlags[wx][wy] & dvl::DFLAG_LIT) && (dvl::dFlags[wx][wy] & dvl::DFLAG_MISSILE)) {
				color.peRed = 255;
				color.peGreen = 255;
				color.peBlue = 64;
			} else if (dvl::dMonster[wx][wy] > 0 && (dvl::dFlags[wx][wy] & dvl::DFLAG_VISIBLE) && (dvl::dFlags[wx][wy] & dvl::DFLAG_LIT)) {
				color.peRed = 255;
			} else if (dvl::dItem[wx][wy]) {
				color.peBlue = 48;
			} else if (dvl::dObject[wx][wy]) {
				color.peRed = 200;
				color.peGreen = 200;
				color.peBlue = 80;
			} else if (!dvl::CheckNoSolid(wx, wy)) {
				color.peRed = 180;
				color.peGreen = 180;
				color.peBlue = 180;
			}

			if (p->_pHitPoints >> 6 == 0) {
				color.peGreen = 0;
				color.peBlue = 0;
			}

			int ax = abs(x - 3);
			int ay = abs(y - 3);
			ax *= ax;
			ay *= ay;
			int l = (int)((MAXVISION - p->_pLightRad) * (sqrt(ax + ay) * 1.5));
			//printf("%d", l);
			if (color.peRed)
				color.peRed = std::max(48, color.peRed - l);
			if (color.peGreen)
				color.peGreen = std::max(48, color.peGreen - l);
			if (color.peBlue)
				color.peBlue = std::max(48, color.peBlue - l);

			fb->pixel[7 - x][y] = rgb565(color.peRed, color.peGreen, color.peBlue);
		}
		//printf("\n");
	}
	//printf("\n");

	if (false) {
		int hp = (int)((double)p->_pHitPoints / p->_pMaxHP * 8) - 1;
		int mana = 7 - (int)((double)p->_pMana / p->_pMaxMana * 8);
		for (int x = 0; x < 8; x++) {
			fb->pixel[7 - x][7] = rgb565(x <= hp ? 255 : 0, 0, x <= mana ? 0 : 255);
		}

		int xpo = dvl::ExpLvlsTbl[p->_pLevel - 1];
		int xp = (int)((double)(p->_pExperience - xpo) / (p->_pNextExper - xpo) * 7);
		for (int y = 0; y < 7; y++) {
			fb->pixel[0][y] = 7 - y <= xp ? rgb565(148, 148, 70) : 0;
		}
	} else {
		int hp = 7 - (int)((double)p->_pHitPoints / p->_pMaxHP * 8);
		for (int x = 0; x < 8; x++) {
			fb->pixel[7 - x][7] = x <= hp ? 0 : 0xF800;
		}

		int xp = (int)((double)p->_pMana / p->_pMaxMana * 7);
		for (int y = 0; y < 7; y++) {
			fb->pixel[0][y] = 7 - y <= xp ? 0x1F : 0;
		}
	}
}

static int is_event_device(const struct dirent *dir)
{
	return strncmp("event", dir->d_name,
	           strlen("event") - 1)
	    == 0;
}

static int open_evdev(const char *dev_name)
{
	struct dirent **namelist;
	int i, ndev;
	int fd = -1;
	char devInput[11] = "/dev/input";

	ndev = scandir(devInput, &namelist, is_event_device, 0);
	if (ndev <= 0)
		return ndev;

	for (i = 0; i < ndev; i++) {
		char fname[64];
		char name[256];

		snprintf(fname, sizeof(fname),
		    "%s/%s", devInput, namelist[i]->d_name);
		fd = open(fname, O_RDONLY);
		if (fd < 0)
			continue;
		ioctl(fd, EVIOCGNAME(sizeof(name)), name);
		if (strcmp(dev_name, name) == 0)
			break;
		close(fd);
	}

	for (i = 0; i < ndev; i++)
		free(namelist[i]);

	return fd;
}

void initHat()
{
	evpoll.events = POLLIN;

	evpoll.fd = open_evdev("Raspberry Pi Sense HAT Joystick");
	if (evpoll.fd < 0) {
		fprintf(stderr, "Event device not found.\n");
	}

	int fbfd = open_fbdev("RPi-Sense FB");
	if (fbfd <= 0) {
		printf("Error: cannot open framebuffer device.\n");
	}

	fb = (fb_t *)mmap(0, 128, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
	if (!fb) {
		close(fbfd);
	}
	memset(fb, 0, 128);
	//close(evpoll.fd);
	//memset(fb, 0, 128);
	//munmap(fb, 128);
	//close(fbfd);
}

static std::string build_cmdline(int argc, char **argv)
{
	std::string str;
	for (int i = 1; i < argc; i++) {
		if (i != 1) {
			str += ' ';
		}
		str += argv[i];
	}
	return str;
}

int main(int argc, char **argv)
{
	initHat();
	auto cmdline = build_cmdline(argc, argv);
	return dvl::WinMain(NULL, NULL, (char *)cmdline.c_str(), 0);
}
