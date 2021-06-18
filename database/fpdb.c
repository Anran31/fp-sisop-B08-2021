#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/time.h>
#define PORT 8080
#define PathMax 2048

char cwd[PathMax] = {0}, databasesPath[PathMax] = {0}, masterDatabasePath[PathMax] = {0};
char userListTablePath[PathMax] = {0}, userPermissionTablePath[PathMax] = {0};
int isRoot = 0;
char loggedUser[PathMax] = {0};
char currentDatabase[PathMax] = {0};
char dummyRead[3] = "ok";
char dummySend[3] = "ok";

void checkDatabasesFolder();
void checkMasterDatabase();
void checkUserListTable();
void checkUserPermissionTable();
void getFullCommand(char *fullCommand, char *lowerFullCommand[], char *realFullCommand[],int *i);
char* mainQuery(char *fullCommand);
void makeLower(char *from, char *to);
int tryCreateUser(char *lower, char *real);
int checkUserPassExist(char *userPass);
char* getUser(char *userPass);
int tryCreateDB(char *lower,char *real);
int tryGrantPermission(char *lower,char *real);
int checkUserExist(char *user);
int tryUse(char *lower,char *real);
int checkUserPermission(char *realDBName);
void makeLog(char *fullCommand);
int tryCreateTable(char *lower, char *real);
int tryDropTable(char *lower, char *real);
void *connection_handler(void *socket_desc);

int main() {
  //printf("mulai\n");
  pid_t pid, sid;        // Variabel untuk menyimpan PID

  pid = fork();     // Menyimpan PID dari Child Process

  /* Keluar saat fork gagal
  * (nilai variabel pid < 0) */
  if (pid < 0) {
    exit(EXIT_FAILURE);
  }

  /* Keluar saat fork berhasil
  * (nilai variabel pid adalah PID dari child process) */
  if (pid > 0) {
    exit(EXIT_SUCCESS);
  }

  umask(0);

  sid = setsid();
  if (sid < 0) {
    exit(EXIT_FAILURE);
  }

  getcwd(cwd,PathMax);
  if ((chdir(cwd)) < 0) {
    exit(EXIT_FAILURE);
  }
  //printf("after chdir\n");
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
  open("/dev/null", O_RDONLY);
  open("/dev/null", O_RDWR);
  open("/dev/null", O_RDWR);

  checkDatabasesFolder();
  checkMasterDatabase();
  checkUserListTable();
  checkUserPermissionTable();

  while (1) {
      printf("tes\n");
      int server_fd, new_socket, valread;
      struct sockaddr_in address;
      int opt = 1;
      int addrlen = sizeof(address);
      char buffer[1024] = {0};
    
        
      if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
          perror("socket failed");
          exit(EXIT_FAILURE);
      }
      printf("tes socket\n");
      if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
          perror("setsockopt");
          exit(EXIT_FAILURE);
      }
      printf("tes set sockopt\n");
      address.sin_family = AF_INET;
      address.sin_addr.s_addr = INADDR_ANY;
      address.sin_port = htons( PORT );
        
      if (bind(server_fd, (struct sockaddr *)&address, sizeof(address))<0) {
          perror("bind failed");
          exit(EXIT_FAILURE);
      }
      printf("tes bind\n");
      if (listen(server_fd, 3) < 0) {
          perror("listen");
          exit(EXIT_FAILURE);
      }
      printf("tes listen\n");
      // Tulis program kalian di sini
      while ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)))
      {       
              //printf("tes dalam while accept\n");
              int *new_sock;
              pthread_t sniffer_thread;
              new_sock = malloc(sizeof(int));
              *new_sock = new_socket;
              
              //printf("new_sock = %d\n", *new_sock);
              sniffer_thread = pthread_create( &sniffer_thread , NULL ,  connection_handler , (void*) new_sock);
              if( sniffer_thread < 0)
              {
                  perror("could not create thread");
                  return 1;
              }
              
              //Now join the thread , so that we dont terminate before the thread
              pthread_join( sniffer_thread , NULL);
              //pthread_detach(sniffer_thread);
              //puts("Handler assigned");
      }
      
      if ((new_socket)<0) {
          perror("accept");
          exit(EXIT_FAILURE);
      }
  }
}



