#include "handleconn.h"

extern volatile sig_atomic_t exit_requested;

/* 
 * Returns 0 on success, -1 on failure, errno = EIO on short write
 */
static ssize_t write_all(int fd, const void *buf, size_t len) {
	if (fd < 0 || !buf) { errno = EINVAL; return -1; }
	if ((ssize_t)len > SSIZE_MAX) { errno = EINVAL; return -1; }

	const char *p = buf;
	ssize_t n = write(fd, p, len);
	if (n == (ssize_t)len) return 0; /* Success */

	/* Try again on EINTR */
	else if (n == -1 && errno == EINTR) {
		for(;;) {
			n = write(fd, p, len);
			if (n == -1) {
				if (errno == EINTR) continue;
				else return -1;
			}
			if (n == (ssize_t)len)
				return 0;
			if (n > 0 && n < (ssize_t)len)
				goto short_write;
		}
	} else if (n == -1) {
		return -1;
	} else if (n < (ssize_t)len) {
		goto short_write;
	}

short_write:
	errno = EIO;
	return -1;
}

static int send_all(int fd, char *buf, size_t len) {
	int rc;
	size_t bytes_written;
	if (fd == -1 || !buf)
		return -1;

	bytes_written = 0;
	while ((rc = write(fd, buf, WRITE_CHUNK_SZ)) != 0) {
		if (rc == -1) {
			if (errno == EINTR) continue;
			else return -1;
		}

		bytes_written += rc;
		if (bytes_written == len) break;
	}

	return 0;
}

static int send_file_to_client(int send_fd, const char *path) {
	int fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd == -1) return -1;

	char buf[8192];
	for (;;) {
		ssize_t n = read(fd, buf, sizeof buf);
		if (!n)	break; /* EOF */
		if (n < 0) {
			if (errno == EINTR) continue;
			close(fd);
			return -1;
		}

		if (send_all(send_fd, buf, (size_t)n) == -1) {
			close(fd);
			return -1;
		}
	}

	close(fd);
	return 0;
}

int handle_connection(int fd, int write_fd, StringBuilder *sb, 
		char *scratch, hc_result_t *res) {
	int rc;
	char recv_buf[RECV_BUF_SZ];
	ssize_t bytes_received;
	sb->len = 0; // Reset the pending buf per conn. Consider realloc
	bool discard = false;
	while (1) {
		bytes_received = recv(fd, recv_buf, RECV_BUF_SZ, 0);
		if (bytes_received == -1) {
			if (errno == EINTR) continue;
			res->outcome = HC_OUTCOME_ERROR;
			res->op = HC_OP_RECV;
			res->sys_errno = errno;
			return EXIT_ERROR;
		} else if ((bytes_received == 0) || exit_requested) {
			/* Connection closed by peer/signal */
			sb->len = 0;
			res->outcome = HC_OUTCOME_CLOSED;
			return 0;
		} 

		size_t bytes_len = (size_t)bytes_received;

		char *pos = recv_buf;
		char *end = recv_buf + bytes_len;
		size_t remaining = bytes_len;

		/*
		 * Three valid states:
		 * 1: Discard mode - oversized packet.
		 * 2: Normal mode - newline found.
		 * 3: Normal mode - newline not found.
		 */
		while (remaining > 0) {
			/* Check for newline in buffer */
			char *nl = memchr(pos, '\n', remaining);

			/* Discard mode */
			if (discard) {
				if (!nl) {
					/* 
					 * Still no new line,
					 * keep discarding
					 */
					pos = end;
					remaining = 0;
					continue;
				} else {
					pos = nl + 1;
					remaining = end - pos;
					discard = false;
					continue;
				}
			}

			/* Normal mode - newline found */
			if (nl) {
				size_t seg_len = (nl - pos) + 1;

				/* Avoid overflow */
				if (sb->len > MAX_PACKET - seg_len) {
					discard = false;
					sb->len = 0;
					pos += seg_len;
					remaining = (size_t)(end - pos);
					continue;
				}

				size_t packet_len = sb->len + seg_len;
				memcpy(scratch, sb->str, sb->len);
				memcpy(scratch+sb->len, pos, seg_len);
				rc = write_all(write_fd, scratch, 
						packet_len);
				if (rc == -1) {
					if (errno == EIO) {
						res->outcome = HC_OUTCOME_ERROR;
						res->op = HC_OP_APPEND;
						res->err = HC_ERR_SHORT_WRITE;
						res->sys_errno = errno;
						res->intended = packet_len;
						return EXIT_ERROR;
					} else {
						res->outcome = HC_OUTCOME_ERROR;
						res->op = HC_OP_APPEND;
						res->err = HC_ERR_IO;
						res->sys_errno = errno;
						res->intended = packet_len;
						return EXIT_ERROR;
					} 
				}

				sb->len = 0;
				pos += seg_len;
				remaining = (size_t)(end - pos);

				if (send_file_to_client(fd, 
							AESD_DATA_PATH) == -1) {
					res->outcome = HC_OUTCOME_ERROR;
					res->op = HC_OP_SEND;
					res->err = HC_ERR_IO;
					res->sys_errno = errno;
					return EXIT_ERROR;
				}

			/* Normal mode - newline not found */
			} else {
				size_t chunk_len = remaining;

				if (sb->len > MAX_PACKET - chunk_len) {
					discard = true;
					sb->len = 0;
					pos = end;
					remaining = 0;
					continue;
				}

				/* No newline, stash in pending */
				rc = sb_reserve(sb, sb->len + chunk_len, 
						MAX_PACKET - 1);
				if (rc == -1) {
					if (errno == EOVERFLOW) {
						discard = true;
						sb->len = 0;
						pos = end;
						remaining = 0;
						continue;
					} else {
						res->outcome = HC_OUTCOME_ERROR;
						res->op = HC_OP_NONE;
						res->err = HC_ERR_ALLOC;
						res->sys_errno = errno;
						return EXIT_ERROR;
					}
				}

				memcpy(sb->str + sb->len, pos, chunk_len);
				pos = end;
				remaining = 0;
				sb->len += chunk_len;
			}
		}
	}

	return 0;
}

