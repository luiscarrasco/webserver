
#include <stdio.h>
#include <pthread.h>
#include <stdint.h>
#include <netinet/in.h>    
#include <sys/socket.h>    
#include <arpa/inet.h>  
#include <string.h>
#include <netdb.h>
#include <errno.h>

#define MAX_THREADS 	500
#define STATUS_SUCCESS 	0
#define STATUS_FAILED  	1
#define SERVER_PORT   	20904
#define BUF_SIZE        500


const char* GetRequest = "GET /main.c HTTP/1.1\n"
		"Host: localhost:8080\n"
		"Connection: keep-alive\n"
		"Cache-Control: max-age=0\n"
		"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\n"
		"User-Agent: Mozilla/5.0 (X11; Linux i686) AppleWebKit/537.22 (KHTML, like Gecko) Chrome/25.0.1364.172 Safari/537.22\n"
		"Accept-Encoding: gzip,deflate,sdch\n"
		"Accept-Language: en-US,en;q=0.8\n"
		"Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.3\n";

void *http_client(void*);


int main() {
	int 			thread_ctr;
	pthread_t 		tids[MAX_THREADS];
	pthread_attr_t  attr;     
	int 			retvals[MAX_THREADS];
	int 			failures = 0;
	

	/*Initialize pthread attributes*/
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 65536);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	/*Create N number of pthreads that will make a GET request*/
	printf("Creating test threads\n");
	for(thread_ctr = 0; thread_ctr < MAX_THREADS; thread_ctr++){
		int ret = pthread_create(
			&(tids[thread_ctr]),
			&attr,
			http_client,
			NULL
		);

		if(ret != 0) {
			perror("pthread_create");
		}
	}
	/*Wait for all pthreads to finish*/
	printf("Waiting for thread return codes\n");

	for(thread_ctr = 0; thread_ctr < MAX_THREADS; thread_ctr++){
		uintptr_t status;
        
        pthread_join(tids[thread_ctr], (void **)&status);
        retvals[thread_ctr] = status;

        if(status != STATUS_SUCCESS)
        {
            failures++;
        }
	}

	printf("Number of failures: %d\n", failures);
	/*Clean up*/

	return 0;
}


void *http_client(void* arg) {
	char           			in_buf[BUF_SIZE];
	int   			        server_s;
	struct sockaddr_in    	server_addr;
	int 					bytes_recieved;
	struct linger 			lg;
	struct timeval 			tv;
	int                     fail;
	int bytes_total = 0;

	bytes_recieved = 0;
	/* 30 Secs Timeout */
	tv.tv_sec = 3;
	tv.tv_usec = 0;
	fail = 0;

	lg.l_onoff = 1;
	lg.l_linger = 3;

	/*Initialize socket*/
	server_s = socket(AF_INET, SOCK_STREAM, 0);

	setsockopt(server_s, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));
	setsockopt(server_s, SOL_SOCKET, SO_LINGER , (char *)&lg,sizeof(struct linger));
	/*Create server address structure*/

	bzero(&server_addr, sizeof(server_addr));

    server_addr.sin_family = 	AF_INET;
    server_addr.sin_addr.s_addr =   inet_addr("10.15.200.32");
    server_addr.sin_port = 		htons(SERVER_PORT);

    if(server_s < 0) {
    	perror("socket");
    } else {
    	/*Connect to server*/
	    if(connect(server_s, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 ) {
	    	perror("connect");
	    } else {
	    	/*Wait for response*/
			if (write(server_s, GetRequest, strlen(GetRequest)) < 0) {
				perror("write");
			} else {

				bytes_recieved = read(server_s, in_buf, BUF_SIZE);
				printf("Read %d bytes\n", bytes_recieved);
				if(bytes_recieved < 0) {
					perror("read");
					printf("%d\n", errno);
				} else if(bytes_recieved == 0) {
					printf("Socket has been closed\n");
				} else {
					in_buf[bytes_recieved] = '\0';
				}
			}

			/*Check response*/
			
	    }
    }

	/*Exit Thread*/
	close(server_s);

	if(bytes_recieved != 46) {
		return (void *)STATUS_FAILED;
	}

	return (void *)STATUS_SUCCESS;
}