void *connection_handler(void *socket_desc)
{
    //puts("In Connection Handler");
    //Get the socket descriptor
    int sock = *(int*)socket_desc;
    int read_size;

    memset(loggedUser,0,PathMax);
    memset(currentDatabase,0,PathMax);
    char fullCommand[2048] = {0};

    char cursor[PathMax] = {0};
    strcpy(currentDatabase,"(none)");
    sprintf(cursor,"fpdb[%s]>> ",currentDatabase);

    memset(fullCommand,0,sizeof(fullCommand));

    //FILE *f = fopen("/home/anran/kuliah/sisop/fp/client/test.txt", "a");


    read(sock,&isRoot,sizeof(isRoot));
    send(sock, dummySend, 3, 0);
    //fprintf(f, "is root = %d\n", isRoot);

    if(!isRoot)
    {
        char userPass[2048] = {0};
        read_size = read(sock , userPass, 2048);
        //fprintf(f, "userPass = %s", userPass);
        
        if(checkUserPassExist(userPass))
        {
          char *Acc = "Login Success";
          send(sock,Acc,strlen(Acc),0);
          read(sock , dummyRead , 3);

          strcpy(loggedUser,getUser(userPass));
        }
        else{
          char *Acc = "Invalid Username/Password";
          send(sock,Acc,strlen(Acc),0);
          //fclose(f);
          fflush(stdout);
          free(socket_desc);
	
	        return 0;
        }
    }
    else strcpy(loggedUser,"root");

    //fprintf(f,"loggedUser = %s\n",loggedUser);

    send(sock , cursor , strlen(cursor),0);
    while((read_size = read(sock , fullCommand , 2048))>0)
    {
        //fprintf(f, "%s\n", fullCommand);
        //fprintf(f,"Last char = %c\n",fullCommand[strlen(fullCommand)-1]);
        //printf("full command = %s\n",fullCommand);
        char status[PathMax] = {0};

        strcpy(status,mainQuery(fullCommand));

        if(strcmp(status,"Bye\n") != 0)
        {
          sprintf(cursor,"%s\nfpdb[%s]>> ",status,currentDatabase);
          send(sock,cursor,strlen(cursor),0);
        }
        else
        {
          send(sock,status,strlen(status),0);
          read_size = 0;
          break;
        }
        memset(fullCommand,0,sizeof(fullCommand));
    }
    if(read_size == 0)
    {
      //fclose(f);
      puts("Client disconnected");
      fflush(stdout);
    }
    else if(read_size == -1)
    {
      //fclose(f);
      perror("recv failed");
    }
		
	//Free the socket pointer
	free(socket_desc);
	
	return 0;
}

void checkDatabasesFolder()
{
  sprintf(databasesPath,"%s/databases",cwd);
  DIR * dir = opendir(databasesPath);
  if(dir == NULL)
  {
    mkdir(databasesPath,0777);
    //FILE *f = fopen("/home/anran/kuliah/sisop/fp/client/makeDatabases.txt", "a");
    //fprintf(f, "%s\n", databasesPath);
    //fclose(f);
  }
}

void checkMasterDatabase()
{
  sprintf(masterDatabasePath,"%s/fpdb",databasesPath);
  DIR * dir = opendir(masterDatabasePath);
  if(dir == NULL)
  {
    mkdir(masterDatabasePath,0777);
    //FILE *f = fopen("/home/anran/kuliah/sisop/fp/client/makeMasterDB.txt", "a");
    //fprintf(f, "%s\n", masterDatabasePath);
    //fclose(f);
  }
}

void checkUserListTable()
{
  sprintf(userListTablePath,"%s/userList",masterDatabasePath);
  FILE *f = fopen(userListTablePath,"a");
  fclose(f);
}

void checkUserPermissionTable()
{
  sprintf(userPermissionTablePath,"%s/userPermission",masterDatabasePath);
  FILE *f = fopen(userPermissionTablePath,"a");
  fclose(f);
}

