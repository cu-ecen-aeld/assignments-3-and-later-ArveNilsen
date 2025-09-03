#ifndef __AESDSOCKET_H__
#define __AESDSOCKET_H__

#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <syslog.h>
#include <fcntl.h>
#include <stdint.h>

#include "aesd_config.h"
#include "sb.h"
#include "handleconn.h"

typedef struct {
	/* config */
	char *port;
	const char *data_path;
	bool daemonize;

	/* long-lived resourced */
	int listen_fd;
	int append_fd;
	char *scratch;
	StringBuilder sb;

	/* state */
	volatile sig_atomic_t *exit_flag;
} ServerContext;


#endif
