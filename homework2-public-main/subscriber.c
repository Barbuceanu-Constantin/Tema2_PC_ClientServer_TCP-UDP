#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <math.h>
#include "helpers.h"
#include "app_protocol.h"

void check_command_line_arguments(int argc, char *argv[]) {
    DIE(argc != 4, "client must have precisely 3 command line arguments.\n");
}

void initialize_socket_sets(fd_set *read, fd_set *tmp) {
    FD_ZERO(read);
    FD_ZERO(tmp);
}

void connect_to_server(int *sock, fd_set *read, int *fdmax,
                    char *argv[], struct sockaddr_in *serv) {
    int ret;
    
    //Socket TCP pe care trimit mesaje la server.
    *sock = socket(AF_INET, SOCK_STREAM, 0);
	DIE(*sock == -1, "[subscriber] Error at creating socket.\n");

    int port = atoi(argv[3]);
    DIE(port <= 0, "[subscriber] Error at converting the port from string to int (atoi).\n");

    //Completare structura server.
    memset((char*) serv, 0, sizeof(struct sockaddr_in));
	serv -> sin_family = AF_INET;
	serv -> sin_port = htons(port);
	ret = inet_aton(argv[2], &(serv -> sin_addr));
	DIE(ret < 0, "[subscriber] inet_aton.\n");

    //Conectare la server.
    ret = connect(*sock, (struct sockaddr*) serv, sizeof(struct sockaddr_in));
	DIE(ret < 0, "[subscriber] connect_client\n");

    //Introduc socketii de input in multimea de socketi.
    FD_SET(*sock, read);
	FD_SET(0, read);

    //Setez fdmax.
    *fdmax = *sock;

    //Se trimite id-ul clientului la server.
    from_tcp_client m;
    memset((char*)&m, 0, sizeof(from_tcp_client));
    memcpy(m.id, argv[1], strlen(argv[1]) + 1);
    ret = send(*sock, &m, sizeof(from_tcp_client), 0);
	DIE(ret == -1, "[subscriber] Error at sending the client id.\n");
}

void close_client_socket(int sock) {
    close(sock);
}

void check_for_error(char *c, char *type1) {
    char type2[CLIENT_MAX_COMMAND_LEN];
    char topic[TOPIC_MAX_LEN];
    unsigned char SF = 0xff;
    
    if(strcmp(type1, "subscribe") == 0) {
        sscanf(c, "%s %s %hhu", type2, topic, &SF);
        DIE(strcmp(topic, "") == 0, "Subscribe operation must have a topic.\n");
        DIE((SF != 0 && SF != 1), "SF must be either 0 or 1.\n");
    } else if(strcmp(type1, "unsubscribe") == 0) {
        sscanf(c, "%s %s", type2, topic);
        DIE(strcmp(topic, "") == 0, "Unsubscribe operation must have a topic.\n");
    }
}

void subscribe(char *command, int sock) {
    char type[CLIENT_MAX_COMMAND_LEN];
    char topic[TOPIC_MAX_LEN];
    unsigned char SF = 0xff;
    int ret;

    //Verific cazurile de eroare in comanda.
    check_for_error(command, "subscribe");

    //Parsez comanda.
    sscanf(command, "%s %s %hhu", type, topic, &SF);

    //Trimit mesajul de abonare la topic.
    from_tcp_client m;
    memcpy(m.c, type, CLIENT_MAX_COMMAND_LEN);
	memcpy(m.topic, topic, TOPIC_MAX_LEN);
	m.SF = SF;

	ret = send(sock, &m, sizeof(from_tcp_client), 0);
	DIE(ret == -1, "[subscriber] Error at sending subscribe tcp message.\n");

    //Afisez la stdout mesajul de abonare.
    printf("Subscribed to topic.\n");
}

void unsubscribe(char *command, int sock) {
    char type[CLIENT_MAX_COMMAND_LEN];
    char topic[TOPIC_MAX_LEN];
    int ret;

    //Verific cazurile de eroare in comanda.
    check_for_error(command, "unsubscribe");

    //Parsez comanda.
    sscanf(command, "%s %s", type, topic);

    //Trimit mesajul de dezabonare la topic.
    from_tcp_client m;
    memset(&m, 0, sizeof(m));

    //Scriu unsubscribe in campul c si topicul in campul "topic".
    memcpy(m.c, type, CLIENT_MAX_COMMAND_LEN);
    memcpy(m.topic, topic, TOPIC_MAX_LEN);

    ret = send(sock, &m, sizeof(from_tcp_client), 0);
	DIE(ret == -1, "[subscriber] Error at sending subscribe tcp message.\n");

    //Afisez la stdout mesajul de dezabonare.
    printf("Unsubscribed from topic.\n");
}

int stdin_command(int sock) {
    char string[BUFFER_LEN];
    char type[CLIENT_MAX_COMMAND_LEN];

    //Setez pe 0 bufferul comenzii.
    memset(string, 0, BUFFER_LEN);

    //Citesc de la stdin comanda.
	fgets(string, BUFFER_LEN - 1, stdin);

    //Extrag doar tipul comenzii. 
    sscanf(string, "%s ", type);

    if(strcmp(type, "exit") == 0) {
        return 0;
    } else if(strcmp(type, "subscribe") == 0) {
        subscribe(string, sock);
        return 1;
    } else if(strcmp(type, "unsubscribe") == 0) {
        unsubscribe(string, sock);
        return 1;
    } else {
        return -1;
    }
}