char* mainQuery(char *fullCommand)
{
    char retMessage[PathMax] = {0};
    char *ret;
    int StatVal = 0;
    if(fullCommand[strlen(fullCommand)-1] != ';') 
    {
      strcpy(retMessage,"Invalid Syntax (need ';' at end of query)");
      ret = retMessage;
      return ret;
    }
    else
    {
      char lowerFullCommand[PathMax] = {0}, copyFullCommand[PathMax] = {0};
      
      strcpy(copyFullCommand,fullCommand);
      makeLower(fullCommand,lowerFullCommand);

      if(copyFullCommand && lowerFullCommand)
      {
        // FILE *f = fopen(userListTablePath,"a");
        // fprintf(f,"REAL = %s\nFULL = %s\nLOWER = %s\n",fullCommand,copyFullCommand,lowerFullCommand);
        // fclose(f);
        if(!strncmp(lowerFullCommand,"create user ",12))
        {
            StatVal = tryCreateUser(lowerFullCommand,copyFullCommand);
            if(StatVal == 1) strcpy(retMessage,"User Created");
            else if(StatVal == 2) strcpy(retMessage,"User Already Existed");
        }
        else if(!strncmp(lowerFullCommand,"create database ",16))
        {
            StatVal = tryCreateDB(lowerFullCommand,copyFullCommand);
            if(StatVal == 1) strcpy(retMessage,"Database Created");
            else if(StatVal == 2) strcpy(retMessage,"Database Already Existed");
        }
        else if(!strncmp(lowerFullCommand,"grant permission ",17))
        {
            StatVal = tryGrantPermission(lowerFullCommand,copyFullCommand);
            if(StatVal == 1) strcpy(retMessage,"Permission Granted");
            else if(StatVal == 2) strcpy(retMessage,"User Didn't Exist");
            else if(StatVal == 3) strcpy(retMessage,"Database Didn't Exist");
        }
        else if(!strncmp(lowerFullCommand,"use ",4))
        {
            StatVal = tryUse(lowerFullCommand,copyFullCommand);
            if(StatVal == 1) strcpy(retMessage,"Change Working Database");
            else if(StatVal == 2) strcpy(retMessage,"Database Didn't Exist");
        }
        else if(!strncmp(lowerFullCommand,"create table ",13))
        {
            StatVal = tryCreateTable(lowerFullCommand,copyFullCommand);
            if(StatVal == 1) strcpy(retMessage,"Table Created");
            else if(StatVal == 2) strcpy(retMessage,"Table Already Exist");
            else if(StatVal == 3) strcpy(retMessage,"Must Use A Database First");
        }
        else if(!strncmp(lowerFullCommand,"drop table ",11))
        {
            StatVal = tryDropTable(lowerFullCommand,copyFullCommand);
            if(StatVal == 1) strcpy(retMessage,"Table Dropped");
            else if(StatVal == 2) strcpy(retMessage,"Table Didn't Exist");
            else if(StatVal == 3) strcpy(retMessage,"Must Use A Database First");
        }
        else if(!strncmp(lowerFullCommand,"exit",4))
        {
          StatVal = 100;
          strcpy(retMessage,"Bye\n");
        }
      }

      if(StatVal == 1) makeLog(fullCommand);
      if(StatVal == 0)
      {
        strcpy(retMessage,"Invalid Syntax");
      }
      else if (StatVal == -1)
      {
        strcpy(retMessage,"Permission Denied");
      }
      ret = retMessage;
      return ret;
    }
}

