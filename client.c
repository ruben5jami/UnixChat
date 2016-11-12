#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<netdb.h>
#include<fcntl.h>	
#include<dirent.h>				
#include<sys/types.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<sys/stat.h>				
#include<sys/mman.h>
#define PORT "6667" 				// the port client will be connecting to
#define MAXDATASIZE 512 			// max number of bytes we can get at once

void *get_in_addr(struct sockaddr *sa)		//get sockaddr, IPv4 or IPv6:
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int sockfd, numbytes;
	int bytesent;
	int sizeofile;
	int filesize;
	int byterecived;
	int offSet = 0;
	fd_set master;				// master file descriptor list
	int fdmax;
	int k, j, fd;			
	char buf[MAXDATASIZE];
	char msg[MAXDATASIZE];
	char nickname[10];
	char uploadFile[MAXDATASIZE];
	char downloadFile[MAXDATASIZE];
	char downfilename[MAXDATASIZE];
	char download[MAXDATASIZE];
	void *pmap;
	struct stat mystat;			
	struct stat statbuffer;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	
	char s[INET6_ADDRSTRLEN];
	if (argc != 1) {
		fprintf(stderr,"usage: client hostname\n");
		exit(1);
	}
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if ((rv = getaddrinfo("127.0.0.1", PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype,
		servinfo->ai_protocol)) == -1) {
		perror("client: socket");
	}
	if (connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
		close(sockfd);
		perror("client: connect");
	}

	inet_ntop(servinfo->ai_family, get_in_addr((struct sockaddr *)servinfo->ai_addr),
	s, sizeof s);
	printf("client: connecting to %s\n", s);
	printf("----------------Welcome to the chat----------------\n");
	printf("Please choose a nickname\n");
	scanf("%s", nickname);
	send(sockfd, nickname, sizeof nickname, 0);			//first login send the nickname
	freeaddrinfo(servinfo); 
	
	
	struct timeval tv;
	fd_set readfds;
	
	while(strcmp("/exit",buf)){
		tv.tv_sec = 6000;
		tv.tv_usec = 500000;
		FD_ZERO(&readfds);
		FD_SET(0,&readfds);
		FD_SET(sockfd,&readfds);
		if (select(sockfd+1, &readfds, NULL, NULL, &tv) == -1) {
				perror("select");
				exit(4);
		}
					
		if (FD_ISSET(sockfd, &readfds)) { 			
			int nbytes = recv(sockfd, buf, MAXDATASIZE, 0);
			if(nbytes == -1){
				perror("recv:");
				return 0;
			}
			buf[nbytes] = '\0';
			puts(buf);
		}
		else if (FD_ISSET(0, &readfds)) {
			fgets(msg,sizeof msg, stdin);
			msg[strlen(msg)-1] = '\0';
			if(strcmp("/exit",msg) == 0){
				strcpy(msg, "User has left");
				send(sockfd, msg, MAXDATASIZE, 0);
				exit(5);
			}
			if(strncmp("/nick", msg, strlen("/nick"))==0){
				send(sockfd, msg, MAXDATASIZE, 0);
			}
			else{
				send(sockfd, msg, MAXDATASIZE, 0);
			}
			if(strncmp("/upload", msg, strlen("/upload"))==0){
				for(k=8, j=0; k<strlen(msg); k++, j++){			//transfer from buf to uploadfile
					uploadFile[j] = msg[k];
					if(k == (strlen(msg)-1)){
						uploadFile[j+1] = '\0'; 
						printf("File is uploading: %s\n", uploadFile);
					}
				}
				fd = open(uploadFile, O_RDWR);					//mapping
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
				send(sockfd, (char*)&filesize, sizeof (int), 0);
				while(   (bytesent = (send(sockfd, pmap, MAXDATASIZE, 0))) > 0  &&  (filesize > 0)  ){	//send MAXDATASIZE on each loop
					filesize = filesize - bytesent;
					pmap = pmap + bytesent;																//continue from last place
				}
				close(fd);
				munmap(0, statbuffer.st_size);		
			}

			if(strncmp("/download", msg, strlen("/download"))==0){				
				send(sockfd, msg, MAXDATASIZE, 0);
				for(k=10, j=0; k<strlen(msg); k++, j++){			//transfer from buf to uploadfile
					downfilename[j] = msg[k];
					if(k == (strlen(msg)-1)){
						downfilename[j+1] = '\0'; 
						printf("File is downloading: %s\n", downfilename);
					}
				}
				strcpy(downloadFile, "download/");
				strcat(downloadFile, downfilename);					//the name of file with partial path name
				printf("%s\n", downloadFile);
				recv(sockfd, (char*)&sizeofile, sizeof (int), 0);
				filesize = sizeofile;
				printf("this is filesize: %d\n", filesize);
				FILE* fp = fopen(downloadFile , "w");
				if(fp==NULL)
					printf("error closing fp\n");
				download[0] = '\0';
				if(filesize > 1024){
					while (((byterecived = recv(sockfd, download, MAXDATASIZE, 0))>0)&&(filesize>0)){
						filesize = filesize - byterecived;
						fwrite(download, sizeof (char), byterecived, fp);
					}
				}
				if(filesize <= 1024){
					while (((byterecived = recv(sockfd, download, MAXDATASIZE, 0))>0)&&(filesize>0)){
						filesize = filesize - byterecived;
						fprintf(fp, "%s", download);
					}
				}
					fclose(fp);
					downloadFile[0] = '\0';							//initialize varible with file data	
			}															
		}

		else{
			printf("Time Out...\n");
		}	
	}
					
	close(sockfd);
	return 0;
}
