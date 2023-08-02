/***************************************************************************//**

  @file         main.c

  @author       Stephen Brennan

  @date         Thursday,  8 January 2015

  @brief        LSH (Libstephen SHell)

*******************************************************************************/

#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define PAGE_SIZE 5 //line per page
#define BUFFER_SIZE 1024 // buffer size for program file output
#define STRING_SIZE 50 // used for string concats on file names


/*
  Function Declarations for builtin shell commands:
 */
int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);
int lsh_show(char **args);
int lsh_run(char **args);
int lsh_launch(char **args);

/*
  List of builtin commands, followed by their corresponding functions.
 */
char *builtin_str[] = {
  "cd",
  "help",
  "exit",
  "show",
  "run"


};

int (*builtin_func[]) (char **) = {
  &lsh_cd,
  &lsh_help,
  &lsh_exit, 
  &lsh_show,
  &lsh_run
  
};

int lsh_num_builtins() {
  return sizeof(builtin_str) / sizeof(char *);
}

/*
  Builtin function implementations.
*/

int test(char *buffer, char *testLine, char *input) {
    if (strcmp(buffer, testLine) == 0) {
        printf("Test for %sSuccess\n", input);
        return 1;
    } else {
        printf("\nTest for %sFailed\n", input);
        printf("Expected: \"%s\"\n", testLine);
        printf("Actual: \"%s\"\n", buffer);
        return 0;
    }
}

