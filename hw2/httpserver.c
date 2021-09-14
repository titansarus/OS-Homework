#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unistd.h>

#include "libhttp.h"
#include "wq.h"

#define BUFF_SIZE 16384

//UNIX Filesystem's maximum path length
#define MAX_PATH 4096

/*
 * Global configuration variables.
 * You need to use these in your implementation of handle_files_request and
 * handle_proxy_request. Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */
wq_t work_queue;
int num_threads;
int server_port;
char *server_files_directory;
char *server_proxy_hostname;
int server_proxy_port;

/*
 * Actually arguments passing for thread and request_handler can be done without this struct, but this struct will help
 * us in case we want to pass more data to the thread starter.
 */

typedef struct arguments {
    void (*func)(int);
} arguments;

typedef struct proxy_struct {
    int src;
    int dest;
    pthread_cond_t *cond;
    int *is_finished;
} proxy_struct;

/*
 * Write file in path to client port indicated by fd.
 * print error and return from function if file does not exist.
 */

void write_file_to_client(int fd, char *path) {
  int served_file = open(path, O_RDONLY);
  if (served_file == -1) {
    return;
  }
  void *buffer = malloc((BUFF_SIZE) * sizeof(char));
  size_t size;
  while ((size = read(served_file, buffer, BUFF_SIZE)) > 0) {
    http_send_data(fd, buffer, size);
  }
  free(buffer);
  close(served_file);
}

/*
 * Serves the contents the file stored at `path` to the client socket `fd`.
 * It is the caller's responsibility to ensure that the file stored at `path` exists.
 * You can change these functions to anything you want.
 *
 * ATTENTION: Be careful to optimize your code. Judge is
 *            sensitive to time-out errors.
 */

void serve_file(int fd, char *path, struct stat *st) {
  long size = st->st_size;
  char *content_length_buff = malloc(64 * sizeof(char));
  snprintf(content_length_buff, 64, "%ld", size);
  http_start_response(fd, 200);
  http_send_header(fd, "Content-Type", http_get_mime_type(path));
  http_send_header(fd, "Content-Length", content_length_buff);
  http_end_headers(fd);
  write_file_to_client(fd, path);
  free(content_length_buff);

}

/*
 * This function will send links to the contents of a directory and its parent.
 * Note that the case in which directory contains index.html is handled separately.
 */

void serve_directory(int fd, char *path) {
  http_start_response(fd, 200);
  http_send_header(fd, "Content-Type", http_get_mime_type(".html"));
  http_end_headers(fd);
  DIR *dir = opendir(path);
  if (dir) {
    int send_string_size = MAX_PATH * 2 + 30;
    char *send_string = malloc(send_string_size);
    struct dirent *dirent;
    while ((dirent = readdir(dir)) != NULL) {
      snprintf(send_string, send_string_size, "<a href='./%s'>%s</a><br>\n", dirent->d_name, dirent->d_name);
      http_send_string(fd, send_string);
    }
    free(send_string);
    closedir(dir);
  }

}


void send_404_not_found(int fd) {
  http_start_response(fd, 404);
  http_send_header(fd, "Content-Type", "text/html");
  http_end_headers(fd);
}

/*
 * Reads an HTTP request from stream (fd), and writes an HTTP response
 * containing:
 *
 *   1) If user requested an existing file, respond with the file
 *   2) If user requested a directory and index.html exists in the directory,
 *      send the index.html file.
 *   3) If user requested a directory and index.html doesn't exist, send a list
 *      of files in the directory with links to each.
 *   4) Send a 404 Not Found response.
 *
 *   Closes the client socket (fd) when finished.
 */
void handle_files_request(int fd) {

  struct http_request *request = http_request_parse(fd);

  if (request == NULL || request->path[0] != '/') {
    http_start_response(fd, 400);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    free(request);
    return;
  }

  if (strstr(request->path, "..") != NULL) {
    http_start_response(fd, 403);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    free(request);
    return;
  }

  char *path = malloc(MAX_PATH);
  strcpy(path, server_files_directory);
  strcat(path, request->path);

  struct stat st;

  stat(path, &st);

  if (S_ISREG(st.st_mode)) {
    serve_file(fd, path, &st);
  } else if (S_ISDIR(st.st_mode)) {
    char *file_path = malloc(MAX_PATH + 1);
    strcpy(file_path, path);
    strcat(file_path, "/index.html");
    if (stat(file_path, &st) == 0) {
      serve_file(fd, file_path, &st);
    } else {
      serve_directory(fd, path);
    }
    free(file_path);
  } else {
    send_404_not_found(fd);
  }
  free(path);
  free(request);
}


/*
 * A thread function to handle proxy send and receive.
 * args must be a pointer to proxy_struct.
 */
