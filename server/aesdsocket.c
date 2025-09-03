#include "aesdsocket.h"

static void print_usage(void) {
	fprintf(stderr, "Usage: no arg || -d || -p <PORT>\n"
			"Port must be 4 digits!");
}

static int parse_args(bool *daemonize, char **port, int argc, char **argv) {
	if (argc > 4) {
		print_usage();
		return -1;
	} 

	switch (argc) {
	case 1:
		/* run default port, no daemon */
		return 0;
	case 2:
		/* Must be -d or invalid */
		if (!memcmp(argv[1], "-d", 2)) {
			*daemonize = true;
			return 0;
		} else {
			print_usage();
			return -1;
		}
	case 3:
		/* Must be -p <PORT> */
		if (!memcmp(argv[1], "-p", 2)) {
			if ((strlen(argv[2]) == 4)) {
				*port = argv[2];
			}
		}
		break;
	case 4:
		/* -d -p <PORT> or invalid */
		if ((!memcmp(argv[1], "-d", 2) &&
		   (!memcmp(argv[2], "-p", 2)) &&
		   ((strlen(argv[3]) == 4)))) {
			*daemonize = true;
			*port = argv[3];
			return 0;
		} else {
			print_usage();
			return -1;
		}
	}
		   
	return 0;
}

static int setup_daemon(int fd) {
	pid_t pid = fork();
	if (pid < 0) { /* error */ return EXIT_ERROR; }
	if (pid > 0) { /* this is the parent process */
		close(fd);
		_exit(EXIT_SUCCESS);
	}

	if ((setsid()) == -1) {
		return EXIT_ERROR;
	}

	pid_t pid2 = fork();
	if (pid2 < 0) { /* error */ return EXIT_ERROR; }
	if (pid2 > 0) { _exit(EXIT_SUCCESS); }

	umask(0);
	if (chdir("/") == -1) _exit(EXIT_ERROR);
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	int fd_null = open("/dev/null", O_RDWR);
	if (fd_null == -1) {
		_exit(EXIT_ERROR);
	}

	if (dup2(fd_null, STDIN_FILENO) == -1) {
		_exit(EXIT_ERROR);
	}

	if (dup2(fd_null, STDOUT_FILENO) == -1) {
		_exit(EXIT_ERROR);
	}

	if (dup2(fd_null, STDERR_FILENO) == -1) {
		_exit(EXIT_ERROR);
	}

	if (fd_null > 2) { close(fd_null); }

	return 0;
}

int daemonize_after_listen(int listen_fd, bool daemonize) {
	if (!daemonize) return 0;

	if ((setup_daemon(listen_fd)) == -1) {
		return EXIT_ERROR;
	}
	return 0;
}

volatile sig_atomic_t exit_requested = 0;

void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

