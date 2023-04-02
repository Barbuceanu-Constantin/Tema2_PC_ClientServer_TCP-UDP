#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include "helpers.h"
#include "app_protocol.h"

void check_command_line_arguments(int argc, char *argv[]) {
    DIE(argc != 2, "server must have only 1 command line argument.\n");
}

void initialize_socket_sets(fd_set *read, fd_set *tmp) {
    FD_ZERO(read);
    FD_ZERO(tmp);
}

void open_server(fd_set *read, int *udp, int *tcp,
                struct sockaddr_in *server, char *port_s,
                int *fdmax) {
    int ret;

    //Socket UDP pe care se primesc si conexiuni si mesaje.
    *udp = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(*udp == -1, "Error at opening of udp socket\n.");

    //Socket TCP pe care se primesc doar conexiuni (creare).
    *tcp = socket(AF_INET, SOCK_STREAM, 0);
    DIE(*tcp == -1, "Error at opening of tcp socket\n.");

    int port = atoi(port_s);
    DIE(port <= 0, "[server] Error at converting the port from string to int (atoi).\n");

    //Completare structura server.
    memset((char*) server, 0, sizeof(struct sockaddr_in));
	server -> sin_family = AF_INET;
	server -> sin_port = htons(port);
	server -> sin_addr.s_addr = INADDR_ANY;

    //Legare socket UDP la port.
    ret = bind(*udp, (struct sockaddr*) server, sizeof(struct sockaddr));
	DIE(ret == -1, "Error at binding udp socket to port.\n");

    //Legare socket pasiv TCP la port.
    ret = bind(*tcp, (struct sockaddr*) server, sizeof(struct sockaddr));
	DIE(ret == -1, "Error at binding tcp passive socket to port.\n");

    //Face socketul TCP sa primeasca doar connect-uri.
    ret = listen(*tcp, MAX_WAITING_CLIENTS);
	DIE(ret == -1, "Error at listen function call.\n");

    //Introduc socketii de citire ai serverului.
    FD_SET(*tcp, read);
    FD_SET(*udp, read);
    FD_SET(0, read);

    //Setez fdmax.
    if(*tcp < *udp) {
        *fdmax = *udp;
    } else {
        *fdmax = *tcp;
    }
}

void close_sockets(int udp, int tcp) {
    close(udp);
    close(tcp);
}

int close_server(int udp, int tcp) {
    char comm[15];
    int ok;
    
    //Citesc comanda.
    fscanf(stdin, "%s", comm);

    ok = strcmp(comm, "exit");
    if(!ok) {
        //Daca s-a primit exit.
        close_sockets(udp, tcp);
        return 1;
    }

    //Daca nu s-a primit exit.
    DIE(ok != 0, "Error. Server can only get \"exit\" command.\n");
    return 0;
}

void output_server(struct sockaddr_in cl, from_tcp_client m, char *type) {
    if(strcmp(type, "connected") == 0) {
        printf("New client %s connected from %s:%d.\n", m.id,
                inet_ntoa(cl.sin_addr),
                ntohs(cl.sin_port));
    }
}

void disable_Neagle(int sock) {
    int ok = 1;
	int result = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *) &ok, sizeof(int));
	DIE(result == -1, "[server] Error at disabling Neagle.\n");
}

void fill_tcp_sending_structure(void *buf, struct sockaddr_in from_udp,
                                to_tcp_client *m) {
    //Setez structura pe 0.
    memset(m, 0, sizeof(to_tcp_client));

    //Completez ip-ul si portul.
    memcpy(m -> udp_client_ip, inet_ntoa(from_udp.sin_addr), IP_MAX_LEN);
    m -> port = from_udp.sin_port;   //Nu stiu daca e htons sau ntohs. De verificat.

    //Completez topicul, type-ul si payloadul.
    memcpy(m -> m.topic, ((from_udp_client *) buf) -> topic, TOPIC_MAX_LEN);
    m -> m.type = ((from_udp_client *) buf) -> type;

    //Daca payloadul e de tip string.
    if (((from_udp_client *) buf) -> type == 3) {
		char aux[PAYLOAD_LEN];
		strcpy(aux, ((from_udp_client *) buf) -> payload);
		strcat(aux, "\0");
		strcpy(m -> m.payload, aux);
	} else {
        //Dac payloadul nu e string.
		memcpy(m -> m.payload,((from_udp_client *) buf) -> payload, PAYLOAD_LEN);
	}
}