void *proxy_thread_handler(void *args) {
  proxy_struct *proxyStruct = (proxy_struct *) args;
  int src = proxyStruct->src;
  int dest = proxyStruct->dest;
  pthread_cond_t *cond = proxyStruct->cond;


  // Read data from source and immediately send it to dest.
  size_t size;
  char *buffer = malloc(BUFF_SIZE);
  while (!(*proxyStruct->is_finished) && (size = read(src, buffer, BUFF_SIZE - 1)) > 0) { ;
    http_send_data(dest, buffer, size);
  }

  /*
   * set is_finished to 1. If the processes are scheduled such that this thread runs sooner than the proxy_request thread,
   * the line below will cause the main handler handle_proxy_request to proceed and doesn't wait on pthread_cond.
   */
  *(proxyStruct->is_finished) = 1;
  if (cond != NULL) {
    pthread_cond_broadcast(cond);
  }
  free(buffer);
  return NULL;
}

void server_error_502(int fd, int target_fd) {
  /* Dummy request parsing, just to be compliant. */
  http_request_parse(fd);

  http_start_response(fd, 502);
  http_send_header(fd, "Content-Type", "text/html");
  http_end_headers(fd);
  http_send_string(fd, "<center><h1>502 Bad Gateway</h1><hr></center>");
  close(target_fd);
  close(fd);
}

int make_proxy_connection(int fd) {
  /*
   * The code below does a DNS lookup of server_proxy_hostname and
   * opens a connection to it. Please do not modify.
   */
  struct sockaddr_in target_address;
  memset(&target_address, 0, sizeof(target_address));
  target_address.sin_family = AF_INET;
  target_address.sin_port = htons(server_proxy_port);

  struct hostent *target_dns_entry = gethostbyname2(server_proxy_hostname, AF_INET);

  int target_fd = socket(PF_INET, SOCK_STREAM, 0);
  if (target_fd == -1) {
    fprintf(stderr, "Failed to create a new socket: error %d: %s\n", errno, strerror(errno));
    close(fd);
    exit(errno);
  }

  if (target_dns_entry == NULL) {
    fprintf(stderr, "Cannot find host: %s\n", server_proxy_hostname);
    close(target_fd);
    close(fd);
    exit(ENXIO);
  }

  char *dns_address = target_dns_entry->h_addr_list[0];


  memcpy(&target_address.sin_addr, dns_address, sizeof(target_address.sin_addr));
  int connection_status = connect(target_fd, (struct sockaddr *) &target_address,
                                  sizeof(target_address));

  if (connection_status < 0) {
    server_error_502(fd, target_fd);
    return -1;
  }
  return target_fd;
}


/*
 * Opens a connection to the proxy target (hostname=server_proxy_hostname and
 * port=server_proxy_port) and relays traffic to/from the stream fd and the
 * proxy target. HTTP requests from the client (fd) should be sent to the
 * proxy target, and HTTP responses from the proxy target should be sent to
 * the client (fd).
 *
 *   +--------+     +------------+     +--------------+
 *   | client | <-> | httpserver | <-> | proxy target |
 *   +--------+     +------------+     +--------------+
 */
void handle_proxy_request(int fd) {


  int target_fd = make_proxy_connection(fd);
  if (target_fd != -1) {
    int finished = 0;

    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


    // Making two proxy structs. One to send from client to proxy target and one to send from proxy target to client
    proxy_struct *proxyStruct1 = malloc(sizeof(proxy_struct));
    proxy_struct *proxyStruct2 = malloc(sizeof(proxy_struct));

    proxyStruct1->src = fd;
    proxyStruct1->dest = target_fd;
    proxyStruct1->cond = &cond;
    proxyStruct1->is_finished = &finished;

    proxyStruct2->src = target_fd;
    proxyStruct2->dest = fd;
    proxyStruct2->cond = &cond;
    proxyStruct2->is_finished = &finished;

    pthread_t threads[2];

    pthread_create(&threads[0], NULL, proxy_thread_handler, proxyStruct1);
    pthread_create(&threads[1], NULL, proxy_thread_handler, proxyStruct2);

    // While both thread are active and none of them is finished, Wait.
    while (!finished) {
      pthread_cond_wait(&cond, &mutex);
    }

    pthread_cancel(threads[0]);
    pthread_cancel(threads[1]);

    pthread_cond_destroy(&cond);
    pthread_mutex_destroy(&mutex);

    close(target_fd);
    free(proxyStruct1);
    free(proxyStruct2);
  }
}


void *thread_runner(void *args) {
  arguments *argu = (arguments *) args;
  void (*request_handler)(int) = argu->func;
  while (1) {
    int fd = wq_pop(&work_queue);
    request_handler(fd);
    close(fd);
  }
}

