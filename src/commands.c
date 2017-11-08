#define FILE_SERVER "/tmp/test_server"

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

static struct built_in_command built_in_commands[] = {
  { "cd", do_cd, validate_cd_argv },
  { "pwd", do_pwd, validate_pwd_argv },
  { "fg", do_fg, validate_fg_argv }
};

struct sockaddr_in {
    unsigned short int sun_family;
    char sun_path[108];
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

void* cli_function(void *client_sock){
   
  struct sockaddr_in client_addr;

  memset(&client_addr, 0, sizeof(struct sockaddr_in));

 *((int*)client_sock) = socket(PF_FILE, SOCK_STREAM, 0);

  if(*((int*)client_sock) == -1){
      printf("SOCKET ERROR!\n");
      exit(1);
  }

  client_addr.sun_family = AF_UNIX;
  strcpy(client_addr.sun_path, FILE_SERVER);
}

/*
 * Description: Currently this function only handles single built_in commands. You should modify this structure to launch process and offer pipeline functionality.
 */
int evaluate_command(int n_commands, struct single_command (*commands)[512])
{

  if(n_commands >= 2){
    struct single_command* incom = (*commands);
    struct single_command* outcom = &((*commands)[1]);

    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    pthread_t Cthread;
  
    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    memset(&client_addr, 0, sizeof(struct sockaddr_in));

    server_sock = socket(PF_FILE, SOCK_STREAM, 0);

    if(server_sock == -1){
	    printf("SOCKET ERROR!\n");
	    exit(1);
    }

    server_addr.sun_family = AF_UNIX;
    strcpy(server_addr.sun_path, FILE_SERVER);

    if(bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1){
      printf("BIND ERROR!\n");
      exit(1);
    }

    int thr_id = pthread_create(&Cthread, NULL, cli_function, (void*)&client_sock);

    if(thr_id < 0){
	printf("THREAD ERROR!\n");
	exit(1);
    }

    pthread_join(Cthread, (void*)outcom);

    if(listen(server_sock, 5) == -1){
	printf("LISTEN ERROR!\n");
	exit(1);
    }
   
    return 0;
  }
  
  if (n_commands > 0) { 
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
      int is_bg= 0;

      pid = fork();

      if(pid == 0){

	if(strcmp(com->argv[(com->argc)-1],"&") == 0){
	  com->argv[(com->argc)-1] = NULL;
	  (com->argc)--;
	  is_bg = 1;
        }      

	if(execv(com->argv[0], com->argv) == -1)
          exit(0);
	else if(is_bg == 1){
	  printf("is_bg : %d\n",is_bg);
	  signal(SIGSTOP, SIG_DFL);
	}
	
     }
      else{
	  if(is_bg == 0)
	      wait(&status);
	  else if(is_bg == 1)
	      waitpid(pid, &status, WNOHANG);
	  
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