void fillClient(client *clients, int *nr_clients, int *active_clients, int sock, from_tcp_client m) {
    clients[*nr_clients].socket = sock;
    memcpy(clients[*nr_clients].id, m.id, strlen(m.id));
    clients[*nr_clients].nr_topics = 0;
    clients[*nr_clients].topics_array_phys_dim = 2;
    clients[*nr_clients].topics = (topic *) malloc(sizeof(topic) * clients[*nr_clients].topics_array_phys_dim);
	clients[*nr_clients].connected = 1;

    DIE(clients[*nr_clients].topics == NULL, "[server] Error at allocating memory for topics.\n");
	
    clients[*nr_clients].nr_stored_messages = 0; 
    clients[*nr_clients].stored_messages_array_phys_dim = 2;
    clients[*nr_clients].stored_messages_array = (to_tcp_client *) malloc(sizeof(to_tcp_client) * 2);

    DIE(clients[*nr_clients].stored_messages_array == NULL, "[server] Error at allocating memory for stored messages.\n");

    //Incrementez.
    (*nr_clients) ++;
	(*active_clients) ++;
}

void send_message_error_to_client(int sock, char *error) {
    to_tcp_client m;
    int ret;

    strcpy(m.m.payload, error);

    ret = send(sock, &m, sizeof(to_tcp_client), 0);
	DIE(ret == -1, "[server] Error at sending error message to client.\n");
}

void empty_message_queue(to_tcp_client *queue, int sock, int nr) {
    int i, ret;
    
    //Daca exista mesaje in coada.
    if(nr > 0) {
        //Le parcurg si le trimit.
        for(i = 0; i < nr; ++i) {
            ret = send(sock, &queue[i], sizeof(to_tcp_client), 0);
            DIE(ret == -1, "[server] Error at sending message to tcp client (empty message queue).\n");
        }
    }

    //Eliberez coada(care este de fapt un vector de structuri.)
    free(queue);
}

void check_client(client *clients, int *nr_clients, int *active_clients, int sock, fd_set *read,
                  from_tcp_client *m, int *reconnected, int *already_existing_client,
                  struct sockaddr_in cl) {
    int i;
    for(i = 0; i < *nr_clients; ++i) {
        if(clients[i].connected == 1 && strcmp(clients[i].id, m -> id) == 0) {
            //Daca exista deja un client conectat cu idul respectiv.
            *already_existing_client = 1;

            printf("Client %s already connected.\n", m -> id);

            //Trimit mesajul de eroare si elimin socketul din vectorul de descriptori.
            send_message_error_to_client(sock, "ID_already_in_use");
			FD_CLR(sock, read);

            break;
        }
        if(clients[i].connected == 0 && strcmp(clients[i].id, m -> id) == 0) {
            /*
                Daca exista deja o intrare in baza de date
                cu id-ul respectiv dar niciun client nu este
                conectat la ea la momentul curent.
            */
            *reconnected = 1;
            
            *active_clients += 1;
			clients[i].connected = 1;
			clients[i].socket = sock;

            //Afisez din nou chiar daca s-a reconectat.
            output_server(cl, *m, "connected");

            //Daca are mesaje ce trebuie trimise odata ce s-a reconectat, se goleste coada de mesaje a clientului.
            empty_message_queue(clients[i].stored_messages_array, clients[i].socket, clients[i].nr_stored_messages);

            break;
        }
    }
}

void realloc_database(client **clients, int *max_nr) {
    *max_nr = (*max_nr) * 2;

    client *aux = (client *) realloc(*clients, sizeof(client) * (*max_nr));
	DIE(aux == NULL, "[server] Error at reallocating clients database.\n");
	
    *clients = aux; 
}