int tryCreateUser(char *lower, char *real)
{
  if(!isRoot) return -1;
  char user[PathMax] = {0}, pass[PathMax] = {0}; 

  char copyLower[PathMax] ={0};
  strcpy(copyLower,lower);

  char copyReal[PathMax] ={0};
  strcpy(copyReal,real);

  int index = 0;

  const char *p=" ";
  char *a,*b;
  for( a=strtok_r(copyLower,p,&b) ; a!=NULL ; a=strtok_r(NULL,p,&b) )
  {
    int indexC = 0;
    strcpy(copyReal,real);
    char  *c, *d;
    for(c=strtok_r(copyReal,p,&d); c!=NULL ; c=strtok_r(NULL,p,&d))
    {
      if(indexC == index) break;
      indexC++;
    }

    if(index == 2) strcpy(user,c);

    if(index == 3 && strcmp(a,"identified") != 0) return 0;

    if(index == 4 && strcmp(a,"by") != 0) return 0;

    if(index == 5 && c[strlen(c)-1] == ';') strncpy(pass,c,strlen(c));
    else if (index == 5)strcpy(pass,c);

    if(index == 6 && strcmp(c,";") != 0) return 0;
    
    // fprintf(f,"index = %d\na = %s strlen = %d\nc = %s strlen = %d\n",index,a,strlen(a),c,strlen(c));
    // if(index == 5) fprintf(f,"user = %s pass = %s\n",user,pass);
    // fclose(f);
    // FILE *f = fopen(userListTablePath,"a");
    index++;
  }

  char userPass[PathMax] = {0};

  char realPass[PathMax] = {0};
  if(pass[strlen(pass)-1] == ';') strncpy(realPass,pass,strlen(pass)-1);
  else strcpy(realPass,pass);

  sprintf(userPass,"%s:%s\n",user,realPass);
  if(checkUserPassExist(userPass)) return 2;
  else{
    FILE *f = fopen(userListTablePath,"a");
    fprintf(f,"%s",userPass);
    fclose(f);
    return 1;
  }
}

void makeLower(char *from, char *to)
{
  for(int i = 0; i<strlen(from);i++)
  {
    to[i] = tolower(from[i]);
  }
}

int checkUserPassExist(char *userPass)
{
    FILE * fPtr;

    fPtr = fopen(userListTablePath, "r");

    char *creds;
    size_t len = 0;
    while ((getline(&creds, &len, fPtr)) != -1) {
        if(!strcmp(creds,userPass))
        {
          fclose(fPtr);
          return 1;
        }
    }

    fclose(fPtr);
    return 0;
}

char* getUser(char *userPass)
{
  char retMessage[PathMax] = {0};
  char *ret;

  const char *p=":";
  char *a,*b;
  for( a=strtok_r(userPass,p,&b) ; a!=NULL ; a=strtok_r(NULL,p,&b) )
  {
    strcpy(retMessage,a);
    ret = retMessage;
    return ret;
  }
}

int tryCreateDB(char *lower,char *real)
{
  char dbName[PathMax] = {0};
  char copyLower[PathMax] ={0};
  strcpy(copyLower,lower);

  char copyReal[PathMax] ={0};
  strcpy(copyReal,real);

  int index = 0;

  const char *p=" ";
  char *a,*b;
  for( a=strtok_r(copyLower,p,&b) ; a!=NULL ; a=strtok_r(NULL,p,&b) )
  {
    int indexC = 0;
    strcpy(copyReal,real);
    char  *c, *d;
    for(c=strtok_r(copyReal,p,&d); c!=NULL ; c=strtok_r(NULL,p,&d))
    {
      if(indexC == index) break;
      indexC++;
    }

    if(index == 2) strcpy(dbName,c);

    if(index == 3 && strcmp(c,";") != 0) return 0;
    index++;
  }

  char realDBName[PathMax] = {0};
  if(dbName[strlen(dbName)-1] == ';') strncpy(realDBName,dbName,strlen(dbName)-1);
  else strcpy(realDBName,dbName);

  char dbPath[PathMax] = {0};
  sprintf(dbPath,"%s/%s",databasesPath,realDBName);
  // FILE *f = fopen(userPermissionTablePath,"a");
  // fprintf(f,"DBpATH = %s\n",dbPath);
  // fclose(f);
  if(access(dbPath,F_OK) != -1) return 2;
  else {
    mkdir(dbPath,0777);

    char userDB[PathMax] = {0};
    sprintf(userDB,"%s:%s",loggedUser,realDBName);

    FILE *f = fopen(userPermissionTablePath,"a");
    fprintf(f,"%s\n",userDB);
    fclose(f);
    return 1;
  } 
}

