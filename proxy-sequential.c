#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000    // 최대 캐시 크기
#define MAX_OBJECT_SIZE 102400    // 최대 객체 크기

void doit(int clientfd);
void parse_uri(char *uri, char *hostname, char *port, char *path);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void read_requesthdrs(rio_t *rp);

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

int main(int argc, char **argv) {
  int listenfd, clientfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);

  while(1)
  {
    clientlen = sizeof(clientaddr);
    clientfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s %s)\n", hostname, port);
    doit(clientfd);
    Close(clientfd);
  }
}


/*
 * < 함수 전체 로직 >
 * 1. 클라이언트의 요청을 받아들인다.
 * 2. 받은 요청을 파싱하여 요청된 URI를 추출한다.
 * 3. 추출된 URI를 기반으로 서버에 새로운 연결을 생성한다.
 * 4. 클라이언트로 받은 요청을 서버로 전송한다.
 * 5. 서버로부터의 응답을 읽어들인다.
 * 6. 받은 응답을 클라이언트에게 전송한다.
 */
void doit(int clientfd)
{
  int serverfd;     // 서버 소켓 파일 디스크립터
  char request_buf[MAXLINE], response_buf[MAX_OBJECT_SIZE];   // 요청, 응답 데이터용 버퍼
  char method[MAXLINE], uri[MAXLINE], path[MAXLINE];
  char hostname[MAXLINE], port[MAXLINE];
  rio_t request_rio, response_rio;                

  // 1. 클라이언트의 요청을 받아들인다.
  Rio_readinitb(&request_rio, clientfd);                      // 클라이언트 소켓과 request 버퍼 연결
  Rio_readlineb(&request_rio, request_buf, MAXLINE);          // request 버퍼에서 한 줄 읽어들임
  printf("Request header: %s\n", request_buf);

  sscanf(request_buf, "%s %s", method, uri);                  // 버퍼의 내용을 method와 uri에 저장

  // 2. 받은 요청을 파싱하여 요청된 URI를 추출한다.
  parse_uri(uri, hostname, port, path);                       // uri 분석하는 함수

  printf("uri: %s\n", uri);

  sprintf(request_buf, "%s /%s %s\r\n", method, path, "HTTP/1.0");
  printf("%s\n", request_buf);
  sprintf(request_buf, "%sConnection: close\r\n", request_buf);
  sprintf(request_buf, "%sProxy-Connection: close\r\n", request_buf);
  sprintf(request_buf, "%s%s\r\n", request_buf, user_agent_hdr);

  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD"))
  {
    clienterror(clientfd, method, "501", "Not Implemented", "Proxy does not implement this method");
    return;
  }

  // 3. 추출된 URI를 기반으로 서버에 새로운 연결을 생성한다.
  serverfd = Open_clientfd(hostname, port);     // 서버 소켓 열기
  if (serverfd < 0)
  {
    clienterror(clientfd, hostname, "404", "Not found", "Proxy couldn't connect to the server");
    return;
  }
  printf("%s\n", request_buf);

  // 4. 클라이언트로 받은 요청을 서버로 전송한다.
  Rio_writen(serverfd, request_buf, strlen(request_buf));   // 요청 내용 서버에 보내기
  Rio_readinitb(&response_rio, serverfd);                   // 서버 소켓과 response 버퍼 연결

  // 5. 서버로부터의 응답을 읽어들인다.
  // 응답 버퍼에서 한줄 씩 읽어와서 클라이언트 소켓에 전송 
  ssize_t n;
  while ((n = Rio_readlineb(&response_rio, response_buf, MAX_OBJECT_SIZE)) > 0)
  {
    // 6. 받은 응답을 클라이언트에게 전송한다.
    Rio_writen(clientfd, response_buf, n);
    if (!strcmp(response_buf, "\r\n"))         // 응답 헤더 전송
      break;
  }

  while ((n = Rio_readlineb(&response_rio, response_buf, MAX_OBJECT_SIZE)) > 0)
  {
    Rio_writen(clientfd, response_buf, n);    // 응답 본문 전송
  }
  Close(serverfd);                            // 서버 소켓 닫기
}


/*
 * parse_uri -> 파싱해야 할 데이터
 * URI: http://www.cmu.edu:8080/hub/index.html
 * Hostname: www.cmu.edu
 * Port: 8080
 * Path: hub/index.html
 */ 
void parse_uri(char *uri, char *hostname, char *port, char *path)
{
  printf("====> parse_uri: %s\n", uri);
  char *hostname_ptr = strstr(uri, "//") != NULL ? strstr(uri, "//") + 2 : uri + 1; 
  char *port_ptr = strstr(hostname_ptr, ":");
  char *path_ptr = strstr(hostname_ptr, "/");

  if (path_ptr > 0)
  {
    *path_ptr = '\0';
    strcpy(path, path_ptr + 1);
  }

  if (port_ptr > 0)
  {
    *port_ptr = '\0';
    strcpy(port, port_ptr + 1);
  }

  strcpy(hostname, hostname_ptr);
  printf("====> parse_uri host: %s, port: %s, path: %s\n", hostname, port, path);
}


// 웹 서버에서 오류가 발생했을 때 클라이언트에게 오류를 알리는 함수
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  // sprintf() - 형식화된 문자열을 생성하여 주어진 버퍼에 저장
  sprintf(body, "<html><title>Proxy Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Proxy server<\em>\r\n", body);

  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));   // 위의 body 내용 출력
}


// 클라이언트로부터 수신된 HTTP 요청 헤더를 읽고 출력하는 함수
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  
  while(strcmp(buf, "\r\n"))
  {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

