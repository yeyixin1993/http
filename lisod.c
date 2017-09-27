#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <fcntl.h>

#define BUF_SIZE 4096

FILE *logfile = NULL;
char *www;

/**
 * Print log to lisod.log, debug use.
 */
int printlog(const char *message)
{
	int ret = fprintf(logfile, "%s\n", message);
	fflush(logfile);
	return ret;
}

/**
 * Close socket.
 */
int close_socket(int sock)
{
	if (close(sock))
	{
		printlog("Failed closing socket.\n");
		return 1;
	}
	return 0;
}

/**
 * Get file type based on extension name.
 */
char* get_type(char *uri)
{
	if (strstr(uri, ".html"))
	{
		return "text/html";
	}
	else if (strstr(uri, ".css"))
	{
		return "text/css";
	}
	else if (strstr(uri, ".png"))
	{
		return "image/png";
	}
	else if (strstr(uri, ".jpg"))
	{
		return "image/jpeg";
	}
	else if (strstr(uri, ".gif"))
	{
		return "image/gif";
	}
	else
	{
		return "application/octet-stream";
	}
}

/**
 * Handle GET and POST request.
 */
void handle_get_and_post(int sock, char *uri)
{
	char path[BUF_SIZE];
	char buf[BUF_SIZE];
	memset(path, 0, BUF_SIZE);
	memset(buf, 0, BUF_SIZE);
	// get the file path
	sprintf(path, "%s%s", www, uri);
	printlog(path);
	struct stat st;
	// check file exist
	if (access(path, W_OK) < 0)
	{
		sprintf(buf, "HTTP/1.1 404 NOT FOUND\r\n");
		send(sock, buf, strlen(buf), 0);
	}
	else if (stat(path, &st) < 0)
	{
		sprintf(buf, "HTTP/1.1 404 NOT FOUND\r\n");
		send(sock, buf, strlen(buf), 0);
	}
	else
	{
		time_t now;
		struct tm *t;
		time(&now);
		t = localtime(&now);
		// send http header
		sprintf(buf, "HTTP/1.1 200 OK\r\n");
		strftime(buf + strlen(buf), BUF_SIZE - strlen(buf),
				"Date: %a, %d %b %Y %H:%M:%S %Z\r\n", t);
		sprintf(buf + strlen(buf), "Server: Liso/1.0\r\n");
		strftime(buf + strlen(buf), BUF_SIZE - strlen(buf),
				"Last-Modified: %a, %d %b %Y %H:%M:%S %Z\r\n", t);
		sprintf(buf + strlen(buf), "Content-Length: %ld\r\n", st.st_size);
		sprintf(buf + strlen(buf), "Connection: close\r\n");
		sprintf(buf + strlen(buf), "Content-Type: %s\r\n", get_type(uri));
		sprintf(buf + strlen(buf), "\r\n");

		printlog(buf);
		// send http body
		send(sock, buf, strlen(buf), 0);
		int fd = open(path, O_RDONLY);
		memset(buf, 0, BUF_SIZE);
		int sz = 0;
		sz = read(fd, buf, BUF_SIZE);
		while (sz > 0)
		{
			send(sock, buf, sz, 0);
			sz = read(fd, buf, BUF_SIZE);
		}
	}
}

/**
 * Handle HEAD request.
 */
void handle_head(int sock, char *uri)
{
	char path[BUF_SIZE];
	char buf[BUF_SIZE];
	memset(path, 0, BUF_SIZE);
	memset(buf, 0, BUF_SIZE);
	// get the file path
	sprintf(path, "%s%s", www, uri);
	printlog(path);
	struct stat st;
	// check file exist
	if (access(path, W_OK) < 0)
	{
		sprintf(buf, "HTTP/1.1 404 NOT FOUND\r\n");
		send(sock, buf, strlen(buf), 0);
	}
	else if (stat(path, &st) < 0)
	{
		sprintf(buf, "HTTP/1.1 404 NOT FOUND\r\n");
		send(sock, buf, strlen(buf), 0);
	}
	else
	{
		time_t now;
		struct tm *t;
		time(&now);
		t = localtime(&now);
		// send http header only
		sprintf(buf, "HTTP/1.1 200 OK\r\n");
		strftime(buf + strlen(buf), BUF_SIZE - strlen(buf),
				"Date: %a, %d %b %Y %H:%M:%S %Z\r\n", t);
		sprintf(buf + strlen(buf), "Server: Liso/1.0\r\n");
		strftime(buf + strlen(buf), BUF_SIZE - strlen(buf),
				"Last-Modified: %a, %d %b %Y %H:%M:%S %Z\r\n", t);
		sprintf(buf + strlen(buf), "Content-Length: %ld\r\n", st.st_size);
		sprintf(buf + strlen(buf), "Connection: close\r\n");
		sprintf(buf + strlen(buf), "Content-Type: %s\r\n", get_type(uri));
		sprintf(buf + strlen(buf), "\r\n");

		printlog(buf);
		send(sock, buf, strlen(buf), 0);
	}
}

