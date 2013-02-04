#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <linux/input.h>


#define HID_DEVICE "/dev/event1"
#define MOUSE_DEVICE "/dev/mouse0"

static int readKey();
static int readMouse();
static int tty_fd = -1;
static struct termios save_tty;
static int mouse_fd = -1;

int main() {
	int key;
	char keyc;
	struct termios tty;
	char name[256]="Unknown";
	tty_fd = open(HID_DEVICE, O_RDONLY);
	if (tty_fd == -1) {
		printf("failed to open device\n");
		return -1;
	}
	ioctl(tty_fd, EVIOCGNAME (sizeof(name)), name);
	printf("Reading from : %s(%s)\n", HID_DEVICE, name);

#if 0
	mouse_fd = open(MOUSE_DEVICE, O_RDONLY);
	if (mouse_fd == -1) {
		printf("failed to open mouse device\n");
		return -1;
	}
	ioctl(mouse_fd, EVIOCGNAME (sizeof(name)), name);
	printf("Reading from : %s(%s)\n", MOUSE_DEVICE, name);
#endif
	/*
	if (tcgetattr(tty_fd, &save_tty) == -1) {
		printf("failed to get attr of tty\n");
		return -1;
	}
	tty = save_tty;

	tty.c_lflag &= ~(ECHO | ICANON);
	tty.c_cc[VMIN] = 0;
	tty.c_cc[VTIME] = 0;
	if (tcsetattr(tty_fd, TCSAFLUSH, &tty) == -1) {
		printf("----------\n");
		return -1;
	}
	*/

	while (1) {
		key = readKey();

		/*
		if (key <= 0) {
			usleep(10);
			continue;
		}
		*/
		usleep(100);
		//readMouse();
	}
	return 0;
}

static int readMouse() {
	struct input_event ev[64];
	int size = sizeof(struct input_event);
	int i;
	read(mouse_fd, ev, size*64);

	for (i = 0; i < 2; i++) {
	printf("type:%d, code:%d, value:%d\n", ev[i].type, ev[i].code, ev[i].value);	
	}
}

static int readKey() {
	struct input_event ev[64];
	unsigned char key;
	ssize_t count;
	int value;
	int i;
	printf("try to read\n");
	int size = sizeof(struct input_event);
	count = read(tty_fd, ev, size*64);
	value = ev[0].value;
	//only read the key pressed event
	/*
	if (value != ' ' && ev[1].value == 1 && ev[1].type == 1) { 
		printf("%d, %d\n", ev[0].value, ev[1].code);	
		printf("%d, %c\n", ev[0].value - 458659, ev[0].value - 458659);	
	}
	*/
	for (i = 0; i < 2; i++) {
	printf("type:%d, code:%d, value:%d\n", ev[i].type, ev[i].code, ev[i].value);	
	}

	return (count < size) ? key : 0;
}

