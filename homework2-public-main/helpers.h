#include <stdio.h>
#include <stdlib.h>

/*
 * Macro de verificare a erorilor
 * Exemplu:
 *     int fd = open(file_name, O_RDONLY);
 *     DIE(fd == -1, "open failed");
 */

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

#define MAX_WAITING_CLIENTS SOMAXCONN
#define TOPIC_MAX_LEN 50
#define CLIENT_MAX_COMMAND_LEN 20
#define ID_MAX_LEN 10
#define BUFFER_LEN 250
#define PAYLOAD_LEN 1500
#define IP_MAX_LEN 16