static int create_listen_socket(ServerContext *ctx) {
	int rc;

	/* 
	 * hints - for setting up the socket.
	 * servinfo - the connection
	 * p - temp for looping
	 */
	struct addrinfo hints, *servinfo, *p;
	int yes=1;

	hints = (struct addrinfo){0};
	hints.ai_family = AF_INET; /* IPv4 */
	hints.ai_socktype = SOCK_STREAM | SOCK_CLOEXEC; /* TCP */
	hints.ai_flags = AI_PASSIVE; /* Use current IP */

	if ((rc = getaddrinfo(NULL, ctx->port, &hints, &servinfo)) != 0) {
		return EXIT_ERROR;
	}

	/* 
	 * servinfo is now a linked list of results.
	 * Loop and bind to the first available.
	 */
	for (p = servinfo; p!= NULL; p = p->ai_next) {
		if ((ctx->listen_fd = socket(p->ai_family, p->ai_socktype, 
						p->ai_protocol)) == -1) {
			continue;
		}
		
		if (setsockopt(ctx->listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			freeaddrinfo(servinfo); 
			close(ctx->listen_fd);
			return EXIT_ERROR;
		}

		if (bind(ctx->listen_fd, p->ai_addr, p->ai_addrlen) == -1) {
			continue;
		}

		break;
	}

	if (!p) {
		freeaddrinfo(servinfo); 
		close(ctx->listen_fd);
		return EXIT_ERROR;
	}

	freeaddrinfo(servinfo); 

	if (listen(ctx->listen_fd, BACKLOG) == -1) {
		close(ctx->listen_fd);
		return EXIT_ERROR;
	}

	return 0;
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

static int install_sigaction(void) {
	struct sigaction sa;
	sa.sa_sigaction = sigaction_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO;

	/* SIGINT and SIGTERM with SA_SIGINFO */
	if ((sigaction(SIGINT, &sa, NULL) == -1)  ||
	    (sigaction(SIGTERM, &sa, NULL) == -1)) {
		return EXIT_ERROR;
	}

	/* SIGCHLD with SA_RESTART in addition */
	sa.sa_flags |= SA_RESTART;
	if ((sigaction(SIGCHLD, &sa, NULL) == -1)) {
		return EXIT_ERROR;
	}

	return 0;
}

static int open_append(ServerContext *ctx) {
	if ((ctx->append_fd = open(AESD_DATA_PATH, 
			O_WRONLY | O_APPEND | 
			O_CREAT | O_CLOEXEC, 0644)) == -1) {
		return EXIT_ERROR;
	}

	return 0;
}

static int alloc_runtime_buffers(ServerContext *ctx) {

	/* scratch buffer for recv loop */
	char *scratch = malloc(MAX_PACKET);
	if (!scratch) {
		return EXIT_ERROR;
	}

	if (sb_init(&ctx->sb, 4096, MAX_PACKET) == -1) {
		free(scratch);
		return EXIT_ERROR;
	}

	return 0;
}

static int run_accept_loop(ServerContext *ctx) {
	for(;;) {
		int new_fd;
		char peer_ip[INET6_ADDRSTRLEN];
		/* Address of the connector */
		struct sockaddr_storage their_addr; 
		socklen_t sin_size; /* Size of the connector */

		/* Exit per signal handler */
		if (exit_requested) {
			break;
		}

		sin_size = sizeof their_addr;
		new_fd = accept(ctx->listen_fd, 
				(struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			if (errno == EINTR) continue;
			else if (exit_requested) break;
			else {
				syslog(LOG_ERR, "accept failed\n");
				return EXIT_ERROR;
			}
		}

		inet_ntop(their_addr.ss_family,
				get_in_addr((struct sockaddr *)&their_addr),
				peer_ip, sizeof peer_ip);
		syslog(LOG_INFO, "Accepted conection from %s\n", peer_ip);

		hc_result_t res;
		handle_connection(new_fd, ctx->append_fd, &ctx->sb, ctx->scratch, 
				&res);
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
		}
		
		syslog(LOG_INFO, "Closed connection from %s", peer_ip);
	}
	return 0;
}

void ctx_init(ServerContext *ctx) {
	ctx->port = NULL;
	ctx->data_path = AESD_DATA_PATH;
	ctx->daemonize = false;
	ctx->listen_fd = -1;
	ctx->append_fd = -1;
	ctx->scratch = NULL;
	ctx->exit_flag = &exit_requested;
}

int main(int argc, char** argv) {
	int rc = EXIT_FAILURE;
	bool unlink_on_exit = false;

	ServerContext ctx;
	ctx_init(&ctx);

	if ((parse_args(&ctx.daemonize, &ctx.port, argc, argv)) == -1)
		goto cleanup;

	if (create_listen_socket(&ctx) == -1)
		goto cleanup;

	if (daemonize_after_listen(ctx.listen_fd, ctx.daemonize) == -1)
		goto cleanup;

	if ((install_sigaction()) == -1)
		goto cleanup;

	openlog("aesdsocket", LOG_PID, LOG_USER);
	syslog(LOG_INFO, "server: waiting for connections...\n");

	if (open_append(&ctx) == -1) 
		goto cleanup;

	if (alloc_runtime_buffers(&ctx) == -1)
		goto cleanup;

	if (run_accept_loop(&ctx) == -1) {
		unlink_on_exit = false;
		goto cleanup;
	}

	/* Graceful shutdown via signal */
	unlink_on_exit = true;

	rc = EXIT_SUCCESS;

cleanup:
	if (ctx.listen_fd != -1) {
		close(ctx.listen_fd);
		ctx.listen_fd = -1;
	}

	if (ctx.append_fd != -1) {
		close(ctx.append_fd);
		ctx.append_fd = -1;
	}

	if (ctx.scratch) { free(ctx.scratch); ctx.scratch = NULL; }
	sb_free(&ctx.sb);

	if (unlink_on_exit && ctx.data_path) {
		unlink(ctx.data_path);
	}

	closelog();
	return rc;
}