void init_thread_pool(int num_threads, void (*request_handler)(int)) {
  pthread_t pthread[num_threads];
  for (int i = 0; i < num_threads; i++) {
    arguments *args = malloc(sizeof(arguments));
    args->func = request_handler;
    pthread_create(&pthread[i], NULL, thread_runner, args);
  }
}


/*
 * Opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
void serve_forever(int *socket_number, void (*request_handler)(int)) {

  struct sockaddr_in server_address, client_address;
  size_t client_address_length = sizeof(client_address);
  int client_socket_number;

  *socket_number = socket(PF_INET, SOCK_STREAM, 0);
  if (*socket_number == -1) {
    perror("Failed to create a new socket");
    exit(errno);
  }

  int socket_option = 1;
  if (setsockopt(*socket_number, SOL_SOCKET, SO_REUSEADDR, &socket_option,
                 sizeof(socket_option)) == -1) {
    perror("Failed to set socket options");
    exit(errno);
  }

  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(server_port);

  if (bind(*socket_number, (struct sockaddr *) &server_address,
           sizeof(server_address)) == -1) {
    perror("Failed to bind on socket");
    exit(errno);
  }

  if (listen(*socket_number, 1024) == -1) {
    perror("Failed to listen on socket");
    exit(errno);
  }

  printf("Listening on port %d...\n", server_port);

  init_thread_pool(num_threads, request_handler);

  while (1) {
    client_socket_number = accept(*socket_number,
                                  (struct sockaddr *) &client_address,
                                  (socklen_t *) &client_address_length);
    if (client_socket_number < 0) {
      perror("Error accepting socket");
      continue;
    }

    printf("Accepted connection from %s on port %d\n",
           inet_ntoa(client_address.sin_addr),
           client_address.sin_port);


    if (num_threads == 0) {
      // Handle single thread program.
      request_handler(client_socket_number);
      close(client_socket_number);
    } else {
      // Handle multi thread program.
      wq_push(&work_queue, client_socket_number);
    }

  }

  shutdown(*socket_number, SHUT_RDWR);
  close(*socket_number);
}

int server_fd;

void signal_callback_handler(int signum) {
  printf("Caught signal %d: %s\n", signum, strsignal(signum));
  printf("Closing socket %d\n", server_fd);
  if (close(server_fd) < 0) perror("Failed to close server_fd (ignoring)\n");
  exit(0);
}

char *USAGE =
        "Usage: ./httpserver --files www_directory/ --port 8000 [--num-threads 5]\n"
        "       ./httpserver --proxy inst.eecs.berkeley.edu:80 --port 8000 [--num-threads 5]\n";

void exit_with_usage() {
  fprintf(stderr, "%s", USAGE);
  exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
  signal(SIGINT, signal_callback_handler);
  signal(SIGPIPE, SIG_IGN);

  /* Default settings */
  server_port = 8000;
  void (*request_handler)(int) = NULL;

  int i;
  for (i = 1; i < argc; i++) {
    if (strcmp("--files", argv[i]) == 0) {
      request_handler = handle_files_request;
      free(server_files_directory);
      server_files_directory = argv[++i];
      if (!server_files_directory) {
        fprintf(stderr, "Expected argument after --files\n");
        exit_with_usage();
      }
    } else if (strcmp("--proxy", argv[i]) == 0) {
      request_handler = handle_proxy_request;

      char *proxy_target = argv[++i];
      if (!proxy_target) {
        fprintf(stderr, "Expected argument after --proxy\n");
        exit_with_usage();
      }

      char *colon_pointer = strchr(proxy_target, ':');
      if (colon_pointer != NULL) {
        *colon_pointer = '\0';
        server_proxy_hostname = proxy_target;
        server_proxy_port = atoi(colon_pointer + 1);
      } else {
        server_proxy_hostname = proxy_target;
        server_proxy_port = 80;
      }
    } else if (strcmp("--port", argv[i]) == 0) {
      char *server_port_string = argv[++i];
      if (!server_port_string) {
        fprintf(stderr, "Expected argument after --port\n");
        exit_with_usage();
      }
      server_port = atoi(server_port_string);
    } else if (strcmp("--num-threads", argv[i]) == 0) {
      char *num_threads_str = argv[++i];
      if (!num_threads_str || (num_threads = atoi(num_threads_str)) < 1) {
        fprintf(stderr, "Expected positive integer after --num-threads\n");
        exit_with_usage();
      }
    } else if (strcmp("--help", argv[i]) == 0) {
      exit_with_usage();
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
      exit_with_usage();
    }
  }

  if (server_files_directory == NULL && server_proxy_hostname == NULL) {
    fprintf(stderr, "Please specify either \"--files [DIRECTORY]\" or \n"
                    "                      \"--proxy [HOSTNAME:PORT]\"\n");
    exit_with_usage();
  }

  wq_init(&work_queue);
  serve_forever(&server_fd, request_handler);

  return EXIT_SUCCESS;
}