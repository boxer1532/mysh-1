#define SOCK_PATH "tpf_unix_sock.server"
#define SERVER_PATH "tpf_unix_sock.server"
#define CLIENT_PATH "tpf_unix_sock.client"

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <pthread.h>

#include "commands.h"
#include "built_in.h"
#include "signal_handlers.h"

static struct built_in_command built_in_commands[] = {
  { "cd", do_cd, validate_cd_argv },
  { "pwd", do_pwd, validate_pwd_argv },
  { "fg", do_fg, validate_fg_argv }
};


static int is_built_in_command(const char* command_name)
{
  static const int n_built_in_commands = sizeof(built_in_commands) / sizeof(built_in_commands[0]);

  for (int i = 0; i < n_built_in_commands; ++i) {
    if (strcmp(command_name, built_in_commands[i].command_name) == 0) {
      return i;
    }
  }

  return -1; // Not found
}

void* c_function(struct single_command (*c1)[512]){
  
  int client_sock, len, fd;
  struct single_command* com = (*c1);

  struct sockaddr_un server_addr;
  struct sockaddr_un client_addr;

  memset(&server_addr, 0, sizeof(struct sockaddr_un));
  memset(&client_addr, 0, sizeof(struct sockaddr_un));
   
  client_sock = socket(AF_UNIX, SOCK_STREAM, 0);

  if(client_sock == -1){
      printf("SOCKET ERROR\n");
      exit(1);
  }

  client_addr.sun_family = AF_UNIX;
  strcpy(client_addr.sun_path, CLIENT_PATH);
  len = sizeof(client_addr);

  unlink(CLIENT_PATH);

  if(bind(client_sock, (struct sockaddr *)&client_addr, len) == -1){
      printf("BIND ERROR\n");
      close(client_sock);
      exit(1);
  }
  
  server_addr.sun_family = AF_UNIX;
  strcpy(server_addr.sun_path, SERVER_PATH);

  if(connect(client_sock, (struct sockaddr *)&server_addr, len) == -1){
      printf("CONNECTION ERROR\n");
      close(client_sock);
      exit(1);
  }
 
  int saveout = dup(STDOUT_FILENO);
  dup2(client_sock, STDOUT_FILENO);
  close(client_sock);

  evaluate_command(1, c1);

  close(STDOUT_FILENO);
  dup2(saveout, STDOUT_FILENO);
  close(saveout);

  close(client_sock);
  
  pthread_exit(NULL);
}
 

/*
 * description: Currently this function only handles single built_in commands. You should modify this structure to launch process and offer pipeline functionality.
 */
int evaluate_command(int n_commands, struct single_command (*commands)[512])
{
  if(n_commands >= 2){
    struct single_command* incom = (*commands);
    struct single_command* outcom = &((*commands)[1]);
   
    struct single_command c1[512];
    struct single_command c2[512];
    
    memcpy(&c1[0], incom, sizeof(struct single_command));
    memcpy(&c2[0], outcom, sizeof(struct single_command)); 

    void *status;
    int len;
    int server_sock, client_sock;
    int infd, outfd;
    struct sockaddr_un server_addr, client_addr;
    pthread_t Cthread;
  
    memset(&server_addr, 0, sizeof(struct sockaddr_un));
    memset(&client_addr, 0, sizeof(struct sockaddr_un));

    server_sock = socket(AF_UNIX, SOCK_STREAM, 0);

    if(server_sock == -1){
	printf("SOCKET ERROR!\n");
	exit(1);
    }
    
    server_addr.sun_family = AF_UNIX;
    strcpy(server_addr.sun_path, SOCK_PATH);
    len = sizeof(server_addr);

    unlink(SOCK_PATH);

    if(bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1){
	printf("BIND ERROR!\n");
	close(server_sock);
	exit(1);
    }

    int thr_id = pthread_create(&Cthread, NULL, (void*)c_function, &c1);

    if(thr_id < 0){
	printf("THREAD ERROR!\n");
	exit(1);
    }

  
    if(listen(server_sock, 5) == -1){
	printf("LISTEN ERROR!\n");
	close(server_sock);
	exit(1);
    }
    
   client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &len);

   if(client_sock == -1){
       printf("ACCEPT ERROR!\n");
       close(server_sock);
       close(client_sock);
       exit(1);
   }

   pthread_join(Cthread, &status);

   int savein = dup(STDIN_FILENO);
   dup2(client_sock, STDIN_FILENO);
   close(client_sock);

   evaluate_command(1, &c2);
   
   close(STDIN_FILENO);

   dup2(savein, STDIN_FILENO);
   close(savein);

   close(server_sock);

   return 0;
  }

  
  else if (n_commands > 0) { 
    struct single_command* com = (*commands);

    assert(com->argc != 0);

    int built_in_pos = is_built_in_command(com->argv[0]);
    if (built_in_pos != -1) {
      if (built_in_commands[built_in_pos].command_validate(com->argc, com->argv)) {
        if (built_in_commands[built_in_pos].command_do(com->argc, com->argv) != 0) {
          fprintf(stderr, "%s: Error occurs\n", com->argv[0]);
        }
      } else {
        fprintf(stderr, "%s: Invalid arguments\n", com->argv[0]);
        return -1;
      }
    } else if (strcmp(com->argv[0], "") == 0) {
      return 0;
    } else if (strcmp(com->argv[0], "exit") == 0) {
      return 1;
    } else {
      int pid;
      int status;
      
      pid = fork();

      if(pid == 0){
        
        char coms[256];
	strcpy(coms, com->argv[0]);

	if(execv(com->argv[0], com->argv) == -1){
            strcpy(com->argv[0], "/usr/local/bin/");
	    strcat(com->argv[0], coms);
	    if(execv(com->argv[0], com->argv) == -1){
              strcpy(com->argv[0], "/usr/bin/");
	      strcat(com->argv[0], coms);
	      if(execv(com->argv[0], com->argv) == -1){
                 strcpy(com->argv[0], "/bin/");
	         strcat(com->argv[0], coms);
	         if(execv(com->argv[0], com->argv) == -1){
		    strcpy(com->argv[0], "/usr/sbin/");
	            strcat(com->argv[0], coms);
	            if(execv(com->argv[0], com->argv) == -1){
                       strcpy(com->argv[0], "/sbin/");
	               strcat(com->argv[0], coms);
	              if(execv(com->argv[0], com->argv) == -1){
			 printf("Invalid Input!\n");
			 exit(1);
	              }
	            }
	         }
	      }
	    }
	}
      }	
      else{
          wait(&status);
	 
      return 0;
      }
    }
  }

  return 0;
}

void free_commands(int n_commands, struct single_command (*commands)[512])
{
  for (int i = 0; i < n_commands; ++i) {
    struct single_command *com = (*commands) + i;
    int argc = com->argc;
    char** argv = com->argv;

    for (int j = 0; j < argc; ++j) {
	free(argv[j]);
    }

    free(argv);
  }

  memset((*commands), 0, sizeof(struct single_command) * n_commands);
}
