#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#define PORT 8080


char dummyRead[3] = "ok";
char dummySend[3] = "ok";

int main(int argc, char const *argv[]) {
    struct sockaddr_in address;
    int sock = 0, valread;
    struct sockaddr_in serv_addr;
    char *hello = "Hello from client";
    char buffer[2048] = {0};
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }
  
    memset(&serv_addr, '0', sizeof(serv_addr));
  
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
      
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }
  
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    char command[2048];
    int isRoot=0;

    if(getuid() == 0) isRoot = 1;

    //printf("uid = %d isRoot = %d\n",getuid(), isRoot);

    send(sock,&isRoot,sizeof(isRoot),0);
    read(sock , dummyRead , 3);

    if(!isRoot)
    {
        char user[1024] = {0}, pass[1024] = {0}, userPass[2048] = {0};
        int username = 0, password = 0; 
        if(argc != 5) 
        {
            printf("Invalid Arguments, -u [username] -p [password]\n");
            return 0;
        }

        for(int i = 0; i<argc; i++)
        {
            //printf("%s\n",argv[i]);
            if(!strcmp(argv[i], "-u") && strncmp(argv[i+1],"-",1)) 
            {
                //printf("username masuk\n");
                strcpy(user,argv[i+1]);
                username++;
            }
            if(!strcmp(argv[i], "-p") && strncmp(argv[i+1],"-",1))
            {
                //printf("password masuk\n");
                strcpy(pass,argv[i+1]);
                password++;
            } 
        }

        if(username && password)
        {
            sprintf(userPass,"%s:%s\n",user,pass);
            
            send(sock , userPass, strlen(userPass),0);
            read( sock , buffer, 2048);
            printf("%s\n",buffer );
            if(!strcmp(buffer,"Invalid Username/Password")) return 0;
            memset(buffer,0,2048);
            send(sock, dummySend, strlen(dummySend), 0);
        }
        else{
            printf("Invalid Arguments, -u [username] -p [password]\n");
            return 0;
        }
    }

    while (1)
    {
        valread = read( sock , buffer, 2048);
        printf("%s",buffer );
        if(!strcmp(buffer,"Bye\n")) 
        {
            memset(buffer,0,2048);
            break;
        }
        memset(buffer,0,2048);

        char fullCommand[2048] = {0},tempCommand[2048] = {0}, file[1024]={0};
        memset(fullCommand,0,sizeof(fullCommand));
        scanf(" %[^\n]",fullCommand);
        strcpy(tempCommand,fullCommand);

        //printf("full command = %s\n",fullCommand);
        //scanf("%s",command);
        send(sock , fullCommand, strlen(fullCommand) , 0 );
        memset(buffer,0,2048);
        memset(fullCommand,0,2048);
    }
    
    return 0;
}