/**
 * Header when error.
 */
void send_head(int sock, char *buf)
{
	time_t now;
	struct tm *t;
	time(&now);
	t = localtime(&now);
	sprintf(buf + strlen(buf), "Server: Liso/1.0\r\n");
	strftime(buf + strlen(buf), BUF_SIZE - strlen(buf),
			"Date: %a, %d %b %Y %H:%M:%S %Z\r\n", t);
	sprintf(buf + strlen(buf), "Connection: close\r\n");
	sprintf(buf + strlen(buf), "\r\n");
	printlog(buf);
	send(sock, buf, strlen(buf), 0);
}

/**
 * TCP connection handler.
 */
void connection_handler(int sock)
{
	int readret;
	char buf[BUF_SIZE], method[BUF_SIZE], uri[BUF_SIZE], version[BUF_SIZE];
	// read request
	readret = recv(sock, buf, BUF_SIZE, 0);
	buf[BUF_SIZE - 1] = '\0';
	if (readret == -1)
	{
		printlog("Error reading from client socket.\n");
		return;
	}
	if (readret == 0)
	{
		return;
	}
	printlog(buf);
	// parse method, uri and version
	if (sscanf(buf, "%s %s %s", method, uri, version) != 3)
	{
		printlog("Error reading header.\n");
		return;
	}
	// invalid version
	if (strncmp(version, "HTTP/1.1", 8) != 0)
	{
		memset(buf, 0, BUF_SIZE);
		sprintf(buf, "HTTP/1.1 501 Method Unimplemented.\r\n");
		send_head(sock, buf);
		return;
	}
	// handle HEAD request
	if (strncmp(method, "HEAD", 4) == 0)
	{
		handle_head(sock, uri);
	}
	// handle GET request
	else if (strncmp(method, "GET", 3) == 0)
	{
		handle_get_and_post(sock, uri);
	}
	// handle POST request
	else if (strncmp(method, "POST", 4) == 0)
	{
		handle_get_and_post(sock, uri);
	}
	// bad request
	else
	{
		memset(buf, 0, BUF_SIZE);
		sprintf(buf, "HTTP/1.1 501 Method Unimplemented.\r\n");
		send_head(sock, buf);
	}
}

int main(int argc, char* argv[])
{
	if (argc < 9)
	{
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		return EXIT_FAILURE;
	}

	logfile = fopen("lisod.log", "w+");
	www = argv[5];

	int sock, client_sock;
	socklen_t cli_size;
	struct sockaddr_in addr, cli_addr;

	printlog("----- Server Start -----\n");

	// all networked programs must create a socket
	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
	{
		printlog("Failed creating socket.\n");
		return EXIT_FAILURE;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(argv[1]));
	addr.sin_addr.s_addr = INADDR_ANY;

	// servers bind sockets to ports---notify the OS they accept connections
	if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)))
	{
		close_socket(sock);
		printlog("Failed binding socket.\n");
		return EXIT_FAILURE;
	}

	if (listen(sock, 1000))
	{
		close_socket(sock);
		printlog("Error listening on socket.\n");
		return EXIT_FAILURE;
	}

	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(sock, &rfds);
	int fdmax = sock, i;

	// finally, loop waiting for input and then write it back */
	while (1)
	{
		if (select(sock + 1, &rfds, NULL, NULL, NULL) < 0)
		{
			printlog("Error select");
			return EXIT_FAILURE;
		}
		for (i = 0; i <= fdmax; i++)
		{
			if (FD_ISSET(i, &rfds))
			{
				// request from listen fd
				if (i == sock)
				{
					cli_size = sizeof(cli_addr);
					// accept client request
					if ((client_sock = accept(sock,
							(struct sockaddr *) &cli_addr, &cli_size)) == -1)
					{
						close(sock);
						printlog("Error accepting connection.\n");
						return EXIT_FAILURE;
					}
					FD_SET(client_sock, &rfds);
					if (client_sock > fdmax)
					{
						fdmax = client_sock;
					}
				}
				// request from client connection
				else
				{
					connection_handler(i);
					close_socket(i);
					FD_CLR(i, &rfds);
				}
			}
		}
	}

	close_socket(sock);

	return EXIT_SUCCESS;
}
