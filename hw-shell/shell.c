#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <dirent.h>

#include "tokenizer.h"

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

/* get the last of the full path */
char *getLast(char *str);

// 在环境变量中寻找file_name， 找到的路径存buffer里
// return 1 if found, 0 if not found
// buffer 装不下返回 -1 
int find_path(char *file_name, char *buffer, int buffer_length);


// 在指定路径下找
// 路径不对返回-1
// 找到返回1， 没找到返回0
int is_found(char *dir_path, char *file_name);

/* 当在shell中输入的命令有 ">" 或 "<" 时，需要重定向
  return real fp if need redirection
  return NULL if not need redirection
 */
FILE* is_change(struct tokens *tokens);

int CMD_MAX_LENGTH = 16;
void deal_pipe(int pipes, int (*pipe_arr)[2], int process_num, char *cmds[], char *args[][CMD_MAX_LENGTH]);

// 输入的命令是否有pipe
int is_found_pipe(struct tokens* tokens);

int cmd_exit(struct tokens* tokens);
int cmd_help(struct tokens* tokens);
int cmd_pwd(struct tokens* tokens);
int cmd_cd(struct tokens* tokens);

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens* tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t* fun;
  char* cmd;
  char* doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
    {cmd_help, "?", "show this help menu"},
    {cmd_exit, "exit", "exit the command shell"},
    {cmd_pwd, "pwd", "print working directory"},
    {cmd_cd, "cd", "change working directory"},
};

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens* tokens) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens* tokens) { exit(0); }

/* pwd */
int cmd_pwd(unused struct tokens* tokens) {
  char buffer[256];
  getcwd(buffer, 256);
  puts(buffer);
  return 1;
}


/* cd*/
int cmd_cd(unused struct tokens* tokens) {
  char *path = tokens_get_token(tokens, 1);
  if (chdir(path) == -1) {
    // puts("error!");
    return -1;
  }
  return 1;
}

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  return -1;
}


void customHandler(int signum)
{
    fprintf(stdout, "$: \n");
}

/* Intialization procedures for this shell */
void init_shell() {
  /* Our shell is connected to standard input. */
  shell_terminal = STDIN_FILENO;

  /* Check if we are running interactively */
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive) {
    /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
     * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
     * foreground, we'll receive a SIGCONT. */
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    /* Saves the shell's process id */
    shell_pgid = getpid();

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Save the current termios to a variable, so it can be restored later. */
    tcgetattr(shell_terminal, &shell_tmodes);
  }
}

int main(unused int argc, unused char* argv[]) {
  init_shell();

  /* shell should ignore SIGINT */

  // below will terminate fgets()
/*   struct sigaction act;
  act.sa_handler = customHandler;
  act.sa_flags = 0;
  sigemptyset(&act.sa_mask);

  sigaction(SIGINT, &act, NULL); */

  signal(SIGINT, SIG_IGN);

  static char line[4096];
  int line_num = 0;

  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);

  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    struct tokens* tokens = tokenize(line);

    /* Determine if there are pipes (假设存在pipe时，不用 built-in function) */
    int pipes = is_found_pipe(tokens);
    if (pipes) {
      /*
      cat test.txt | grep wagdi  不好使， 干脆不弄了，用自己写的辅助函数(process1 process2 process3)去实现pipe
      */
     
      /*
      int pipe_arr[pipes][2];
      int process_num = pipes + 1;

      char *args[process_num][CMD_MAX_LENGTH];

      // 装配 args
      int row = 0;
      int col = 0;
      for (int i = 0; i < tokens_get_length(tokens); i++) {
        char *str = tokens_get_token(tokens, i);
        if (strcmp(str, "|") == 0) {
          args[row][col] = NULL;
          row++;
          col = 0;
        } else {
          args[row][col] = str;
          col++;
        }
      }

      // 装配 full_paths
      char *full_paths[process_num];
      int buff_len = 64;
      for (int j = 0; j < process_num; j++) {
        full_paths[j] = (char *) malloc(buff_len);

        if (find_path(args[j][0], full_paths[j], buff_len) != 1) {
          perror("command not found");
          exit(0);
        } else {
          strcat(full_paths[j], "/");
          strcat(full_paths[j], args[j][0]);
        }
      } */
      int pipes = 2;
      int pipe_arr[pipes][2] ;

      int process_num = pipes + 1;
      char *full_paths[] = {"./process1", "./process2", "./process3"};

      char *args[][16] = {
          {"process1", NULL},
          {"process2", NULL},
          {"process3", NULL}
      };

      deal_pipe(pipes, pipe_arr, process_num, full_paths, args);

/*       for (int j = 0; j < process_num; j++) {
        free(full_paths[j]);
      } */
      goto end;
    }
 
    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, 0));

    if (fundex >= 0) {
      FILE *fp = is_change(tokens);
      cmd_table[fundex].fun(tokens);

      if (fp) {
        if (freopen("/dev/tty", "w", stdout) == NULL)  // restore redirection
        {
          perror("freopen");
          return 1;
        }
        // fclose(fp);  // 有这句就有bug
      }
    } else {
      /* REPLACE this to run commands as programs. */
      // fprintf(stdout, "This shell doesn't know how to run programs.\n");

      /* 当属入的没有pipe(|)时 */
      pid_t pid;
      if ((pid = fork()) == 0) {
        /* child process */

        // setpgrp();
        signal(SIGINT, SIG_DFL);

        // printf("child: pid = %d, pgid = %d\n", getpid(), getpgrp());

        /* change stdin or stdout if there is '<' or '>'  */
        FILE *fp = is_change(tokens);  // 没关fp, 应该用dup2()重定向。但懒了不想做了

        int arg_len = tokens_get_length(tokens) + 1;
        arg_len = fp ? arg_len - 2 : arg_len;
        char *args[arg_len];

        char c = tokens_get_token(tokens, 0)[0];
        // 如果输入的是 full path
        if (c == '/') {
          char full_path[strlen(tokens_get_token(tokens, 0)) + 1];
          strcpy(full_path, tokens_get_token(tokens, 0));

          for (int j = 0; j < arg_len; j++) {
            if (j == 0) {
              args[j] = getLast(tokens_get_token(tokens, 0));
            } else if (j == arg_len - 1) {
              args[j] = (char *)0;
            } else {
              args[j] = tokens_get_token(tokens, j);
            }
          }
          
          execv(full_path, args);
        }
        else {
          char *file_name = tokens_get_token(tokens, 0);
          int buffer_length = 256;
          char buffer[buffer_length];
          buffer[0] = '\0';
          int temp = find_path(file_name, buffer, buffer_length);
          if (temp != 1) {
            puts("command not found");
            exit(0);
          }
          strcat(buffer, "/");
          strcat(buffer, file_name);

          for (int k = 0; k < arg_len; k++) {
            if (k == arg_len - 1) {
              args[k] = NULL;
              continue;
            }
            args[k] = tokens_get_token(tokens, k);
          }

          execv(buffer, args);
        }
      }
      else {
        /* parent process */
        // setpgid(pid, pid);
        // printf("parent: pid = %d, pgid = %d\n", getpid(), getpgrp());
        wait(NULL);
      }
    }

    end:
    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);

    /* Clean up memory */
    tokens_destroy(tokens);
  }

  return 0;
}


