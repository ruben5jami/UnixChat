#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<netdb.h>
#include<fcntl.h>					
#include<sys/types.h>
#include<netinet/in.h>
#include<dirent.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<sys/stat.h>				
#include<sys/mman.h>
#define PORT "6667"							//port we're listening on
#define MAXDATASIZE 512 			// max number of bytes we can get at once

void *get_in_addr(struct sockaddr *sa)					//get sockaddr, IPv4 or IPv6:
{
if (sa->sa_family == AF_INET) {
	return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

	typedef struct nickls{						//struct for linked list (nickname)
		int socknum;
		char nickname[10];
	}nickls;

int main(void)
{
	struct dirent *filedirent;				
	DIR *filedir=NULL;						//for the ls command
	nickls arruser[10];						//array of users
	fd_set master;							// master file descriptor list
	fd_set read_fds;						// temp file descriptor list for select()
	int fdmax, fd;							// maximum file descriptor number
	int listener;							// listening socket descriptor
	int newfd;								// newly accept()ed socket descriptor
	struct sockaddr_storage remoteaddr;		// client address
	struct stat mystat;			
	struct stat statbuffer;
	void *pmap;
	int bytesent;
	int filesize;
	socklen_t addrlen;
	char buf[MAXDATASIZE];
	int flag=0;
	char nickname[10];
	char msg[MAXDATASIZE];
	char who[MAXDATASIZE];
	char uploadFile[MAXDATASIZE];
	char downfilename[MAXDATASIZE];
	char downloadFile[MAXDATASIZE];
	char upload[MAXDATASIZE];				//file name includeing path
	char download[MAXDATASIZE];
	char upfilename[MAXDATASIZE];
	int nbytes;								// buffer for client data
	int sizeofile;
	int byterecived;
	char remoteIP[INET6_ADDRSTRLEN];
	int yes=1;
	int i, j, rv, k;						// for setsockopt() SO_REUSEADDR, below
	struct addrinfo hints, *ai, *p;
	FD_ZERO(&master);						// clear the master and temp sets
	FD_ZERO(&read_fds);						// get us a socket and bind it
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
		fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
		exit(1);
	}
	for(p = ai; p != NULL; p = p->ai_next) {
		listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (listener < 0) {
			continue;						// lose the pesky "address already in use" error message
		}
		setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
		if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
		close(listener);
		continue;
		}
		break;								// if we got here, it means we didn't get bound
	}

	if (p == NULL) {
		fprintf(stderr, "selectserver: failed to bind\n");
		exit(2);
	}
	freeaddrinfo(ai); 						// all done with this
	if (listen(listener, 10) == -1) {
		perror("listen");
		exit(3);							// add the listener to the master set
	}
	FD_SET(listener, &master);				// keep track of the biggest file descriptor
	fdmax = listener; 						// so far, it's this one

	for(;;) {								// main loop
		read_fds = master; 					// copy it
		fflush(stdout);
		if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
			perror("select");
			exit(4);
		}
		flag = 0;
		for(i = 0; i <= fdmax; i++) {			// run through the existing connections looking for data to read
			if (FD_ISSET(i, &read_fds)) { 		// one connection
				if (i == listener) {			// handle new connections
					addrlen = sizeof remoteaddr;
					newfd = accept(listener,(struct sockaddr *)&remoteaddr,	&addrlen);
					if (newfd == -1) {
						perror("accept");
					}
					else 
					{
						FD_SET(newfd, &master); // add to master set
						if (newfd > fdmax) {	// keep track of the max
							fdmax = newfd;
						}
						printf("selectserver: new connection from %s on "
						"socket %d\n",
						inet_ntop(remoteaddr.ss_family,
						get_in_addr((struct sockaddr*)&remoteaddr),
						remoteIP, INET6_ADDRSTRLEN),newfd);
						
						recv(newfd, nickname,sizeof nickname, 0);
						if(newfd>14){
							printf("Too many clients\n");
							exit(1);
						}
						arruser[newfd-4].socknum = newfd;
						strcpy(arruser[newfd-4].nickname,  nickname);
						printf("%s has connected to the chat\n", arruser[newfd-4].nickname);
						
					}
				} 														//else to i  !=listenr
				else
				{								
					if ((nbytes = recv(i, buf, MAXDATASIZE, 0)) <= 0) {	// handle data from a client
																		// got error or connection closed by client
						if (nbytes == 0) {								// connection closed
							printf("selectserver: socket %d hung up\n", i);
						}
						else {
							perror("recv");
						}
						close(i); 										
						printf("%s\n", buf);
						FD_CLR(i, &master); 							// remove from master set
					} 
					else{
						if(buf[0] == '/'){								//enter the command code
							if(!strncmp("/nick", buf, strlen("/nick"))){
							printf("Nickname has been changed to: ");
								for(k=6, j=0; k<strlen(buf); k++, j++){			//transfer from buf to nickname
									nickname[j] = buf[k];
									if(k == (strlen(buf)-1)){
									nickname[j+1] = '\0'; 
									strcpy(arruser[i-4].nickname,  nickname);
									printf("%s\n",nickname);
									}
								}
								strcpy(msg, "--------your nickname has been changed----------\n");
								if (send(arruser[i-4].socknum, msg, MAXDATASIZE , 0) == -1) {
											perror("send");
								}
							}
							printf("%d ", strlen(buf));
							if(strcmp("/who", buf)==0){
								who[0] = '\0';
								strcat(who, "-------List of user connected--------\n");
								for(j=4; j <= fdmax ;j++){							//scan the array of user
									strcat(who, arruser[j-4].nickname);
									strcat(who, " \n");	
								}
								if (send(arruser[i-4].socknum, who, MAXDATASIZE , 0) == -1) {
									perror("send");
								}
							
							}
							if(strncmp("/upload", buf, strlen("/upload"))==0){
								for(k=8, j=0; k<strlen(buf); k++, j++){			//transfer from buf to uploadfile
									upfilename[j] = buf[k];
									if(k == (strlen(buf)-1)){
										upfilename[j+1] = '\0'; 				
										
									}
								}
								flag = 1;
								strcat(uploadFile, "upload/");
								strcat(uploadFile, upfilename);					//the name of file with partial path name
								recv(i, (char*)&sizeofile, sizeof (int), 0); 
								filesize = sizeofile;
								printf("%s\n", uploadFile);
								FILE* fp = fopen(uploadFile , "w");
								if(fp==NULL)
									printf("error closing fp\n");
								if(filesize > 1024){
									while (((byterecived = recv(i, upload, MAXDATASIZE, 0))>0)&&(filesize>0)){
										filesize = filesize - byterecived; 
										fwrite(upload, sizeof (char), byterecived, fp);
										printf("this is recive byte: %d\n" , byterecived);
										printf("this is file size: %d\n" , filesize);

									}
								}
								if(filesize <= 1024){
									while (((byterecived = recv(i, upload, MAXDATASIZE, 0))>0)&&(filesize>0)){
										filesize = filesize - byterecived;
										fprintf(fp, "%s", upload); 
										printf("this is recive byte: %d\n" , byterecived);
										printf("this is file size: %d\n" , filesize);

										
									}
								}						
							fclose(fp);
							printf("close fp");
							uploadFile[0] = '\0';
					
							}
							if(strncmp("/download", buf, strlen("/download"))==0){		
								for(k=10, j=0; k<strlen(buf); k++, j++){			//transfer from buf to downloadfile
									downfilename[j] = buf[k];
									if(k == (strlen(buf)-1)){
										downfilename[j+1] = '\0'; 					//the name of file with partial path name
									}
								}
								fd = open(downfilename, O_RDWR);					//mapping
								if(fd == -1){
									perror("open");
									exit(1);
								}
								if(fstat(fd, &mystat)<0){
									perror("fstat");
									close(fd);
									exit(1);
								}
								pmap = mmap(0, mystat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
								if(pmap == MAP_FAILED){
									perror("mmap");
									close(fd);
									exit(1);
								}
								if (fstat(fd, &statbuffer)<0){ 
									perror("fstat"); 
								}
								filesize = statbuffer.st_size;
								send(i, (char*)&filesize, sizeof (int), 0);
								while(   (bytesent = (send(i, pmap, MAXDATASIZE, 0))) > 0  &&  (filesize > 0)  ){
									filesize = filesize - bytesent;
									pmap = pmap + bytesent;				//continue from the same place after MAXDATASIZE
								}
								close(fd);
								munmap(0, statbuffer.st_size);
								
							}								
							if(strcmp("/ls", buf)==0){
								buf[0] = '\0';
									for (filedir=opendir("upload"), filedirent=readdir(filedir);
										NULL != filedirent;
										filedirent=readdir(filedir)){
										strcat(buf, "\n");
										strcat(buf, filedirent->d_name);
									}
							if (send(arruser[i-4].socknum, buf, MAXDATASIZE , 0) == -1) {
									perror("send");
								}
							}
						}
						else{											// we got some data from a client
							strcpy(msg, arruser[i-4].nickname);
							strcat(msg, ">");
							strcat(msg, buf);
							for(j = 0; j <= fdmax; j++) {				// send to everyone
								if (FD_ISSET(j, &master)) {
																		// except the listener and ourselves
									if (j != listener && j != i) {
										if (send(j, msg, MAXDATASIZE, 0) == -1) {
											perror("send");
										}
										else{
											printf("msg sent from: %d\n", j);
							
										}
									}								
								}
							}
						}
					}						// END handle data from client
				}			 				// END got new incoming connection
			}								// END looping through file descriptors
		}
	}										// END for(;;)--and you thought it would never end!
return 0;
}
