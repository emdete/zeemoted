/*
 * Copyright (C) 2009-2010 Till Harbaum <till@harbaum.org>.
 * Copyright (C) 2011 M. Dietrich <mdt@pyneo.org>.
 *
 * This file is part of zeemoted.
 *
 * zeemoted is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * zeemoted is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with zeemoted. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "zeemote.h"

typedef enum out_types {
	mode_joystick, // emulate a js
	mode_keyboard, // send keystrokes to linux kernel
	mode_fakekey, // fake x11 keystrokes
} out_types;

/********************** lib fake key ***************************/
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xlib.h>
#include <X11/XF86keysym.h>
#include <fakekey/fakekey.h>

static int fakekey_keys[] = {
	XK_Left, // js
	XK_Right, // js
	XK_Up, // js
	XK_Down, // js
	XK_Return, // a
	XK_KP_Space, // b
	XK_Escape, // c
	XK_BackSpace, // d
	0 };

static int init_fakekey() {
	Display* d = XOpenDisplay(getenv("DISPLAY"));
	FakeKey* f = fakekey_init(d);
	return (int)f;
}

static int do_uinput_fakekey(int fd, unsigned short key, int pressed) {
	FakeKey* f = (FakeKey*)fd;
	if (pressed)
		return fakekey_press_keysym(f, fakekey_keys[key], 0);
	else
		fakekey_release(f);
	return pressed? key: 0;
}

/********************** linux uinput ***************************/
#include <linux/uinput.h>

/* Create uinput output */
static int do_uinput(int fd, unsigned short key, int pressed, unsigned short event_type) {
	struct input_event event;
	memset(&event, 0 , sizeof(event));
	gettimeofday(&event.time, NULL);
	event.type = event_type;
	event.code = key;
	event.value = pressed;
	if (write(fd,&event,sizeof(event)) != sizeof(event))
		perror("Writing event");
	return pressed? key: 0;
}

/********************** keyboard uinput ************************/
static int keys[] = {
	KEY_LEFT,
	KEY_RIGHT,
	KEY_UP,
	KEY_DOWN,
	KEY_ENTER,
	KEY_SPACE,
	KEY_ESC,
	KEY_BACKSPACE,
	0 };

static int init_uinput_keyboard() {
	char* state = "init";
	struct uinput_setup usetup;
	int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
	state = "open";
	if (fd < 0)
		goto ERROR;
	state = "UI_SET_EVBIT";
	if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0)
		goto ERROR;
	state = "UI_SET_KEYBIT";
	if (ioctl(fd, UI_SET_KEYBIT, KEY_SPACE) < 0)
		goto ERROR;
	memset(&usetup, 0, sizeof(usetup));
	usetup.id.bustype = BUS_VIRTUAL;
	usetup.id.vendor = 0x1234; /* sample vendor */
	usetup.id.product = 0x5678; /* sample product */
	strcpy(usetup.name, "Zeemote");
	state = "UI_DEV_SETUP";
	if (ioctl(fd, UI_DEV_SETUP, &usetup) < 0)
		goto ERROR;
	state = "UI_DEV_CREATE";
	if (ioctl(fd, UI_DEV_CREATE) < 0)
		goto ERROR;
	return fd;
ERROR:
	perror("opening/controling uinput");
	fprintf(stderr, "Error while %s\n", state);
	if (fd>=0)
		close(fd);
	return -1;
}

