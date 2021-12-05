#include <csp/csp.h>
#include <unistd.h>
#include <slash/slash.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>

static int fd;

void * it69_task(void * param) {

    while(1) {

        /* Each second */
        sleep(1);

        /* Send requrest for data */
        static const char * req_cmd = "GET ALL";
        printf("Writing %s size %ld\n", req_cmd, strlen(req_cmd));
        if (write(fd, req_cmd, strlen(req_cmd) + 1) < 0) {
            printf("it69 write error\n");
            continue;
        }

        static uint8_t buf[100];
        int len = read(fd, buf, 100);

        if (len <= 0) {
            printf("Waiting for response from it69...\n");
            strerror(errno);
        } else {
            csp_hex_dump("recevied", buf, len);
        }
      

    }

}

void it69_init(char * device, int brate) {


    /* Initialize serial port */
    fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (fd < 0) {
		printf("%s: failed to open device: [%s], errno: %s\n", __FUNCTION__, device, strerror(errno));
		return;
	}

#if 0
	struct termios options;
	tcgetattr(fd, &options);
	cfsetispeed(&options, brate);
	cfsetospeed(&options, brate);
	options.c_cflag |= (CLOCAL | CREAD);
	options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8;
	options.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);
	options.c_iflag &= ~(IGNBRK | BRKINT | ICRNL | INLCR | PARMRK | INPCK | ISTRIP | IXON);
	options.c_oflag &= ~(OCRNL | ONLCR | ONLRET | ONOCR | OFILL | OPOST);
	options.c_cc[VTIME] = 0;
	options.c_cc[VMIN] = 1;
	/* tcsetattr() succeeds if just one attribute was changed, should read back attributes and check all has been changed */
	if (tcsetattr(fd, TCSANOW, &options) != 0) {
		printf("%s: Failed to set attributes on device: [%s], errno: %s\n", __FUNCTION__, device, strerror(errno));
		close(fd);
		return;
	}
	fcntl(fd, F_SETFL, 0);
#endif

	/* Flush old transmissions */
	if (tcflush(fd, TCIOFLUSH) != 0) {
		printf("%s: Error flushing device: [%s], errno: %s\n", __FUNCTION__, device, strerror(errno));
		close(fd);
		return;
	}

	/* Start server thread */
    static pthread_t server_handle;
	pthread_create(&server_handle, NULL, &it69_task, NULL);

}


static int it69_init_cmd(struct slash *slash) {

    char * device = "/dev/ttyUSB0";
    if (slash->argc > 1) {
        device = slash->argv[1];
    }

    int baud = 115200;
    if (slash->argc > 2) {
        baud = atoi(slash->argv[2]);
    }

    printf("Init it69 on %s at %d baud\n", device, baud);
	it69_init(device, baud);
	return SLASH_SUCCESS;
}

slash_command(it69, it69_init_cmd, "[device] [baud]", "Start it69 monitor");