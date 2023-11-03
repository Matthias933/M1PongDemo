/*
 * server.c
 *
 *  Created on: Sep 27, 2018
 *      Author: duco
 */

#include "ws/Handshake.h"
#include "ws/Communicate.h"
#include "ws/Errors.h"
#include "ws/Datastructures.h"
#include <sockLib.h>
#include <pthread.h>
#include <inetLib.h>

ws_list *server_l;
int server_port;

#define PORT 4567

/**
 * Shuts down a client in a safe way. This is only used for Hybi-00.
 */
void server_cleanup_client(void *args) {
    ws_client *n = args;
    if (n != NULL) {
        printf("Shutting client down..\n\n> ");
        fflush(stdout);
        list_remove(server_l, n);
    }
}

void *server_handleClient(void *args) {
    pthread_detach(pthread_self());
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    pthread_cleanup_push(&server_cleanup_client, args);

    int buffer_length = 0, string_length = 1, reads = 1;

    ws_client *n = args;
    n->thread_id = pthread_self();

    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    char buffer[BUFFERSIZE];
    n->string = (char *) malloc(sizeof(char));

    if (n->string == NULL) {
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        handshake_error("Couldn't allocate memory.", ERROR_INTERNAL, n);
        pthread_exit((void *) EXIT_FAILURE);
    }

    printf("Client connected with the following information:\n"
           "\tSocket: %d\n"
           "\tAddress: %s\n\n", n->socket_id, (char *) n->client_ip);
    printf("Checking whether client is valid ...\n\n");
    fflush(stdout);

    /**
     * Getting headers and doing reallocation if headers is bigger than our
     * allocated memory.
     */
    do {
        memset(buffer, '\0', BUFFERSIZE);
        if ((buffer_length = recv(n->socket_id, buffer, BUFFERSIZE, 0)) <= 0){
            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
            handshake_error("Didn't receive any headers from the client.",
                    ERROR_BAD, n);
            pthread_exit((void *) EXIT_FAILURE);
        }

        if (reads == 1 && strlen(buffer) < 14) {
            handshake_error("SSL request is not supported yet.",
                    ERROR_NOT_IMPL, n);
            pthread_exit((void *) EXIT_FAILURE);
        }

        string_length += buffer_length;

        char *tmp = realloc(n->string, string_length);
        if (tmp == NULL) {
            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
            handshake_error("Couldn't reallocate memory.", ERROR_INTERNAL, n);
            pthread_exit((void *) EXIT_FAILURE);
        }
        n->string = tmp;
        tmp = NULL;

        memset(n->string + (string_length-buffer_length-1), '\0',
                buffer_length+1);
        memcpy(n->string + (string_length-buffer_length-1), buffer,
                buffer_length);
        reads++;
    } while( strncmp("\r\n\r\n", n->string + (string_length-5), 4) != 0
            && strncmp("\n\n", n->string + (string_length-3), 2) != 0
            && strncmp("\r\n\r\n", n->string + (string_length-8-5), 4) != 0
            && strncmp("\n\n", n->string + (string_length-8-3), 2) != 0 );

    printf("User connected with the following headers:\n%s\n\n", n->string);
    fflush(stdout);

    ws_header *h = header_new();

    if (h == NULL) {
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        handshake_error("Couldn't allocate memory.", ERROR_INTERNAL, n);
        pthread_exit((void *) EXIT_FAILURE);
    }

    n->headers = h;

    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    if ( parseHeaders(n->string, n, server_port) < 0 ) {
        pthread_exit((void *) EXIT_FAILURE);
    }

    if ( sendHandshake(n) < 0 && n->headers->type != UNKNOWN ) {
        pthread_exit((void *) EXIT_FAILURE);
    }

    list_add(server_l, n);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    printf("Client has been validated and is now connected\n\n");
    printf("> ");
    fflush(stdout);

    uint64_t next_len = 0;
    char next[BUFFERSIZE];
    memset(next, '\0', BUFFERSIZE);

    while (1) {
        if ( communicate(n, next, next_len) != CONTINUE) {
            break;
        }

        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        if (n->headers->protocol == CHAT) {
            list_multicast(server_l, n);
        } else if (n->headers->protocol == ECHO) {
            list_multicast_one(server_l, n, n->message);
        } else {
            list_multicast_one(server_l, n, n->message);
        }
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

        if (n->message != NULL) {
            memset(next, '\0', BUFFERSIZE);
            memcpy(next, n->message->next, n->message->next_len);
            next_len = n->message->next_len;
            message_free(n->message);
            free(n->message);
            n->message = NULL;
        }
    }

    printf("Shutting client down..\n\n");
    printf("> ");
    fflush(stdout);

    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    list_remove(server_l, n);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    pthread_cleanup_pop(0);
    pthread_exit((void *) EXIT_SUCCESS);
}