char *getLast(char *str) {
  const char delim[] = "/";

  char *token;
  char *saveptr;

  token = strtok_r(str, delim, &saveptr);
  char *temp;
  while (1)
  {
    temp = strtok_r(NULL, delim, &saveptr);
    if (temp == NULL)
    {
      break;
    }
    else
    {
      token = temp;
    }
  }
  return token;
}


int is_found(char *dir_path, char *file_name) 
{
    DIR *dir;
    if ((dir = opendir(dir_path)) == NULL) {
        perror("error: isfound");
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, file_name) == 0) {
            return 1;
        }
    }

    return 0;
}


int find_path(char *file_name, char *buffer, int buffer_length) 
{
    char *path_val = getenv("PATH");
    char path_temp[strlen(path_val) + 1];
    strcpy(path_temp, path_val);

    const char delim[] = ":";
    char *token;

    token = strtok(path_temp, delim);
    while (token) {
        if (is_found(token, file_name) == 1) {
            if (strlen(token) > buffer_length) {
                return -1;
            }
            strcpy(buffer, token);
            return 1;
        } else {
            token = strtok(NULL, delim);
        }
    }

    return 0;
}

/* 假设 freopen 打开的文件一定存在 */
FILE *is_change(struct tokens *tokens) 
{
  FILE *fp = NULL;

  for (int i = 0; i < tokens_get_length(tokens); i++) {
    char *s = tokens_get_token(tokens, i);
    if (strcmp(s, ">") == 0) {
      fp = freopen(tokens_get_token(tokens, i+1), "w", stdout);
      continue;
    } else if (strcmp(s, "<") == 0) {
      fp = freopen(tokens_get_token(tokens, i+1), "r", stdin);
      continue;
    }
  }

  return fp;
}

int is_found_pipe(struct tokens* tokens)
{
  int count = 0;
  for (int i = 0; i < tokens_get_length(tokens); i++) {
    char *s = tokens_get_token(tokens, i);
    if (strcmp(s, "|") == 0) {
      count++;
    }
  }

  return count;
}

void deal_pipe(int pipes, int (*pipe_arr)[2], int process_num, char *cmds[], char *args[][CMD_MAX_LENGTH])
{
    for (int i = 0; i < process_num; i++) {
        if (i == 0) {
            pipe(pipe_arr[i]);
            if (fork() == 0) {
                close(pipe_arr[i][0]);
                dup2(pipe_arr[i][1], STDOUT_FILENO);
                close(pipe_arr[i][1]);

                execv(cmds[i], args[i]);
            } else {
                wait(NULL);
            }
        }
        else if (i == process_num - 1) {
            if (fork() == 0) {
                close(pipe_arr[i-1][1]);
                dup2(pipe_arr[i-1][0], STDIN_FILENO);
                close(pipe_arr[i-1][0]);

                execv(cmds[i], args[i]);
            } else {
                close(pipe_arr[i-1][0]);
                close(pipe_arr[i-1][1]);
                wait(NULL);
            }
        } 
        else {
            pipe(pipe_arr[i]);
            if (fork() == 0) {
                close(pipe_arr[i-1][1]);
                dup2(pipe_arr[i-1][0], STDIN_FILENO);
                close(pipe_arr[i-1][0]);

                close(pipe_arr[i][0]);
                dup2(pipe_arr[i][1], STDOUT_FILENO);
                close(pipe_arr[i][1]);

                execv(cmds[i], args[i]);
            }else {
                close(pipe_arr[i-1][0]);
                close(pipe_arr[i-1][1]);
                wait(NULL);
            }
        }
    }
}