void enqueue(client *cl, to_tcp_client *m) {
    //Pun mesajul in coada.
    memcpy(&(cl -> stored_messages_array[cl -> nr_stored_messages]), m, sizeof(to_tcp_client));
    
    //Incrementez numarul de mesaje.
    cl -> nr_stored_messages ++;

    //Verific daca s-a atins dimensiunea maxima a vectorului si daca da realoc.
    if(cl -> nr_stored_messages == cl -> stored_messages_array_phys_dim) {
        cl -> stored_messages_array_phys_dim *= 2;

        to_tcp_client *new_array;
        new_array = realloc(cl -> stored_messages_array, sizeof(to_tcp_client) * cl -> stored_messages_array_phys_dim);
        DIE(new_array == NULL, "[server] Error at reallocating stored messages cache.\n");

        cl -> stored_messages_array = new_array;
    }
}

void send_message_to_tcp_clients(int nr_clients, client *clients, from_udp_client *buf,
                                 to_tcp_client *m) {
    int i, ret;

    //Parcurg toti clientii.
    for (i = 0; i < nr_clients; i++) {
		client *cl = &(clients[i]);

        //Parcurg toate topicurile unui client.
		for (int j = 0; j < cl -> nr_topics; j++) {
            /*
                Daca numele topicului se regaseste in lista de topicuri
                a clientului si este si abonat la topic.
            */
			if (strcmp(cl->topics[j].title, buf->topic) == 0 && cl->topics[j].subscribed == 1) {
                
                /*
                    Daca clientul este conectat la momentul venirii mesajului pe server,
                    mesajul se trimite direct la client.
                */
				if (cl->connected == 1) {
					ret = send(cl->socket, m, sizeof(to_tcp_client), 0);
					DIE(ret == -1, "[server] Error at sending message to tcp client.\n");
                }

				/* 	
					Daca clientul este deconectat de la server cand ii vin mesaje,
                    dar are SF setat pe 1, atunci se retine fiecare mesaj m care i
                    se adreseaza in "stored_messages".
				*/
                if (cl->connected == 0 && cl->topics[j].SF == 1) {
					enqueue(cl, m);
				}
			}
		}
	}
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
    int fdmax, ret;
    int udp_socket, tcp_listening_socket, tcp_client_socket;
    int nr_clients, active_clients, max_nr_of_clients;
    struct sockaddr_in serv_addr, cl_addr;

    check_command_line_arguments(argc, argv);
    initialize_socket_sets(&read_fds, &tmp_fds);
    open_server(&read_fds, &udp_socket, &tcp_listening_socket,
                &serv_addr, argv[1], &fdmax);

    /* 
        Vectorul alocat dinamic in care se tine evidenta
        tuturor clientilor care s-au conectat sau se vor
        reconecta.
    */
	client *clients = (client *)malloc(sizeof(client) * max_nr_of_clients);
	DIE(clients == NULL, "[server] Error at allocating memory for clients.\n");

    nr_clients = 0;
    active_clients = 0;
    max_nr_of_clients = 1;

    while(1) {
        //Fac o copie a socketilor de input.
        tmp_fds = read_fds;

        /*
            Se blocheaza in select pana primeste
            date pe cel putin un socket.
        */
        ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret == -1, "Error at the call of select in main.\n");

		if(FD_ISSET(0, &tmp_fds)) {
			
            //Vin date pe socketul de stdin.
            if(close_server(udp_socket, tcp_listening_socket)) {
                //Daca serverul s-a inchis in mod corect.
				break;
			} else {
                /*
                    Serverul nu se inchide daca primeste
                    altceva decat exit.
                */
                continue;
            }

		} else if(FD_ISSET(udp_socket, &tmp_fds)) {
            
            /*
                Vin date pe socketul de udp.
                In structura asta primesc ip-ul si portul clientului udp.
            */
            struct sockaddr_in from_udp;
            from_udp_client *buf = (from_udp_client *) malloc(sizeof(from_udp_client));
            unsigned int l = sizeof(struct sockaddr_in);

            //Setez bufferul de mesaj pe 0.
            memset(buf, 0, sizeof(from_udp_client));

            //Primesc mesajul de la clientul UDP plus credentialele clientului.
            ret = recvfrom(udp_socket, buf, sizeof(from_udp_client), 0, (struct sockaddr*) &from_udp, &l);	
			DIE(ret == -1, "[server] recvfrom problem.\n");

            //Structura trimisa catre clientii tcp.
            to_tcp_client m;
            fill_tcp_sending_structure(buf, from_udp, &m);

            /*
                Trimit mesajele la clientii abonati la topic sau le pun
                in memorie pt. cei deconectati.
            */
            send_message_to_tcp_clients(nr_clients, clients, buf, &m);

            //Dezaloc memoria bufferului de mesaj.
            free(buf);

        } else if(FD_ISSET(tcp_listening_socket, &tmp_fds)) {
            
            //Vin date pe socketul pe care se asculta conexiuni tcp.
            socklen_t cl_length = sizeof(cl_addr);
            int reconnected = 0;
            int already_existing_client = 0;

            //Accept o conexiune.
            tcp_client_socket = accept(tcp_listening_socket,
                                       (struct sockaddr*) &cl_addr,
                                       &cl_length);
			DIE(tcp_client_socket == -1, "[server] Error at accepting connection.\n");

            /*
                Imediat dupa acceptarea unei conexiuni se primeste
                un mesaj cu ID_CLIENT.
            */
            from_tcp_client m;
			ret = recv(tcp_client_socket, &m, sizeof(from_tcp_client), 0);
			DIE(ret == -1, "[server] Error at receiving message.\n");

            /*
                Introduc noul socket de comunicare cu clientul
                in lista de socketi de citire.
            */
			FD_SET(tcp_client_socket, &read_fds);

            /*
                Verific daca se reconecteaza un client deja
                existent in baza de date sau daca clientul
                incearca sa se conecteze cu un id deja existent.
            */
            check_client(clients, &nr_clients, &active_clients,
                        tcp_client_socket, &read_fds, &m,
                        &reconnected, &already_existing_client,
                        cl_addr);

            /*
                Daca nu avem caz de reconectare sau incercare de conectare
                cu id deja existent.
            */
            if(reconnected == 0 && already_existing_client == 0) {
                /* 
				    Daca nr-ul de clienti inregistrati in baza de date a
                    atins limita maxima, se realoca vectorul.
			    */
			    if(active_clients == max_nr_of_clients) {
				    realloc_database(&clients, &max_nr_of_clients);
			    }

                //Daca noul socket e mai mare decat socketul maxim. 
                if(already_existing_client == 0) {
			        if (tcp_client_socket > fdmax) {
				        fdmax = tcp_client_socket;
			        }
                }

                disable_Neagle(tcp_client_socket);

				//Populez structura clientului nou conectat.
                fillClient(clients, &nr_clients, &active_clients, tcp_client_socket, m);

                output_server(cl_addr, m, "connected");
            }

        } else {
            
            //Vin date pe un socket de mesaje de la un client(subscribe/unsubscribe).
            int i;
            for (i = 0; i < nr_clients; i++) {
                client *c = &(clients[i]);

				if (FD_ISSET(c -> socket, &tmp_fds)) {
                    from_tcp_client m;
					ret = recv(c->socket, &m, sizeof(from_tcp_client), 0);
					DIE(ret == -1, "[server] Error at receiving message from tcp client.\n");

                    //Daca un client se deconecteaza.
                    if(ret == 0) { 
						
                        printf("Client %s disconnected.\n", c -> id);
						
                        c -> connected = 0;
						active_clients--;
						
                        FD_CLR(c -> socket, &read_fds);
                    } else if(strncmp(m.c, "subscribe", 9) == 0) {
                        /*
                            S-a primit comanda de subscribe de la client.
						    Daca s-a atins numarul maxim de topicuri se redimensioneaza.
                        */
                        if(c -> nr_topics == c -> topics_array_phys_dim) {
                            c -> topics_array_phys_dim *= 2;
                            topic **t = &(c -> topics);
                            topic *aux = (topic *)realloc(*t, sizeof(topic) * c -> topics_array_phys_dim);
                            *t = aux;
                        }

                        //Se completeaza campurile structurii topic.
						strcpy(c -> topics[c -> nr_topics].title, m.topic);
						c -> topics[c -> nr_topics].SF = m.SF;
						c -> topics[c -> nr_topics].subscribed = 1;
						c -> nr_topics += 1;
					} else {
						continue;
					}
                }
            }

        }
    }

    return 0;
}