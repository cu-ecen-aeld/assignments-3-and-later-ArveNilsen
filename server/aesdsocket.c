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
#include <syslog.h>
#include <fcntl.h>
#include <stdint.h>

#include "aesd_config.h"
#include "sb.h"
#include "handleconn.h"

volatile sig_atomic_t exit_requested = 0;

void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

static void sigaction_handler(int signum, siginfo_t *info, void *context) {
	(void)info;
	(void)context;
	switch (signum) {
		case SIGCHLD:
			int saved_errno = errno;
			while (waitpid(-1, NULL, WNOHANG) > 0) {}
			errno = saved_errno;
			break;
		case SIGINT:
			exit_requested = signum;
			break;
		case SIGTERM:
			exit_requested = signum;
			break;
		default:
			break;
	}
}

int main(void) {
	openlog(NULL, LOG_PID, LOG_USER);

	/* listen on sock_fd, new connection on new_fd */
	int sock_fd, new_fd;

	int tmpfile_fd;

	/* 
	 * hints - for setting up the socket.
	 * servinfo - the connection
	 * p - temp for looping
	 */
	struct addrinfo hints, *servinfo, *p;

	struct sockaddr_storage their_addr; /* Address of the connector */
	socklen_t sin_size; /* Size of the connector */
	struct sigaction sa;

	int yes=1;

	/* return code for error checking */
	int rc;

	bool exit_error = false;

	/* Zero out the hints struct TODO: (would = {0} do?) */
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET; /* IPv4 */
	hints.ai_socktype = SOCK_STREAM; /* TCP */
	hints.ai_flags = AI_PASSIVE; /* Use current IP */

	if ((rc = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		syslog(LOG_ERR, "getaddrinfo: %s\n", gai_strerror(rc));
		return EXIT_ERROR;
	}

	/* 
	 * servinfo is now a linked list of results.
	 * Loop and bind to the first available.
	 */
	for (p = servinfo; p!= NULL; p = p->ai_next) {
		if ((sock_fd = socket(p->ai_family, p->ai_socktype, 
						p->ai_protocol)) == -1) {
			syslog(LOG_INFO, "server: socket");
			continue;
		}
		
		if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			syslog(LOG_ERR, "setsockopt");
			return EXIT_ERROR;
		}

		if (bind(sock_fd, p->ai_addr, p->ai_addrlen) == -1) {
			shutdown(sock_fd, SHUT_RDWR);
			close(sock_fd);
			syslog(LOG_ERR, "server: bind");
			continue;
		}

		break;
	}

	if (!p) {
		syslog(LOG_ERR, "server: failed to bind\n");
		shutdown(sock_fd, SHUT_RDWR);
		close(sock_fd);
		return EXIT_ERROR;
	}

	freeaddrinfo(servinfo); /* all done with this */

	if (listen(sock_fd, BACKLOG) == -1) {
		syslog(LOG_ERR, "listen\n");
		shutdown(sock_fd, SHUT_RDWR);
		close(sock_fd);
		return EXIT_ERROR;
	}

	sa.sa_sigaction = sigaction_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO;
	if ((sigaction(SIGINT, &sa, NULL) == -1)  ||
	   (sigaction(SIGTERM, &sa, NULL) == -1)) {
		syslog(LOG_ERR, "sigaction\n");
		shutdown(sock_fd, SHUT_RDWR);
		close(sock_fd);
		return EXIT_ERROR;
	}

	syslog(LOG_INFO, "server: waiting for connections...\n");

	/* scratch buffer for recv loop */
	char *scratch = malloc(MAX_PACKET);
	if (!scratch) {
		syslog(LOG_ERR, "malloc failed\n");
		shutdown(sock_fd, SHUT_RDWR);
		close(sock_fd);
		return EXIT_ERROR;
	}

	/* buffer for pending writes */
	StringBuilder *sb = malloc(sizeof *sb);
	if (!sb) {
		syslog(LOG_ERR, "malloc failed\n");
		shutdown(sock_fd, SHUT_RDWR);
		close(sock_fd);
		free(scratch);
		return EXIT_ERROR;
	}
	rc = sb_init(sb, 4096, MAX_PACKET);
	if (rc == -1) {
		syslog(LOG_ERR, "malloc failed\n");
		free(sb);
		shutdown(sock_fd, SHUT_RDWR);
		close(sock_fd);
		free(scratch);
		return EXIT_ERROR;
	}

	/* tmpfile for writing received bytes */
	if ((tmpfile_fd = open(AESD_DATA_PATH, 
			O_WRONLY | O_APPEND | 
			O_CREAT | O_CLOEXEC, 0644)) == -1) {
		syslog(LOG_ERR, "open failed\n");
		shutdown(sock_fd, SHUT_RDWR);
		close(sock_fd);
		free(scratch);
		sb_free(sb);
		free(sb);
		return EXIT_ERROR;
	}

	for(;;) {
		char peer_ip[INET6_ADDRSTRLEN];

		/* Exit per signal handler */
		if (exit_requested) {
			syslog(LOG_INFO, "Closed conection from %s\n", 
					peer_ip);
			syslog(LOG_INFO, "Caught signal, exiting");
			break;
		}

		sin_size = sizeof their_addr;
		new_fd = accept(sock_fd, (struct sockaddr *)&their_addr,
				&sin_size);
		if (new_fd == -1) {
			if (errno == EINTR) continue;
			else if (exit_requested) break;
			else {
				syslog(LOG_ERR, "accept failed\n");
				shutdown(sock_fd, SHUT_RDWR);
				close(sock_fd);
				free(scratch);
				sb_free(sb);
				free(sb);
				return EXIT_ERROR;
			}
		}

		inet_ntop(their_addr.ss_family,
				get_in_addr((struct sockaddr *)&their_addr),
				peer_ip, sizeof peer_ip);
		syslog(LOG_INFO, "Accepted conection from %s\n", peer_ip);

		hc_result_t res;
		handle_connection(new_fd, tmpfile_fd, sb, scratch, &res);
		if (res.outcome == HC_OUTCOME_ERROR) {
			switch (res.op) {
			case HC_OP_RECV:
				syslog(LOG_ERR, "recv failed from %s: %s", peer_ip, strerror(res.sys_errno));
				break;
			case HC_OP_APPEND:
				if (res.err == HC_ERR_SHORT_WRITE)
					syslog(LOG_ERR, "short write appending %zu bytes for %s", res.intended, peer_ip);
				else
					syslog(LOG_ERR, "append failed for %s: %s (intended %zu)", peer_ip, strerror(res.sys_errno), res.intended);
				break;
			case HC_OP_SEND:
				syslog(LOG_ERR, "send failed to %s: %s",
					peer_ip, strerror(res.sys_errno));
				break;
			default:
				syslog(LOG_ERR, "connection error with %s: %s", peer_ip, strerror(res.sys_errno));
			}

			exit_error = true;
		}
		
		syslog(LOG_INFO, "Closed connection from %s", peer_ip);
	}

	close(tmpfile_fd);
	shutdown(sock_fd, SHUT_RDWR);
	close(sock_fd);
	shutdown(new_fd, SHUT_RDWR);
	close(new_fd);
	free(scratch);
	sb_free(sb);
	free(sb);

	return (exit_error ? -1 : 0);
}