int lsh_run(char **args) {
    pid_t pid;
    int fd[2]; //file descriptor
    int status;
    // char array for input file lines
    char *inLine;
    // char array for test output lines
    char *testLine;
    char buffer[BUFFER_SIZE];
    size_t inLen = 0;
    size_t testLen = 0;

    // trackers for tests
    int total = 0;
    int passed = 0;

    // get arguments for running program
    char *arguments[3];
    // used for storing file extension
    char *fileExt;
    if((fileExt = strrchr(args[1],'.')) != NULL ) {
        // check for c program
        if(strcmp(fileExt,".c") == 0) {
            char progRun[STRING_SIZE] = "./";
            char *name = strtok(strdup(args[1]), ".");
            char *compile[] = {"gcc", "-o", name, args[1], NULL};
            strcat(progRun, name);
            arguments[0] = strdup(progRun);
            arguments[1] = NULL;
            lsh_launch(compile);
        }
        else if (strcmp(fileExt, ".py") == 0) {
            char *name = strtok(strdup(args[1]), ".");
            arguments[0] = "python3";
            arguments[1] = name;
            arguments[2] = NULL;
        }
        else {
            printf("File type not supported\n");
            return 0;
        }
    }
    FILE *input = fopen(args[2], "r");
    FILE *expected = fopen(args[3], "r");
    if (!input || !expected) {
        perror("File opening error\n");
        return 0;
    }

    while ((getline(&inLine, &inLen, input)) != -1 && (getline(&testLine, &testLen, expected))) {
        // create new pipe
        if (pipe(fd) == -1) {
            perror("Pipe Failed");
            exit(1);
        }

        // temp file used for dup2 multiple test case inputs
        FILE *currInput = tmpfile();
        if (currInput == NULL) {
            perror("Error opening file");
            exit(1);
        }
        fputs(inLine, currInput);
        // set file pointer to start so child reads file stream from beginning
        rewind(currInput);
        pid = fork();
        if (pid == 0) {
            // change STDIN to input file for testing
            dup2(fileno(currInput), STDIN_FILENO);
            // connect pipe entrance to STDOUT to be inherited by child process
            dup2(fd[1], STDOUT_FILENO);
            fclose(currInput);
            close(fd[0]);

            // Child process
            if (execvp(arguments[0], arguments) == -1) {
                perror("Child Error");
            }
            exit(EXIT_FAILURE);
        } else if (pid < 0) {
            // Error forking
            perror("Fork Error");
        } else {
            // Parent process
            close(fd[1]);
            do {
                waitpid(pid, &status, WUNTRACED);
                read(fd[0], buffer, sizeof(buffer));
                passed += test(buffer, testLine, inLine);
                total++;
                // clear buffer so next child output can be read into buffer
                memset(buffer, 0, sizeof(buffer));
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        }
    }
    printf("%d/%d Tests Passed\n", passed, total);
    fclose(input);
    return 1;
}

int lsh_show(char **args)
{
    FILE *file;
    char line[1024];

    // check if a filename was provided
    if (**args < 2) {
        printf("Usage: %s <filename>\n", args[0]);
        return 1;
    }

    // open the file for reading
    file = fopen(args[1], "r");
    if (!file) {
        printf("Error: could not open file %s\n", args[1]);
        return 1;
    }
    int page = 1;
    int line_count = 0;
  
    // read and print each line of the file
    while (fgets(line, sizeof(line), file)) {
        printf("%s", line);
        line_count++;
        if (line_count == PAGE_SIZE) {  // end of page reached
            printf("--- Press ENTER for next page ---\n");
            getchar();  // wait for user to press ENTER
            printf("\n\nPage %d\n\n", ++page);
            line_count = 0;
        }
    }
  

    // close the file
    fclose(file);

    return 1;
}

/**
   @brief Builtin command: change directory.
   @param args List of args.  args[0] is "cd".  args[1] is the directory.
   @return Always returns 1, to continue executing.
 */
int lsh_cd(char **args)
{
  if (args[1] == NULL) {
    fprintf(stderr, "lsh: expected argument to \"cd\"\n");
  } else {
    if (chdir(args[1]) != 0) {
      perror("lsh");
    }
  }
  return 1;
}

/**
   @brief Builtin command: print help.
   @param args List of args.  Not examined.
   @return Always returns 1, to continue executing.
 */
int lsh_help(char **args)
{
  int i;
  printf("Stephen Brennan's LSH\n");
  printf("Type program names and arguments, and hit enter.\n");
  printf("The following are built in:\n");

  for (i = 0; i < lsh_num_builtins(); i++) {
    printf("  %s\n", builtin_str[i]);
  }

  printf("Use the man command for information on other programs.\n");
  return 1;
}

/**
   @brief Builtin command: exit.
   @param args List of args.  Not examined.
   @return Always returns 0, to terminate execution.
 */
int lsh_exit(char **args)
{
  return 0;
}

/**
  @brief Launch a program and wait for it to terminate.
  @param args Null terminated list of arguments (including program).
  @return Always returns 1, to continue execution.
 */
int lsh_launch(char **args)
{
  pid_t pid;
  int status;

  pid = fork();
  if (pid == 0) {
    // Child process
    if (execvp(args[0], args) == -1) {
      perror("lsh");
    }
    exit(EXIT_FAILURE);
  } else if (pid < 0) {
    // Error forking
    perror("lsh");
  } else {
    // Parent process
    do {
      waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }

  return 1;
}

/**
   @brief Execute shell built-in or launch program.
   @param args Null terminated list of arguments.
   @return 1 if the shell should continue running, 0 if it should terminate
 */
int lsh_execute(char **args)
{
  int i;

  if (args[0] == NULL) {
    // An empty command was entered.
    return 1;
  }

  for (i = 0; i < lsh_num_builtins(); i++) {
    if (strcmp(args[0], builtin_str[i]) == 0) {
      return (*builtin_func[i])(args);
    }
  }

  return lsh_launch(args);
}

/**
   @brief Read a line of input from stdin.
   @return The line from stdin.
 */
char *lsh_read_line(void)
{
#ifdef LSH_USE_STD_GETLINE
  char *line = NULL;
  ssize_t bufsize = 0; // have getline allocate a buffer for us
  if (getline(&line, &bufsize, stdin) == -1) {
    if (feof(stdin)) {
      exit(EXIT_SUCCESS);  // We received an EOF
    } else  {
      perror("lsh: getline\n");
      exit(EXIT_FAILURE);
    }
  }
  return line;
#else
#define LSH_RL_BUFSIZE 1024
  int bufsize = LSH_RL_BUFSIZE;
  int position = 0;
  char *buffer = malloc(sizeof(char) * bufsize);
  int c;

  if (!buffer) {
    fprintf(stderr, "lsh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  while (1) {
    // Read a character
    c = getchar();

    if (c == EOF) {
      exit(EXIT_SUCCESS);
    } else if (c == '\n') {
      buffer[position] = '\0';
      return buffer;
    } else {
      buffer[position] = c;
    }
    position++;

    // If we have exceeded the buffer, reallocate.
    if (position >= bufsize) {
      bufsize += LSH_RL_BUFSIZE;
      buffer = realloc(buffer, bufsize);
      if (!buffer) {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }
  }
#endif
}

#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"
/**
   @brief Split a line into tokens (very naively).
   @param line The line.
   @return Null-terminated array of tokens.
 */
char **lsh_split_line(char *line)
{
  int bufsize = LSH_TOK_BUFSIZE, position = 0;
  char **tokens = malloc(bufsize * sizeof(char*));
  char *token, **tokens_backup;

  if (!tokens) {
    fprintf(stderr, "lsh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  token = strtok(line, LSH_TOK_DELIM);
  while (token != NULL) {
    tokens[position] = token;
    position++;

    if (position >= bufsize) {
      bufsize += LSH_TOK_BUFSIZE;
      tokens_backup = tokens;
      tokens = realloc(tokens, bufsize * sizeof(char*));
      if (!tokens) {
		free(tokens_backup);
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }

    token = strtok(NULL, LSH_TOK_DELIM);
  }
  tokens[position] = NULL;
  return tokens;
}

/**
   @brief Loop getting input and executing it.
 */
void lsh_loop(void)
{
  char *line;
  char **args;
  int status;

  do {
    printf("> ");
    line = lsh_read_line();
    args = lsh_split_line(line);
    status = lsh_execute(args);

    free(line);
    free(args);
  } while (status);
}

/**
   @brief Main entry point.
   @param argc Argument count.
   @param argv Argument vector.
   @return status code
 */
int main(int argc, char **argv)
{
  // Load config files, if any.

  // Run command loop.
  lsh_loop();

  // Perform any shutdown/cleanup.

  return EXIT_SUCCESS;
}


