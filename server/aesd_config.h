#ifndef __AESD_CONFIG_H__
#define __AESD_CONFIG_H__

#ifndef BACKLOG
#define BACKLOG 20
#endif

#ifndef EXIT_ERROR
#define EXIT_ERROR -1
#endif

#ifndef MAX_PACKET
#define MAX_PACKET (1<<20)
#endif

#ifndef RECV_BUF_SZ
#define RECV_BUF_SZ 4096
#endif

#ifndef WRITE_CHUNK_SZ
#define WRITE_CHUNK_SZ 1024
#endif

#ifndef AESD_DATA_PATH
#define AESD_DATA_PATH "/var/tmp/aesdsocketdata"
#endif

#endif
