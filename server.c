#include <stdio.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>

int epoll = -1;
/* char response_body[8192] = {}; */

struct io_event {
	int fd;
	int (*event_handler)(struct epoll_event* a_event);
};

struct http_request_info {
	char* method;
	char* url;
	char* version;
	char* host;
	char* user_agent;
	char* accept;
};

int history() {
}

int client_echo(struct epoll_event* a_event) {
	struct io_event* event = a_event->data.ptr;
	char buf[1000] = {};
	read(event->fd, buf, sizeof(buf));
	//GET /index.html HTTP/1.1
	//Host: x.x.x.x:51234
	//User-Agent: curl/7.47.0
	//Accept: */*
	//
	char* lines = buf;
	char* line;
	int lineno = 0;
	struct http_request_info req = {};
	while((line = strsep(&lines, "\n")) && strlen(line) > 2) {
		line[strlen(line) - 1] = '\0'; // remove carriage return
		if(lineno == 0) {
			req.method = strsep(&line, " ");
			req.url = strsep(&line, " ");
			req.version = strsep(&line, " ");
			lineno += 1;
			continue;
		}

		char* key = strsep(&line, ":");
		char* value = line + 1;
		if(strstr(key, "Host"))
			req.host = value;
		else if(strstr(key, "User-Agent"))
			req.user_agent = value;
		else if(strstr(key, "Accept"))
			req.accept = value;

		lineno += 1;
	}

	char response_header[] = "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n";
	uint8_t response_body[8192] = {};

	/* strcat(response_body, req.url); */
	/* strcat(response_body, "\r\n"); */

	int resource = open(req.url + 1, O_RDONLY);
	if(resource == -1) {
		char response_404[] = "404 Not Found\r\n";
		write(event->fd, response_header, sizeof(response_header)-1);
		write(event->fd, response_404, sizeof(response_404)-1);
		close(event->fd);

		free(event);
		return 1;
	}
	read(resource, response_body, sizeof(response_body));
	close(resource);

	write(event->fd, response_header, sizeof(response_header)-1);
	write(event->fd, response_body, sizeof(response_body)-1);
	close(event->fd);
	free(event);

	return 0;
}

int server_accept(struct epoll_event* a_event) {
	struct io_event* event = a_event->data.ptr;

	struct sockaddr_in client_addr;
	socklen_t client_addrlen = sizeof(client_addr);

	int client = accept(event->fd, (struct sockaddr*)&client_addr, &client_addrlen);
	if(client == -1) {
		perror("failed to accept a new client");
		return 1;
	}

	struct io_event* client_context = calloc(1, sizeof(struct io_event));
	client_context->fd = client;
	client_context->event_handler = client_echo;

	struct epoll_event client_event;
	client_event.events = EPOLLIN;
	client_event.data.ptr = client_context;
	epoll_ctl(epoll, EPOLL_CTL_ADD, client, &client_event);

	return 0;
}

int main(int argc, char *argv[])
{
	struct sockaddr_in server_addr;
	int server_fd, client_fd;
	socklen_t len;

	if(argc != 2)
	{
		printf("usage : %s [port]\n", argv[0]);
		exit(0);
	}

	if((server_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
	{// 소켓 생성
		printf("Server : Can't open stream socket\n");
		exit(0);
	}
	memset(&server_addr, 0x00, sizeof(server_addr));
	//server_Addr 을 NULL로 초기화

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(atoi(argv[1]));
	//server_addr 셋팅

	if(bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
	{//bind() 호출
		printf("Server : Can't bind local address.\n");
		exit(0);
	}

	if(listen(server_fd, 5) < 0)
	{//소켓을 수동 대기모드로 설정
		printf("Server : Can't listening connect.\n");
		exit(0);
	}

	epoll = epoll_create1(0);
	if(epoll == -1)
		exit(0);

	struct io_event* server_on_accept = calloc(1, sizeof(struct io_event));
	server_on_accept->fd = server_fd;
	server_on_accept->event_handler = server_accept;

	struct epoll_event server_context;
	server_context.events = EPOLLIN;
	server_context.data.ptr = server_on_accept;
	epoll_ctl(epoll, EPOLL_CTL_ADD, server_fd, &server_context);


	while(1) {
		struct epoll_event events[10];
		int nevent = epoll_wait(epoll, events, 10, 100/*ms*/);
		if(nevent == -1) {
			perror("epoll_wait() failed");
			exit(1);
		}

		for(int i = 0; i < nevent; ++i) {
			struct io_event* e = events[i].data.ptr;
			e->event_handler(&events[i]);
		}
	}

	return 0;
}