void server_sigint_handler(int sig) {
    printf("signal\n");
}

int server_main() {
    int server_socket, client_socket, on = 1;

    struct sockaddr_in server_addr, client_addr;
    socklen_t client_length;
    pthread_t pthread_id;
    pthread_attr_t pthread_attr;

    /**
     * Creating new lists, l is supposed to contain the connected users.
     */
    server_l = list_new();

    /**
     * Listens for CTRL-C and Segmentation faults.
     */
    (void) signal(SIGINT, &server_sigint_handler);
    (void) signal(SIGSEGV, &server_sigint_handler);
    (void) signal(SIGPIPE, &server_sigint_handler);


    printf("Server: \t\tStarted\n");
    fflush(stdout);

    server_port = PORT;

    printf("Port: \t\t\t%d\n", server_port);
    fflush(stdout);

    /**
     * Opening server socket.
     */
    if ( (server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
        server_error(strerror(errno), server_socket, server_l);
    }

    printf("Socket: \t\tInitialized\n");
    fflush(stdout);

    /**
     * Allow reuse of address, when the server shuts down.
     */
    if ( (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &on,
                    sizeof(on))) < 0 ){
        server_error(strerror(errno), server_socket, server_l);
    }

    printf("Reuse Port %d: \tEnabled\n", server_port);
    fflush(stdout);

    memset((char *) &server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(server_port);

    printf("Ip Address: \t\t%s\n", inet_ntoa(server_addr.sin_addr));
    fflush(stdout);

    /**
     * Bind address.
     */
    if ( (bind(server_socket, (struct sockaddr *) &server_addr,
            sizeof(server_addr))) < 0 ) {
        server_error(strerror(errno), server_socket, server_l);
    }

    printf("Binding: \t\tSuccess\n");
    fflush(stdout);

    /**
     * Listen on the server socket for connections
     */
    if ( (listen(server_socket, 10)) < 0) {
        server_error(strerror(errno), server_socket, server_l);
    }

    printf("Listen: \t\tSuccess\n\n");
    fflush(stdout);

    /**
     * Attributes for the threads we will create when a new client connects.
     */
    pthread_attr_init(&pthread_attr);
    pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(&pthread_attr, 524288);

    printf("Server is now waiting for clients to connect ...\n\n");
    fflush(stdout);

//    /**
//     * Create commandline, such that we can do simple commands on the server.
//     */
//    if ( (pthread_create(&pthread_id, &pthread_attr, cmdline, NULL)) < 0 ){
//        server_error(strerror(errno), server_socket, server_l);
//    }
//
//    /**
//     * Do not wait for the thread to terminate.
//     */
//    pthread_detach(pthread_id);

    while (1) {
        client_length = sizeof(client_addr);

        /**
         * If a client connects, we observe it here.
         */
        if ( (client_socket = accept(server_socket,
                (struct sockaddr *) &client_addr,
                &client_length)) < 0) {
            server_error(strerror(errno), server_socket, server_l);
        }

        /**
         * Save some information about the client, which we will
         * later use to identify him with.
         */
        char *temp = (char *) inet_ntoa(client_addr.sin_addr);
        char *addr = (char *) malloc( sizeof(char)*(strlen(temp)+1) );
        if (addr == NULL) {
            server_error(strerror(errno), server_socket, server_l);
            break;
        }
        memset(addr, '\0', strlen(temp)+1);
        memcpy(addr, temp, strlen(temp));

        ws_client *n = client_new(client_socket, addr);

        /**
         * Create client thread, which will take care of handshake and all
         * communication with the client.
         */
        if ( (pthread_create(&pthread_id, &pthread_attr, server_handleClient,
                        (void *) n)) < 0 ){
            server_error(strerror(errno), server_socket, server_l);
        }

        pthread_detach(pthread_id);
    }

    list_free(server_l);
    server_l = NULL;
    close(server_socket);
    pthread_attr_destroy(&pthread_attr);
    return EXIT_SUCCESS;
}

void send_to_all(char *message)
{
    do
    {
        if (server_l)
        {
            ws_connection_close status;

            ws_message *m = message_new();
            m->len = strlen(message);

            char *temp = malloc( sizeof(char)*(m->len+1) );
            if (temp == NULL) {
                raise(SIGINT);
                break;
            }
            memset(temp, '\0', (m->len+1));
            memcpy(temp, message, m->len);
            m->msg = temp;
            temp = NULL;

            if ( (status = encodeMessage(m)) != CONTINUE) {
                message_free(m);
                free(m);
                raise(SIGINT);
                break;
            }

            list_multicast_all(server_l, m);
            message_free(m);
            free(m);
        }
        else
        {
            // can't send
        }
    }
    while (0);
}
