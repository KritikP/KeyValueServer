#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <signal.h>
#include <pthread.h>
#include <stdbool.h>

#include "userBST.h"
#include "strbuf.h"

#define BACKLOG 5
#ifndef DEBUG
#define DEBUG 0
#endif

int running = 1;

// the argument we will pass to the connection-handler threads
struct connection {
    struct sockaddr_storage addr;
    socklen_t addr_len;
    int fd;
};

typedef struct args_t{
    struct connection* con;
    userBST* users;
}args_t;

int server(char *port);
void *echo(void *arg);

int main(int argc, char **argv)
{
	if (argc != 2) {
		printf("Usage: %s [port]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

    (void) server(argv[1]);
    return EXIT_SUCCESS;
}

void handler(int signal)
{
	running = 0;
}

int server(char *port)
{
    struct addrinfo hint, *info_list, *info;
    struct connection *con;
    int error, sfd;
    pthread_t tid;
    
    args_t* args = malloc(sizeof(args_t));
    args->users = newuserBST();
    args->con = con;
    
    // initialize hints
    memset(&hint, 0, sizeof(struct addrinfo));
    hint.ai_family = AF_UNSPEC;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_flags = AI_PASSIVE;
    	// setting AI_PASSIVE means that we want to create a listening socket

    // get socket and address info for listening port
    // - for a listening socket, give NULL as the host name (because the socket is on
    //   the local host)
    error = getaddrinfo(NULL, port, &hint, &info_list);
    if (error != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
        return -1;
    }

    // attempt to create socket
    for (info = info_list; info != NULL; info = info->ai_next) {
        sfd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
        
        // if we couldn't create the socket, try the next method
        if (sfd == -1) {
            continue;
        }

        // if we were able to create the socket, try to set it up for
        // incoming connections;
        // 
        // note that this requires two steps:
        // - bind associates the socket with the specified port on the local host
        // - listen sets up a queue for incoming connections and allows us to use accept
        if ((bind(sfd, info->ai_addr, info->ai_addrlen) == 0) &&
            (listen(sfd, BACKLOG) == 0)) {
            break;
        }

        // unable to set it up, so try the next method
        close(sfd);
    }

    if (info == NULL) {
        // we reached the end of result without successfuly binding a socket
        fprintf(stderr, "Could not bind\n");
        return -1;
    }

    freeaddrinfo(info_list);
        struct sigaction act;
        act.sa_handler = handler;
        act.sa_flags = 0;
        sigemptyset(&act.sa_mask);
        sigaction(SIGINT, &act, NULL);
        
        sigset_t mask;
        
        sigemptyset(&mask);
        sigaddset(&mask, SIGINT);
    
    // at this point sfd is bound and listening
    printf("Waiting for connection\n");
    while (running) {
    	// create argument struct for child thread
		con = malloc(sizeof(struct connection));
        con->addr_len = sizeof(struct sockaddr_storage);
        	// addr_len is a read/write parameter to accept
        	// we set the initial value, saying how much space is available
        	// after the call to accept, this field will contain the actual address length
        
        // wait for an incoming connection
        con->fd = accept(sfd, (struct sockaddr *) &con->addr, &con->addr_len);
        	// we provide
        	// sfd - the listening socket
        	// &con->addr - a location to write the address of the remote host
        	// &con->addr_len - a location to write the length of the address
        	//
        	// accept will block until a remote host tries to connect
        	// it returns a new socket that can be used to communicate with the remote
        	// host, and writes the address of the remote hist into the provided location
        
        // if we got back -1, it means something went wrong
        if (con->fd == -1) {
            perror("accept");
            continue;
        }

        // temporarily block SIGINT (child will inherit mask)
        error = pthread_sigmask(SIG_BLOCK, &mask, NULL);
        if (error != 0) {
        	fprintf(stderr, "sigmask: %s\n", strerror(error));
        	abort();
        }

		// spin off a worker thread to handle the remote connection
        args->con = con;
        //error = pthread_create(&tid, NULL, echo, con);
        error = pthread_create(&tid, NULL, echo, args);

		// if we couldn't spin off the thread, clean up and wait for another connection
        if (error != 0) {
            fprintf(stderr, "Unable to create thread: %d\n", error);
            close(con->fd);
            free(con);
            continue;
        }

		// otherwise, detach the thread and wait for the next connection request
        pthread_detach(tid);

        // unblock SIGINT
        error = pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
        if (error != 0) {
        	fprintf(stderr, "sigmask: %s\n", strerror(error));
        	abort();
        }

    }
    puts("No longer listening.");
    freeuserBST(args->users);
    free(con);
    free(args);
	pthread_detach(pthread_self());
	pthread_exit(NULL);
    // never reach here
    return 0;
}

#define BUFSIZE 8

enum request{GET, SET, DEL};

void *echo(void *arg)
{
    char host[100], port[10], buf[BUFSIZE + 1];
    //struct connection *c = (struct connection *) arg;
    args_t* args = (args_t*) arg;
    struct connection *c = args->con;
    FILE* fp = fdopen(args->con->fd, "w");

    bool initBufs = true;
    bool logIn = false;
    int error, nread;
    enum request req;
    int field = 0;
    int byteSize;
    int currByteCount = 0;
    bool isFailed = false;
    strbuf_t* readBuf = malloc(sizeof(strbuf_t));
    strbuf_t* field1 = malloc(sizeof(strbuf_t));

	// find out the name and port of the remote host
    error = getnameinfo((struct sockaddr *) &c->addr, c->addr_len, host, 100, port, 10, NI_NUMERICSERV);
    	// we provide:
    	// the address and its length
    	// a buffer to write the host name, and its length
    	// a buffer to write the port (as a string), and its length
    	// flags, in this case saying that we want the port as a number, not a service name
    if (error != 0) {
        fprintf(stderr, "getnameinfo: %s", gai_strerror(error));
        close(c->fd);
        return NULL;
    }

    printf("[%s:%s] connection\n", host, port);

    strbuf_t* username = malloc(sizeof(strbuf_t));
    strbuf_t* password = malloc(sizeof(strbuf_t));
    sb_init(username, 10);
    sb_init(password, 10);
    field = -2;

    if(args->users->totalCount == 0){
        
        fprintf(fp, "No accounts found, creating new account.\nEnter username: ");
        fflush(fp);
        
        while(!logIn && (nread = read(c->fd, buf, BUFSIZE)) > 0){
            buf[nread] = '\0';
            
            for(int i = 0; i < nread; i++){
                
                if(field == -2){    //Entering username
                
                    if(buf[i] != '\n'){
                        sb_append(username, buf[i]);
                    }
                    else{
                        fprintf(fp, "Enter password: ");
                        fflush(fp);
                        field = -1;
                    }
                }
                else if(field == -1){   //Entering password
                    if(buf[i] != '\n'){
                        sb_append(password, buf[i]);
                    }
                    else{
                        field++;
                        logIn = true;
                        break;
                    }
                }
                
            }
        }
        insertUser(args->users, username->data, password->data);
    }
    else{
        fprintf(fp, "Enter username: ");
        fflush(fp);
        while(!logIn && (nread = read(c->fd, buf, BUFSIZE)) > 0){
            for(int i = 0; i < nread; i++){
                if(field == -2){    //Entering username
                    if(buf[i] != '\n'){
                        sb_append(username, buf[i]);
                    }
                    else{
                        fprintf(fp, "Enter password: ");
                        fflush(fp);
                        field = -1;
                    }
                }
                else if(field == -1){   //Entering password
                    if(buf[i] != '\n'){
                        sb_append(password, buf[i]);
                    }
                    else{
                        field++;
                        logIn = true;
                        break;
                    }
                }
                
            }
        }
        
    }

    user_node* user = findUser(args->users, username->data);
    if(user != NULL){
        if(strcmp(user->pass, password->data) == 0)
            fprintf(fp, "Login successful.\n");
        else
            fprintf(fp, "Login failed, incorrect password.");
        fflush(fp);

    }
    else{
        fprintf(fp, "Login failed, user not found.\n");
        fflush(fp);
        isFailed = true;
    }

    sb_destroy(username);
    sb_destroy(password);
    free(username);
    free(password);

    field = 0;
    while ((nread = read(c->fd, buf, BUFSIZE)) > 0 && !isFailed) {
        buf[nread] = '\0';

        if(initBufs){
            sb_init(readBuf, 10);
            sb_init(field1, 10);
            initBufs = false;
        }

        for(int i = 0; i < nread; i++){
            if(buf[i] != '\n'){
                sb_append(readBuf, buf[i]);
            }
            else{
                if(field == 0){     //Finished reading the request type
                    field++;
                    if(DEBUG)printf("Found first newline; data is [%s]\n", readBuf->data);
                    if(strcmp(readBuf->data, "GET") == 0){
                        req = GET;      //0
                    }
                    else if(strcmp(readBuf->data, "SET") == 0){
                        req = SET;      //1
                    }
                    else if(strcmp(readBuf->data, "DEL") == 0){
                        req = DEL;      //2
                    }
                    else{
                        req = -1;
                        isFailed = true;
                        sb_destroy(readBuf);
                        sb_destroy(field1);
                        fprintf(fp, "BAD\n");
                        fflush(fp);
                        break;
                    }
                    if(DEBUG)printf("Request type: %d\n", req);
                    sb_destroy(readBuf);
                    sb_init(readBuf, 10);
                }
                else if(field == 1){    //Finished reading the bytesize
                    if(DEBUG)printf("Found second newline, data is %s\n", readBuf->data);
                    field++;
                    byteSize = atoi(readBuf->data);
                    if(byteSize == 0){
                        sb_destroy(readBuf);
                        sb_destroy(field1);
                        fprintf(fp, "BAD\n");
                        fflush(fp);
                        isFailed = true;
                        break;
                    }
                    else{
                        if(DEBUG) printf("ByteSize set to %d\n", byteSize);
                    }
                    sb_destroy(readBuf);
                    sb_init(readBuf, 10);
                    currByteCount = 0;
                }
                else if(field == 2){    //Read first string field
                    if(DEBUG) printf("Found third newline, data is %s\n", readBuf->data);
                    field++;

                    //If the bytecount is beyond what it should be
                    if(currByteCount > byteSize){
                        sb_destroy(readBuf);
                        sb_destroy(field1);
                        fprintf(fp, "LEN\n");
                        fflush(fp);
                        isFailed = true;
                        break;
                    }
                    else if(req == GET){
                        if(currByteCount != byteSize){
                            if(DEBUG) printf("Current byte [%d] and byteSize [%d]\n", currByteCount, byteSize);
                            sb_destroy(readBuf);
                            sb_destroy(field1);
                            fprintf(fp, "LEN\n");
                            fflush(fp);
                            isFailed = true;
                            break;
                        }
                        field = 0;
                        currByteCount = 0;
                        node* val = findValue(user->tree, readBuf->data);
                        
                        if(val != NULL){
                            fprintf(fp, "OKG\n%ld\n%s\n", strlen(val->value) + 1, val->value);
                            fflush(fp);
                        }
                        else{
                            fprintf(fp, "KNF\n");
                            fflush(fp);
                        }
                        sb_destroy(readBuf);
                        sb_destroy(field1);
                        initBufs = true;
                    }
                    else if(req == DEL){
                        if(currByteCount != byteSize){
                            if(DEBUG) printf("Current byte [%d] and byteSize [%d]\n", currByteCount, byteSize);
                            sb_destroy(readBuf);
                            sb_destroy(field1);
                            fprintf(fp, "LEN\n");
                            fflush(fp);
                            isFailed = true;
                            break;
                        }
                        field = 0;
                        currByteCount = 0;
                        node* val = findValue(user->tree, readBuf->data);
                        if(val != NULL){
                            fprintf(fp, "OKD\n%ld\n%s\n", strlen(val->value) + 1, val->value);
                            fflush(fp);
                            deleteValue(user->tree, readBuf->data);
                        }
                        else{
                            fprintf(fp, "KNF\n");
                            fflush(fp);
                        }
                        sb_destroy(readBuf);
                        sb_destroy(field1);
                        initBufs = true;

                    }
                    else{   //req == SET
                        sb_concat(field1, readBuf->data);
                        sb_destroy(readBuf);
                        sb_init(readBuf, 10);
                    }

                }
                else if(field == 3){
                    if(DEBUG) printf("Found fourth newline, data is %s\n", readBuf->data);

                    //Do stuff with second field, depends on request
                    if(currByteCount != byteSize){
                        if(DEBUG) printf("Current byte [%d] and byteSize [%d]\n", currByteCount, byteSize);
                        sb_destroy(readBuf);
                        sb_destroy(field1);
                        fprintf(fp, "LEN\n");
                        fflush(fp);
                        isFailed = true;
                        break;
                    }
                    if(req == SET){
                        field = 0;
                        currByteCount = 0;
                        fprintf(fp, "OKS\n");
                        fflush(fp);
                        insert(user->tree, field1->data, readBuf->data);
                    }
                    else{
                        isFailed = true;
                        sb_destroy(readBuf);
                        sb_destroy(field1);
                        fprintf(fp, "BAD\n");
                        fflush(fp);
                        break;
                    }
                    sb_destroy(readBuf);
                    sb_destroy(field1);
                    initBufs = true;
                }
            }
            currByteCount++;
        }

        //printf("[%s:%s] read %d bytes |%s|\n", host, port, nread, buf);
    }
    printf("[%s:%s] got EOF\n", host, port);
    
    fclose(fp);
    close(c->fd);
    free(readBuf);
    free(field1);
    free(c);
    //printTree(user->tree);
    return NULL;
}