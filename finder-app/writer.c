#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>

int main(int argc, char** argv)
{
	openlog(NULL, LOG_PID, LOG_USER);

	if (argc != 3) {
		syslog(LOG_ERR, "Invalid number of arguments: %d", argc);
		closelog();
		return 1;
	}

	const char* filename = argv[1];
	const char* content = argv[2];

	int fd = creat(filename, 0664);
	if (fd == -1) {
		syslog(LOG_ERR, "Could not create file named: %s.", filename);
		closelog();
		return 1;
	}

	size_t count = strlen(content);
	ssize_t nr = write(fd, content, count);

	if (nr == -1) {
		syslog(LOG_ERR, "Could not write to file. Error: %m.");
		close(fd);
		closelog();
		return 1;
	}

	if (close(fd) == -1) {
		syslog(LOG_ERR, "Could not close file. Error: %m.");
		closelog();
		return 1;
	}

	syslog(LOG_DEBUG, "Writing %s to %s.", content, filename);

	closelog();

	return 0;
}
