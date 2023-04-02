#include "helpers.h"

typedef struct message_from_tcp_client_to_server {
    char c[CLIENT_MAX_COMMAND_LEN];
    char topic[TOPIC_MAX_LEN];
    char id[ID_MAX_LEN];
    unsigned char SF;
} from_tcp_client;

typedef struct message_from_udp_client_to_server {
	char topic[TOPIC_MAX_LEN];			
	unsigned char type;		
	char payload[PAYLOAD_LEN];
} from_udp_client;

typedef struct message_from_server_to_tcp_client {
    char udp_client_ip[IP_MAX_LEN];			
	int port;						
	from_udp_client m;
} to_tcp_client;

typedef struct topic_client {
	unsigned char SF;
	char title[TOPIC_MAX_LEN];
	int subscribed;
} topic;

typedef struct tcp_client {
	int socket;				
	char id[ID_MAX_LEN];		
	topic *topics;
    int topics_array_phys_dim;			 
	int nr_topics;  /*
                        Initializiat cu 1. Cand se pune pe pozitia 0 primul, devine 2.
                        Indica mereu catre primul index de dupa finalul vectorului de topicuri.
                    */				
	int connected;
    int nr_stored_messages;
    to_tcp_client *stored_messages_array;
    int stored_messages_array_phys_dim;	
} client;