#ifndef __HANDLECONN_H__
#define __HANDLECONN_H__

#include <stdint.h>  /* unint64_t */
#include <stdbool.h> /* bool */
#include <unistd.h>  /* write */
#include <limits.h>  /* SSIZE_MAX */
#include <sys/socket.h> /* recv */
#include <string.h>  /* memchr */
#include <fcntl.h>   /* open */
#include <signal.h>  /* sig_atomic_t */

#include "aesd_config.h"
#include "sb.h" /* StringBuilder */

typedef enum {
	HC_OUTCOME_CLOSED = 0, /* peer closed normally */
	HC_OUTCOME_ERROR,      /* terminated due to error */
} hc_outcome_t;

typedef enum {
	HC_OP_NONE = 0,
	HC_OP_RECV,
	HC_OP_APPEND, /* write all */
	HC_OP_SEND,   /* send_file_to_client */
} hc_op_t;

typedef enum {
	HC_ERR_NONE = 0,
	HC_ERR_EINTR_AGAIN,
	HC_ERR_SHORT_WRITE, /* write_all returned -1 with EIO */
	HC_ERR_IO, 	    /* I/O failure (write/read/send) */
	HC_ERR_ALLOC,	    /* ENOMEM from buffer builder */
} hc_err_t;

typedef struct {
	hc_outcome_t outcome;	/* CLOSED OR ERROR */
	hc_op_t op;
	hc_err_t err;
	int sys_errno;		/* snapshot of errno */
	size_t intended;	/* intended bytes count for the op */
	size_t transferred;	/* bytes actual */
	uint64_t packets_written;
	uint64_t packets_dropped_oversize;
} hc_result_t;

int handle_connection(int fd, int write_fd, StringBuilder *sb, 
		char *scratch, hc_result_t *res);

#endif
