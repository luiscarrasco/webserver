/*
The MIT License (MIT)

Copyright (c) 2013 luiscarrasc0

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include <stdio.h>
#include "c_hashmap/hashmap.h"
#include <unistd.h>
#include <netinet/in.h>    
#include <sys/socket.h>    
#include <arpa/inet.h>     
#include <stdlib.h>        
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/inotify.h>
#include <errno.h>

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )
#define SERVER_PORT            (8080)
#define PEND_CONNECTIONS     	100     
#define BUF_SIZE            	1024
#define OK_TEXT 				"HTTP/1.0 200 OK\nContent-Type:text/html\n\n"
#define ERROR_400				"HTTP/1.0 404 Not Found\nContent-Type:text/html\n\n"

void *handle_client(void*);
void *handle_file_system(void*);
map_t map;
pthread_mutex_t map_mutex;

typedef struct file_entry {
	char * content;
	time_t last_modified;
	size_t contentSize;
} sFile;


int main() {

	unsigned int          	server_s;             
	struct sockaddr_in    	server_addr;          
	struct sockaddr_in    	client_addr; 
	
	unsigned int    		client_s;     
	int                   	addr_len;
	int 					socket_thread;   
	int 					fsystem_thread;
	pthread_attr_t       	attr;                   //  pthread attributes
  	pthread_t             	threads;                // Thread ID (used by OS
  	pthread_t 				inot_thread;
  	int err;
  	int tr = 1;

	/*Create hasmap for file cache*/  	
  	map = hashmap_new();

  	/*Create server socket*/
	server_s = socket(AF_INET, SOCK_STREAM, 0);
	/*Thread creation attributes*/
	pthread_attr_init(&attr);
	/*Create file change notification thread*/
	pthread_create(
			&inot_thread,
			&attr,
			handle_file_system,
			&fsystem_thread
		);

	/*Set detached attribute to threads. We do not need to wait for threads to finish*/
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	setsockopt(server_s,SOL_SOCKET,SO_REUSEADDR,&tr,sizeof(int));
	//setNonblocking(server_s);

	/*Create server address structure*/
	memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port        = htons(SERVER_PORT);

    addr_len = sizeof(client_addr);

    /*Bind server address and socket*/
    if( 0 == (err = bind(server_s, (struct sockaddr *)&server_addr, sizeof(server_addr)) )) {
    	/*Listen for connections*/
    	if( 0 == (err = listen(server_s, PEND_CONNECTIONS) )) {
    		/*Client connection acceptance loop*/
    		while(1) {
		    	/*Accept client connections */
		    	;

		    	if((client_s = accept(server_s, (struct sockaddr *)&client_addr, &addr_len)) < 0) {
		    		perror("accept");
		    	} else {
		    		/*Create thread that will handle http request*/
		    		socket_thread = client_s;   		
		    		pthread_create(
		    				&threads,
		    				&attr,
		    				handle_client,
		    				&socket_thread
		    			);
		    	}
		    }
    	} else {
    		perror("listen");
    	}
    } else {
    	perror("bind");
    }
    
}

