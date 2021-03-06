#include <X11/Xlib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>

#define MAX_DISPLAY 1024

void arg_error() {
	fprintf(stderr,"Options: [-display DISPLAY] [-delay N] [-maxtry N | -wait] [-v]\n");
	exit(2);
}

void print_display(const char *display_name) {
	if (!display_name) printf("default ");
	printf("display");
	if (display_name) printf("=%s", display_name);
}

static int open_display(const char *display_name, int verbose) {
	int opened = 0;
	Display *disp = XOpenDisplay(display_name);
	if (disp) {
		opened = 1;
		if (verbose) {
			printf("Opened ");
			print_display(display_name);
			printf("\n");
		}
		XCloseDisplay(disp);
	}
	return opened;
}

int main(int argc, char **argv) {
	char *arg;
	int i, tries = 0;

	char *display_name = NULL;
	int delay = 1, max_try = 3, verbose = 0;

	for (i = 1; i < argc; i++) {
		arg = argv[i];
		if (!strcmp(arg,"-display")) {
			if (i+1 >= argc) arg_error();
			display_name = (char *)malloc(MAX_DISPLAY * sizeof(char));
			strncpy(display_name, argv[++i], MAX_DISPLAY);
			display_name[MAX_DISPLAY-1] = '\0';
		} else if (!strcmp(arg,"-delay")) {
			if (i+1 >= argc) arg_error();
			delay = atoi(argv[++i]);
		} else if (!strcmp(arg,"-maxtry")) {
			if (i+1 >= argc) arg_error();
			max_try = atoi(argv[++i]);
		} else if (!strcmp(arg,"-wait")) {
			max_try = 0;
		} else if (!strcmp(arg,"-v")) {
			verbose++;
		} else arg_error();
	}

	while (1) {
		tries++;
		if (open_display(display_name, verbose))
			break;

		if (max_try && tries >= max_try) {
			if (verbose) {
				printf("Exceeded -maxtry %d while opening ", max_try);
				print_display(display_name);
				printf("\n");
			}
			exit(1);
		}
		sleep(delay);
	}

	return 0;
}