int int_value(char *payload) {
    int value;

    if(payload[0] == 0) {
        //Nr. pozitiv.
        value = 1;
    } else if(payload[0] == 1) {
        //Nr. negativ.
        value = -1;
    }

    uint32_t payload_value;
    memcpy(&payload_value, &payload[1], 4);

    value = value * ntohl(payload_value);

    return value;
}

float short_real_value(char *payload) {
    uint16_t modulus;

    modulus = 0;
    memcpy(&modulus, payload, 2);
    
    modulus = ntohs(modulus); 

    return modulus / 100.0;
}

double float_value(char *payload) {
    double value;
    uint32_t modulus;
    uint16_t negative_power_of_10;

    if(payload[0] == 0) {
        //Nr. pozitiv.
        value = 1.0;
    } else if(payload[0] == 1) {
        //Nr. negativ.
        value = -1.0;
    }

    modulus = 0;
    memcpy(&modulus, &payload[1], 4);
    modulus = ntohl(modulus);

    negative_power_of_10 = payload[5];
    
    if(negative_power_of_10 != 0) {
        return (value * modulus) / pow(10, negative_power_of_10);
    }

    return value * modulus;
}

void show_int(to_tcp_client m) {
    printf("%s:%d - %s - INT - %d\n", m.udp_client_ip, m.port,
                                      m.m.topic, int_value(m.m.payload));
}

void show_short_real(to_tcp_client m) {
    printf("%s:%d - %s - SHORT_REAL - %0.2f\n", m.udp_client_ip, m.port,
                                            m.m.topic, short_real_value(m.m.payload));
}

void show_float(to_tcp_client m) {
    printf("%s:%d - %s - FLOAT - %f\n", m.udp_client_ip, m.port,
                                          m.m.topic, float_value(m.m.payload));
}

void show_string(to_tcp_client m) {
    printf("%s:%d - %s - STRING - %s\n", m.udp_client_ip, m.port,
                                         m.m.topic, m.m.payload);
}

void output_subscriber(to_tcp_client m) {
    if(m.m.type == 0) {
        show_int(m);
    } else if(m.m.type == 1) {
        show_short_real(m);
    } else if(m.m.type == 2) {
        show_float(m);
    } else if(m.m.type == 3) {
        show_string(m);
    }
}

int check_if_error_from_server(to_tcp_client *m) {
    if(strcmp(m -> m.payload, "ID_already_in_use") == 0) {
        fprintf(stderr, "[subscriber] ID already in use.\n");
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[]) {

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    //Multimea socketilor pe care se primesc mesaje.
    fd_set read_fds;
    /*
        Multimea de socketi in care se copiaza read_fds
        la fiecare iteratie, ca sa nu se piarda continutul
        dupa apelul select(...).
    */
    fd_set tmp_fds;
    int sock, ret, fdmax;
    struct sockaddr_in serv_addr;

    check_command_line_arguments(argc, argv);
    initialize_socket_sets(&read_fds, &tmp_fds);
    connect_to_server(&sock, &read_fds, &fdmax, argv, &serv_addr);

    while(1) {
        //Fac o copie a socketilor de input.
        tmp_fds = read_fds;

        /*
            Se blocheaza in select pana primeste
            date pe cel putin un socket.
        */
        ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret == -1, "[subscriber] Error at the call of select in main.\n");

        //Se primeste comanda la stdin.
        if(FD_ISSET(0, &tmp_fds) != 0) {
            ret = stdin_command(sock);
            
            if(ret == 0) {
                //Se da exit pe subscriber.
                close_client_socket(sock);
				break;
            } else if (ret == -1) {
                //Se da un tip de comanda nerecunoscut.
                DIE(ret == -1, "[subscriber] Command type unknown.\n");
                continue;
            } else if (ret == 1) {
                /*
                    Se da subscribe/unsubscribe. Cazurile
                    sunt tratate in functie.
                */
                continue;
            }
        }

        //Se primeste mesaj de la server.
        if(FD_ISSET(sock, &tmp_fds) != 0) {
            //Structura in care parsez mesajul de la server.
            to_tcp_client m;
			
            ret = recv(sock, &m, sizeof(to_tcp_client), 0);
			DIE(ret == -1, "[subscriber] Error at receveing from server.\n");

			/* 	
                Daca serverul s-a inchis, se inchid si subscriberii.
			*/
			if (ret == 0) {
                close_client_socket(sock);
				break;
			}

            //Se trateaza mesaje de eroare primite de la server.
            ret = check_if_error_from_server(&m);
            if(ret == 1) {
                /*
                    Cazul in care clientul a incercat sa se
                    conecteze cu un id deja existent.
                */
                close_client_socket(sock);
                break;
            } else {
                output_subscriber(m);
            }
        }
    }

    return 0;
}