void * handle_file_system(void * arg) {
	int length, i = 0;
	int fd;
	int wd;
	char buffer[EVENT_BUF_LEN];
	int 		   is_missing;
	struct stat fileStats;

	/*Create inotify file handler*/
	if( (fd = inotify_init()) < 0 ) {
		perror("inotify_init");
	} else {
		/*Success on inotify*/
	}

	/*Add watch for file create, file delete and file modify on close*/
	wd = inotify_add_watch( fd, "./", IN_CREATE | IN_DELETE | IN_CLOSE_WRITE);

	//setNonblocking(fd);

	while(1){
		i = 0;
		/*Read event from file descriptor*/
		length = read( fd, buffer, EVENT_BUF_LEN ); 

		if( errno == EINTR) {
			/*Interrupted Signal*/
		} else {
			/*checking for error*/
			if ( length < 0 ) {
				//perror( "read" );
			}  

			/*Iterate over read events from inotify file descriptor*/
			while ( i < length ) {     
				struct inotify_event *event = ( struct inotify_event * ) &buffer[ i ];     
				/*If we have an event with a length*/
				if ( event->len ) {



			  		
			  		/*Retrieve file stats*/
			  		if ( stat(event->name, &fileStats ) < 0 ) {
			  		} else {
			  			/*File stats retrieved*/
			  		}

			  		/*Handle file reading*/
			  		if ( event->mask & IN_CLOSE_WRITE ) {
			  			sFile *fEntry;

			  			 pthread_mutex_lock(&map_mutex);
			        	/*Read Hash*/
			        	is_missing = hashmap_get(map, event->name, (void**)(&fEntry));
			        	/*Unlock hash*/
			        	pthread_mutex_unlock(&map_mutex);

			        	if(MAP_MISSING != is_missing){
						    if ( 0 == (event->mask & IN_ISDIR) ) {    
						    	int fh;
						    	/*Open file for reading*/
						    	if (( fh = open(event->name, O_RDONLY, S_IREAD)) == -1 ) {
						    		perror("open");
						    	} else {
							    	/*Allocate memory*/
							    	fEntry = malloc(sizeof(sFile));
						        	fEntry->content = malloc(fileStats.st_size);
						        	fEntry->contentSize = fileStats.st_size;

							    	while(1) {
							    		/*No automatic signal restart*/
							    		/*Read file*/
							    		int readbytes = read(fh, fEntry->content, fEntry->contentSize);

							    		
							    		
							    		if( errno == EINTR){
							    			/*Read again*/
							    			printf("error\n");
							    		} else {
								    		/*Lock Hash*/
								    		pthread_mutex_lock(&map_mutex);
								    		/*Add file to hash*/
								    		hashmap_put(map, event->name, (any_t)fEntry);
								    		/*Unlock hash*/
								    		pthread_mutex_unlock(&map_mutex);
								    		break;
							    		}
							    	}

						    	}

							    close(fh);

						    } else {
						    	/*File has not been requested*/
						    }				    	

				    	} else {
				    		/*File has not been requested*/
											    		
				    	}
			  		}
			  		/*Handle file deleting*/
			  		else if ( event->mask & IN_DELETE ) {
			    		if ( 0 == (event->mask & IN_ISDIR) ) {
			      			/*Lock hash*/
			      			pthread_mutex_lock(&map_mutex);
			      			/*Remove entry from hash*/
			      			hashmap_remove(map, event->name);
			      			/*Unlock hash*/
			      			pthread_mutex_unlock(&map_mutex);
			    		} else {
			    			/*Not a file, actually a directory*/
			    		}
			  		} else {
			  			/*Unhandled notification message*/
			  		}

				}
				/*Iterate on all events received*/
				i += EVENT_SIZE + event->len;
			}

			/*We do not want to break from this read while loop if it is not interrupted*/
		}

		
	}
}

void * handle_client(void* arg) {
	unsigned int    myClient_s;         
	char           in_buf[BUF_SIZE];
	char           out_buf[BUF_SIZE];
	unsigned int   buf_len;             
	unsigned int   fh;                
	char           *file_name;        
	int 		   is_missing;
	struct stat fileStats;
	sFile *fEntry;

	/*Copy socket from thread argument*/
	myClient_s = *(unsigned int *)arg;
	/*Receive information from socket*/
	if( recv(myClient_s, in_buf, BUF_SIZE, 0) < 0 ) {
		/*Did not receive from socket successfully*/
	} else {
		/*Extract file from received header*/
		strtok(in_buf, " ");
        file_name = strtok(NULL, " ");

        /*Open file to see if file exists*/

        if(file_name != NULL)
        {
	        printf("Request for: %s\n", file_name);

	        pthread_mutex_lock(&map_mutex);
        	/*Read Hash*/
        	is_missing = hashmap_get(map, &file_name[1], (void**)(&fEntry));
        	/*Unlock hash*/
        	pthread_mutex_unlock(&map_mutex);

        	if( MAP_MISSING == is_missing){

		        if( (fh = open(&file_name[1], O_RDONLY, S_IREAD)) == -1 ) {
		        	/*Handle HTML 400 ERROR*/
		        	strcpy(out_buf, ERROR_400);
		        	send(myClient_s, out_buf, strlen(out_buf), MSG_NOSIGNAL );
		        } else {
	        		/*Retrieve file stats*/
		        	if ( stat(&file_name[1], &fileStats ) < 0) {

		        	} else {
		        		/*Allocate memory for file entry*/
		        		fEntry = malloc(sizeof(sFile));
					    fEntry->contentSize = fileStats.st_size;
					    fEntry->content = malloc(fEntry->contentSize);

		        		while(1) {
		        			/*Read the file and save it to cache*/
		        			int readBytes = read(fh, fEntry->content, fEntry->contentSize);
		        			
	        				/*No automatic signal restart*/
		        			if( errno == EINTR ) {
		        				/*Retry*/
		        			} else {
					        	/*Lock Hash*/
							    pthread_mutex_lock(&map_mutex);
							    /*Write to hash*/
					        	hashmap_put(map, &file_name[1], (any_t)fEntry);
					        	/*Unlock Hash*/
					        	pthread_mutex_unlock(&map_mutex);
					        	break;
		        			}
		        		}
				        	
		        	}
		        } 


			} else {
				/*File exists in cache.*/
			}

			
			/*Send content*/
			if(fEntry != NULL){
				strcpy(out_buf, OK_TEXT);
					send(myClient_s, out_buf, strlen(out_buf), MSG_NOSIGNAL );
				if(fEntry->contentSize > 0)
					send(myClient_s, fEntry->content, fEntry->contentSize, MSG_NOSIGNAL);
			}
			
		}
		/*Handle closing*/
		close(fh);  
		close(myClient_s); 
	}
	
}