int tryGrantPermission(char *lower,char *real)
{
  if(!isRoot) return -1;
  char dbName[PathMax] = {0}, username[PathMax] = {0};
  char copyLower[PathMax] ={0};
  strcpy(copyLower,lower);

  char copyReal[PathMax] ={0};
  strcpy(copyReal,real);

  int index = 0;

  const char *p=" ";
  char *a,*b;
  for( a=strtok_r(copyLower,p,&b) ; a!=NULL ; a=strtok_r(NULL,p,&b) )
  {
    int indexC = 0;
    strcpy(copyReal,real);
    char  *c, *d;
    for(c=strtok_r(copyReal,p,&d); c!=NULL ; c=strtok_r(NULL,p,&d))
    {
      if(indexC == index) break;
      indexC++;
    }

    if(index == 2) strcpy(dbName,c);

    if(index == 3 && strcmp(a,"into") != 0) return 0;

    if(index == 4) strcpy(username,c);

    if(index == 5 && strcmp(c,";") != 0) return 0;
    index++;
  }

  char realUname[PathMax] = {0};
  if(username[strlen(username)-1] == ';') strncpy(realUname,username,strlen(username)-1);
  else strcpy(realUname,username);

  if(!checkUserExist(realUname)) return 2;

  char dbPath[PathMax] = {0};
  sprintf(dbPath,"%s/%s",databasesPath,dbName);
  // FILE *f = fopen(userPermissionTablePath,"a");
  // fprintf(f,"DBpATH = %s\n",dbPath);
  // fclose(f);
  if(access(dbPath,F_OK) == -1) return 3;
  else {
    char userDB[PathMax] = {0};
    sprintf(userDB,"%s:%s",realUname,dbName);

    FILE *f = fopen(userPermissionTablePath,"a");
    fprintf(f,"%s\n",userDB);
    fclose(f);

    return 1;
  } 
}

int checkUserExist(char *user)
{
    char copyUser[PathMax] = {0};
    strcpy(copyUser,user);
    strcat(copyUser,":");

    FILE * fPtr;

    fPtr = fopen(userListTablePath, "r");

    char *creds;
    size_t len = 0;
    while ((getline(&creds, &len, fPtr)) != -1) {
        if(!strncmp(creds,copyUser,strlen(copyUser)))
        {
          fclose(fPtr);
          return 1;
        }
    }

    fclose(fPtr);
    return 0;
}

int tryUse(char *lower,char *real)
{
  char dbName[PathMax] = {0};
  char copyLower[PathMax] ={0};
  strcpy(copyLower,lower);

  char copyReal[PathMax] ={0};
  strcpy(copyReal,real);

  int index = 0;

  const char *p=" ";
  char *a,*b;
  for( a=strtok_r(copyLower,p,&b) ; a!=NULL ; a=strtok_r(NULL,p,&b) )
  {
    int indexC = 0;
    strcpy(copyReal,real);
    char  *c, *d;
    for(c=strtok_r(copyReal,p,&d); c!=NULL ; c=strtok_r(NULL,p,&d))
    {
      if(indexC == index) break;
      indexC++;
    }

    if(index == 1) strcpy(dbName,c);

    if(index == 2 && strcmp(c,";") != 0) return 0;
    index++;
  }

  char realDBName[PathMax] = {0};
  if(dbName[strlen(dbName)-1] == ';') strncpy(realDBName,dbName,strlen(dbName)-1);
  else strcpy(realDBName,dbName);

  char dbPath[PathMax] = {0};
  sprintf(dbPath,"%s/%s",databasesPath,realDBName);
  // FILE *f = fopen(userPermissionTablePath,"a");
  // fprintf(f,"DBpATH = %s\n",dbPath);
  // fclose(f);
  if(access(dbPath,F_OK) == -1) return 2;
  else {
    if(isRoot)
    {
      memset(currentDatabase,0,PathMax);
      strcpy(currentDatabase,realDBName);
      return 1;
    }
    else if(checkUserPermission(realDBName))
    {
      memset(currentDatabase,0,PathMax);
      strcpy(currentDatabase,realDBName);
      return 1;
    }
    else return -1;
  } 
}