/********************** linux joystick *************************/
static int init_uinput_joystick() {
	char* state = "init";
	int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
	state = "open";
	if (fd < 0)
		goto ERROR;
	struct uinput_setup usetup;
	memset(&usetup, 0, sizeof(usetup));
	usetup.id.bustype = BUS_VIRTUAL;
	usetup.id.vendor = 0x1234; /* sample vendor */
	usetup.id.product = 0x5678; /* sample product */
	strcpy(usetup.name, "Zeemote");
	state = "UI_DEV_SETUP";
	if (ioctl(fd, UI_DEV_SETUP, &usetup) < 0)
		goto ERROR;
	state = "UI_SET_EVBIT EV_ABS";
	if (ioctl(fd, UI_SET_EVBIT, EV_ABS) < 0)
		goto ERROR;
	state = "UI_SET_ABSBIT ABS_X";
	if (ioctl(fd, UI_SET_ABSBIT, ABS_X) < 0)
		goto ERROR;
	state = "UI_SET_ABSBIT ABS_Y";
	if (ioctl(fd, UI_SET_ABSBIT, ABS_Y) < 0)
		goto ERROR;
	state = "UI_SET_EVBIT EV_KEY";
	if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0)
		goto ERROR;
	state = "UI_SET_KEYBIT BTN_0";
	if (ioctl(fd, UI_SET_KEYBIT, BTN_0) < 0)
		goto ERROR;
	state = "UI_SET_KEYBIT BTN_1";
	if (ioctl(fd, UI_SET_KEYBIT, BTN_1) < 0)
		goto ERROR;
	state = "UI_SET_KEYBIT BTN_2";
	if (ioctl(fd, UI_SET_KEYBIT, BTN_2) < 0)
		goto ERROR;
	state = "UI_SET_KEYBIT BTN_3";
	if (ioctl(fd, UI_SET_KEYBIT, BTN_3) < 0)
		goto ERROR;
	state = "UI_DEV_CREATE";
	if (ioctl(fd, UI_DEV_CREATE) < 0)
		goto ERROR;
	return fd;
ERROR:
	perror("opening/controling uinput");
	fprintf(stderr, "Error while %s\n", state);
	if (fd>=0)
		close(fd);
	return -1;
}

/********************** bluetooth ******************************/
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/rfcomm.h>