int checkUserPermission(char *realDBName)
{

    char userTable[PathMax] = {0};

    sprintf(userTable,"%s:%s\n",loggedUser,realDBName);
    FILE * fPtr;
    fPtr = fopen(userPermissionTablePath, "r");

    // FILE *f;
    // f = fopen("/home/anran/kuliah/sisop/fp/server/userPermission.log","a");

    char *creds;
    size_t len = 0;
    while ((getline(&creds, &len, fPtr)) != -1) {
        if(!strcmp(creds,userTable))
        {
          fclose(fPtr);
          return 1;
        }
    }

    // fclose(f);
    fclose(fPtr);
    return 0;
}

void makeLog(char *fullCommand)
{
  char logPath[PathMax] = {0};
  sprintf(logPath,"%s/fpdb.log",databasesPath);

  FILE *f = fopen(logPath,"a");
  time_t now;
	time ( &now );
	struct tm * timeinfo = localtime (&now);

  char command[PathMax] = {0};
  strcpy(command,fullCommand);
  command[strcspn(command,";")] = '\n';
	fprintf(f, "%04d-%02d-%02d %02d:%02d:%02d:%s:%s",timeinfo->tm_year+1900,timeinfo->tm_mon+1,timeinfo->tm_mday,timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec,loggedUser,command);
  fclose(f);
}

int tryCreateTable(char *lower, char *real)
{
  if(!strcmp(currentDatabase,"(none)")) return 3;

  char tableName[PathMax] = {0};
  char copyLower[PathMax] ={0};
  strcpy(copyLower,lower);

  char copyReal[PathMax] ={0};
  strcpy(copyReal,real);

  int index = 0;

  const char *p=" ";
  char *a,*b;
  for( a=strtok_r(copyLower,p,&b) ; a!=NULL ; a=strtok_r(NULL,p,&b) )
  {
    int indexC = 0;
    strcpy(copyReal,real);
    char  *c, *d;
    for(c=strtok_r(copyReal,p,&d); c!=NULL ; c=strtok_r(NULL,p,&d))
    {
      if(indexC == index) break;
      indexC++;
    }

    if(index == 2) 
    {
      strcpy(tableName,c);
      break;
    }

    index++;
  }

  char tablePath[PathMax] = {0};
  sprintf(tablePath,"%s/%s/%s",databasesPath,currentDatabase,tableName);

  char *openParen = strchr(lower,'(');
  char *closeParen = strchr(openParen,')');
  if(openParen == NULL || closeParen == NULL) return 0;

  if(access(tablePath,F_OK) != -1) return 2;
  else{
    FILE *f = fopen(tablePath,"a");
    fclose(f);
    return 1;
  }
}

int tryDropTable(char *lower, char *real)
{
  if(!strcmp(currentDatabase,"(none)")) return 3;

  char tableName[PathMax] = {0};
  char copyLower[PathMax] ={0};
  strcpy(copyLower,lower);

  char copyReal[PathMax] ={0};
  strcpy(copyReal,real);

  int index = 0;

  const char *p=" ";
  char *a,*b;
  for( a=strtok_r(copyLower,p,&b) ; a!=NULL ; a=strtok_r(NULL,p,&b) )
  {
    int indexC = 0;
    strcpy(copyReal,real);
    char  *c, *d;
    for(c=strtok_r(copyReal,p,&d); c!=NULL ; c=strtok_r(NULL,p,&d))
    {
      if(indexC == index) break;
      indexC++;
    }

    if(index == 2) 
    {
      strcpy(tableName,c);
    }

    if(index == 3 && strcmp(c,";") != 0) return 0;
    index++;
  }

  char realTableName[PathMax] = {0};
  if(tableName[strlen(tableName)-1] == ';') strncpy(realTableName,tableName,strlen(tableName)-1);
  else strcpy(realTableName,tableName);

  char tablePath[PathMax] = {0};
  sprintf(tablePath,"%s/%s/%s",databasesPath,currentDatabase,realTableName);

  if(access(tablePath,F_OK) == -1) return 2;
  else {
    unlink(tablePath);
    return 1;
  } 
}