static int bluez_connect(bdaddr_t *bdaddr, int channel) {
	int bt = socket(PF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
	struct sockaddr_rc rem_addr;
	/* bluez connects to BlueClient */
	if (bt < 0) {
		fprintf(stderr, "Can't create socket. %s(%d)\n", strerror(errno), errno);
		return bt;
	}
	/* connect on rfcomm */
	memset(&rem_addr, 0, sizeof(rem_addr));
	rem_addr.rc_family = AF_BLUETOOTH;
	rem_addr.rc_bdaddr = *bdaddr;
	rem_addr.rc_channel = channel;
	if (connect(bt, (struct sockaddr *)&rem_addr, sizeof(rem_addr)) < 0 ) {
		fprintf(stderr, "Can't connect. %s(%d)\n", strerror(errno), errno);
		close(bt);
		return -1;
	}
	return bt;
}

static ssize_t read_num(int fd, void *data, size_t count) {
	ssize_t total = 0;
	while (count) {
		ssize_t rd = read(fd, data, count);
		if (rd < 0) {
			perror("read");
			return rd;
		}
		else {
			count -= rd;
			data += rd;
			total += rd;
		}
	}
	return total;
}

static bdaddr_t* inquiry() {
	inquiry_info *info = NULL;
	uint8_t lap[3] = { 0x33, 0x8b, 0x9e };
	int num_rsp;
	int i;
	int mote;
	bdaddr_t *result = NULL;
	num_rsp = hci_inquiry(-1, 8, 0, lap, &info, 0);
	if (num_rsp < 0) {
		perror("Inquiry failed.");
		exit(1);
	}
	for (mote=0, i = 0; i < num_rsp; i++)
		if ((info+i)->dev_class[0] == ((ZEEMOTE_CLASS >> 0) & 0xff)
		&& (info+i)->dev_class[1] == ((ZEEMOTE_CLASS >> 8) & 0xff)
		&& (info+i)->dev_class[2] == ((ZEEMOTE_CLASS >> 16) & 0xff))
			mote++;
	if (!mote) {
		printf("No Zeemotes found\n");
		return result;
	}
	result = malloc(sizeof(bdaddr_t) * (mote + 1));
	for (mote=0, i = 0; i < num_rsp; i++) {
		if ((info+i)->dev_class[0] == ((ZEEMOTE_CLASS >> 0) & 0xff) &&
		(info+i)->dev_class[1] == ((ZEEMOTE_CLASS >> 8) & 0xff) &&
		(info+i)->dev_class[2] == ((ZEEMOTE_CLASS >> 16) & 0xff)) {
			result[mote++] = (info+i)->bdaddr;
		}
	}
	bacpy(&result[mote++], BDADDR_ANY);
	return result;
}

/********************** main program ***************************/
static void usage(void) {
	printf("\n"
		"Usage:\n"
		"\n"
		"\tzeemoted [OPTIONS] [DEVICE ADDRESS]\n"
		"\n"
		"Options:\n"
		"\n"
		"-h\n"
		"\tthis help\n"
		"\n"
		"-j\n"
		"\temulate joystick (default)\n"
		"\n"
		"-k\n"
		"\temulate keyboard\n"
		"\n"
		"-x\n"
		"\tfake x11 keys via libfakekey\n"
		"\n"
		"-a\n"
		"\tmixin audio control for up/down\n"
		"\n"
		"-s NUM\n"
		"\tset axis sensitivity in keyboard mode. Values range from\n"
		"\t1 (very sensitive) to 127 (very insensitive), default is 64\n"
		"\n"
		"-c X:NUM\n"
		"\tset keycode to be returned in keyboard mode. X ist the axis/button to\n"
		"\tbe set: 0=left, 1=right, 2=up, 3=down, 4-7=buttons a-d. Valid keycodes\n"
		"\tcan e.g. be found in the file /usr/include/linux/input.h\n"
		"\tExample: -c 4:28 makes the A button to act as the enter key.\n"
		"\tDefault is to map the axes to the cursor keys and the buttons the the\n"
		"\tkeys a to d.\n"
		"\n"
		);
}

int main(int argc, char **argv) {
	int bt;
	bdaddr_t *bdaddr = NULL;
	out_types kbd_mode = mode_joystick;
	int c;
	int sensitivity = 64;
	//int bounce = 0;
	/* parse options */
	opterr = 0;
	while ((c = getopt(argc, argv, "vjkxas:b:c:h")) != -1) {
		switch (c) {
		case 'v':
			printf("zeemoted V" VERSION " - " "(c) 2009-2010 by Till Harbaum <till@harbaum.org>\n");
		break;
		case 'j':
			kbd_mode = mode_joystick;
			printf("joystick mode enabled\n");
		break;
		case 'k':
			kbd_mode = mode_keyboard;
			printf("keyboard mode enabled\n");
		break;
		case 'x':
			kbd_mode = mode_fakekey;
			printf("fakekey mode enabled\n");
		break;
		case 'a': {
			fakekey_keys[0] = XK_Page_Up;
			fakekey_keys[1] = XK_Page_Down;
			fakekey_keys[2] = XF86XK_AudioRaiseVolume;
			fakekey_keys[3] = XF86XK_AudioLowerVolume;
			keys[0] = KEY_PAGEUP;
			keys[1] = KEY_PAGEDOWN;
			keys[2] = KEY_VOLUMEDOWN;
			keys[3] = KEY_VOLUMEUP;
			printf("audio key-map enabled\n");
		}
		break;
		case 's':
			sensitivity = atoi(optarg);
			if (sensitivity < 1 || sensitivity > 126) {
				printf("Sensivity out of bounds! Must be between 1 and 126\n");
				sensitivity = 64;
			}
			printf("sensitivity switched to %d\n", sensitivity);
		break;
		//case 'b':
			//bounce = atoi(optarg);
		//break;
		case 'c': {
			int index, code;
			if ((sscanf(optarg, "%d:%d\n", &index, &code) == 2) && index >= 0 && index <= 7)
				keys[index] = code;
			else
				usage();
			printf("key %d set to %d\n", index, code);
			}
		break;
		case '?':
			if (optopt == 's')
				fprintf (stderr, "Option -%c requires an argument.\n", optopt);
			else if (isprint(optopt))
				fprintf(stderr, "Unknown option `-%c'.\n", optopt);
			else
				fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
		case 'h':
		default:
			usage();
			return 1;
		}
	}
	if (optind == argc) {
		printf("No device addresses given, trying to autodetect devices ...\n");
		bdaddr = inquiry();
		if (!bdaddr) {
			printf("No devices found\n");
			exit(0);
		}
	}
	else {
		int i = 0;
		bdaddr = malloc(sizeof(bdaddr_t) * (argc - optind + 1));
		while (optind < argc)
			baswap(bdaddr + i++, strtoba(argv[optind++]));
		bacpy(bdaddr + i++, BDADDR_ANY);
	}
	// fork handler for each Zeemote
	while (bacmp(bdaddr, BDADDR_ANY)) {
		char addr[18];
		ba2str(bdaddr, addr);
		printf("Zeemote JS1 device %s\n", addr);
		// open bluetooth rfcomm
		if ((bt = bluez_connect(bdaddr, 1)) >= 0) {
			int fd = -1;
			switch (kbd_mode) {
				case mode_joystick: fd = init_uinput_joystick(); break;
				case mode_keyboard: fd = init_uinput_keyboard(); break;
				case mode_fakekey: fd = init_fakekey(); break;
			}
			if (fd < 0)
				goto ERROR;
			printf("Zeemote JS1 device %s connected for use as %s.\n",
				addr,
				kbd_mode==mode_keyboard?"keyboard":(kbd_mode==mode_joystick?"joystick":"fakekey")
				);
			pid_t pid = fork();
			if (pid < 0) {
				perror("fork() failed");
				exit(errno);
			}
			if (!pid) {
				char old_x_state = 0;
				char old_y_state = 0;
				int old_button_state = 0;
				while (1) {
					zeemote_hdr_t hdr;
					zeemote_data_t data;
					int rd = read_num(bt, &hdr, sizeof(hdr));
					if (rd == sizeof(hdr)) {
						if (hdr.magic == MAGIC && hdr.length >= 2) {
							switch (hdr.type) {
								case ZEEMOTE_STICK: {
									if (hdr.length-2 == sizeof(data.axis)) {
										if (read_num(bt, data.axis, sizeof(data.axis))) {
											if (data.axis[ZEEMOTE_AXIS_UNKNOWN])
												printf("WARNING: ZEEMOTE_STICK axis UNKNOWN != 0!\n");
											int x_state = data.axis[ZEEMOTE_AXIS_X];
											int y_state = data.axis[ZEEMOTE_AXIS_Y];
											switch (kbd_mode) {
												case mode_joystick: {
													if (x_state != old_x_state) {
														do_uinput(fd, ABS_X, x_state*256u, EV_ABS);
														old_x_state = x_state;
													}
													if (y_state != old_y_state) {
														do_uinput(fd, ABS_Y, y_state*256u, EV_ABS);
														old_y_state = y_state;
													}
												}
												break;
												case mode_keyboard: {
													x_state = (x_state < -sensitivity)?-1: ((x_state > sensitivity)?1:0);
													if (x_state != old_x_state) {
														if (old_x_state)
															do_uinput(fd, (old_x_state<0)?keys[0]:keys[1], 0, EV_KEY);
														if (x_state)
															do_uinput(fd, (x_state<0)?keys[0]:keys[1], 1, EV_KEY);
														old_x_state = x_state;
													}
													y_state = (y_state < -sensitivity)?-1: ((y_state > sensitivity)?1:0);
													if (y_state != old_y_state) {
														if (old_y_state)
															do_uinput(fd, (old_y_state<0)?keys[2]:keys[3], 0, EV_KEY);
														if (y_state)
															do_uinput(fd, (y_state<0)?keys[2]:keys[3], 1, EV_KEY);
														old_y_state = y_state;
													}
												}
												break;
												case mode_fakekey: {
													x_state = (x_state < -sensitivity)?-1: ((x_state > sensitivity)?1:0);
													if (x_state != old_x_state) {
														if (old_x_state)
															do_uinput_fakekey(fd, old_x_state<0?0:1, 0);
														if (x_state)
															do_uinput_fakekey(fd, x_state<0?0:1, 1);
														old_x_state = x_state;
													}
													y_state = (y_state < -sensitivity)?-1: ((y_state > sensitivity)?1:0);
													if (y_state != old_y_state) {
														if (old_y_state)
															do_uinput_fakekey(fd, (old_y_state<0)?2:3, 0);
														if (y_state)
															do_uinput_fakekey(fd, (y_state<0)?2:3, 1);
														old_y_state = y_state;
													}
												}
												break;
											}
										}
										else
											printf("ERROR: reading ZEEMOTE_STICK payload failed\n");
									}
									else {
										printf("ERROR: unexpected length %d in ZEEMOTE_STICK\n", hdr.length);
										read_num(bt, data.dummy, hdr.length - 2);
									}
								}
								break;
								case ZEEMOTE_BATTERY: {
									if (hdr.length-2 == sizeof(data.voltage))
										if (read_num(bt, &data.voltage, sizeof(data.voltage)))
											printf("ZEEMOTE_BATTERY: %d.%03u volts\n", ztohs(data.voltage)/1000, ztohs(data.voltage)%1000);
										else
											printf("ERROR: reading ZEEMOTE_BATTERY payload failed\n");
									else {
										printf("ERROR: unexpected length %d in ZEEMOTE_BATTERY\n", hdr.length);
										read_num(bt, data.dummy, hdr.length - 2);
									}
								}
								break;
								case ZEEMOTE_BUTTONS: {
									if (hdr.length-2 == sizeof(data.buttons)) {
										if (read_num(bt, data.buttons, sizeof(data.buttons))) {
											int i;
											int button_state = 0;
											for (i=0;i<sizeof(data.buttons);i++)
												if (data.buttons[i] != ZEEMOTE_BUTTON_NONE)
													button_state |= 1<<data.buttons[i];
											for (i=0;i<4;i++)
												if ((button_state & (1<<i)) != (old_button_state & (1<<i)))
													switch (kbd_mode) {
														case mode_keyboard:
															do_uinput(fd, keys[4+i], (button_state & (1<<i))?1:0, EV_KEY);
														break;
														case mode_joystick:
															do_uinput(fd, BTN_0+i, (button_state & (1<<i))?1:0, EV_KEY);
														break;
														case mode_fakekey:
															do_uinput_fakekey(fd, 4+i, (button_state & (1<<i))?1:0);
														break;
													}
											old_button_state = button_state;
										}
										else
											printf("ERROR: reading ZEEMOTE_BUTTONS payload failed\n");
									}
									else {
										printf("ERROR: unexpected length %d in ZEEMOTE_BUTTONS\n", hdr.length);
										read_num(bt, data.dummy, hdr.length - 2);
									}
								break;
								}
								default: {
									if (hdr.length - 2 > sizeof(data.dummy))
										printf("%d bytes of unknown command %d exceeding limits\n", hdr.length-2, hdr.type);
									read_num(bt, data.dummy, hdr.length - 2);
								break;
								}
							}
						}
					}
					else
						if (rd < 0)
							break;
				}
			}
		}
		else
			printf("connection to %s failed\n", addr);
		bdaddr++;
	}
	int status = 0;
	wait(&status);
ERROR:
	printf("zeemoted terminated %d\n", status);
	return 0;
}
