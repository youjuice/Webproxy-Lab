#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000    // 최대 캐시 크기
#define MAX_OBJECT_SIZE 102400    // 최대 객체 크기

typedef struct node {
    char *key;              // 요청한 URL이나 URI
    unsigned char *value;   // 해당 URL에 대한 응답 데이터
    struct node *prev;
    struct node *next;
    long size;
} cache_node;

typedef struct cache {
    cache_node *root;
    cache_node *tail;
    int size;
} cache;

void doit(int fd);
void parse_uri(char *uri, char *hostname, char *pathname, char *port);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void read_requesthdrs(rio_t *rp);
void *thread(void *vargp);
void init_cache();
cache_node *find_cache_node(cache *c, char *key);
cache_node *create_cache_node(char *key, char *value, long size);
void insert_cache_node(cache *c, char *key, char *value, long size);
void delete_cache_node(cache *c, cache_node *node);


/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

static cache *my_cache;

int main(int argc, char **argv) {
  int listenfd, *clientfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  pthread_t tid;    // 새로운 스레드 ID를 저장하기 위한 변수

  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  // SIGPIPE : 소켓을 통해 데이터를 쓰려고 할 때, 읽는 쪽이 이미 닫혀 있거나 연결이 끊어진 상태에 쓰기 작업을 시도할 때 발생
  // signal(SIGPIPE, SIG_IGN) - SIGPIPE 신호를 무시하도록 설정 (클라이언트 연결이 끊어지더라도 프로세스가 종료되지 않도록 방지)
  // concurrent 환경에서 다른 스레드가 소켓을 닫더라도 현재 작업중인 스레드가 종료되지 않고 계속 실행되도록 하기 위함!!
  signal(SIGPIPE, SIG_IGN);

  listenfd = Open_listenfd(argv[1]);
  init_cache();

  while(1)
  {
    clientlen = sizeof(clientaddr);
    clientfd = Malloc(sizeof(int));
    *clientfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); 
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s %s)\n", hostname, port);
    Pthread_create(&tid, NULL, thread, clientfd);     // 새로운 스레드를 생성하여 클라이언트와 통신을 담당하는 함수 thread 실행
  }
}


void *thread(void *vargp)
{
  int clientfd = *((int *)vargp);
  Pthread_detach(pthread_self());
  Free(vargp);
  doit(clientfd);
  Close(clientfd);
  return NULL;
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
    int serverfd;                                               // 서버 소켓 파일 디스크립터
    char request_buf[MAXLINE], response_buf[MAX_OBJECT_SIZE];   // 요청, 응답 데이터용 버퍼
    char method[MAXLINE], uri[MAXLINE], path[MAXLINE];
    char hostname[MAXLINE], port[MAXLINE];
    rio_t request_rio, response_rio;                

    // 1. 클라이언트의 요청을 받아들인다.
    Rio_readinitb(&request_rio, clientfd);                              // 클라이언트 소켓과 request 버퍼 연결
    Rio_readlineb(&request_rio, request_buf, MAXLINE);                  // request 버퍼에서 한 줄 읽어들임
    printf("Request header: %s\n", request_buf);

    sscanf(request_buf, "%s %s", method, uri);                          // 버퍼의 내용을 method와 uri에 저장
    
    // ** Cache Mode에서 추가 **
    cache_node *cached_node = find_cache_node(my_cache, uri);           // cache node 탐색

    if (cached_node != NULL)                                            // 탐색 성공 시,
    {
        Rio_writen(clientfd, cached_node->value, cached_node->size);    // 데이터 반환
        return;
    }

    // 2. 받은 요청을 파싱하여 요청된 URI를 추출한다.
    parse_uri(uri, hostname, port, path);                               // uri 분석하는 함수

    sprintf(request_buf, "%s /%s %s\r\n", method, path, "HTTP/1.0");
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
    ssize_t response_size = Rio_readnb(&response_rio, response_buf, MAX_OBJECT_SIZE);

    // 6. 받은 응답을 클라이언트에게 전송한다.
    Rio_writen(clientfd, response_buf, response_size);

    // ** Cache Mode에서 추가 **
    if (strlen(response_buf) < MAX_OBJECT_SIZE)
        insert_cache_node(my_cache, uri, response_buf, response_size);  // 전송 후 노드 삽입
    
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
    printf("====> uri: %s\n", uri);
    char uri_copy[MAXLINE];
    strcpy(uri_copy, uri);

    char *hostname_ptr = strstr(uri_copy, "//") != NULL ? strstr(uri_copy, "//") + 2 : uri_copy + 1; 
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
    printf("====> host: %s, port: %s, path: %s\n\n", hostname, port, path);
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


/*
 * <Cache 과제>를 위한 함수 ***
 * - Cache 자료구조 -> 연결리스트 사용
 * 1) 연결리스트를 사용하면 캐시의 크기를 동적으로 조절 가능
 * 2) 연결리스트는 데이터를 삽입한 순서대로 유지할 수 있음
 * 3) 연결리스트의 노드는 순차적으로 배치되어 있어서 접근할 때 효율적
 */
// 캐시 초기화 함수
void init_cache()
{
    my_cache = (cache *)malloc(sizeof(cache));
    my_cache->root = NULL;
    my_cache->tail = NULL;
    my_cache->size = 0;
}


// 새로운 캐시 노드를 생성하는 함수
cache_node *create_cache_node(char *key, char *value, long size)
{
    cache_node *new_node = (cache_node *)malloc(sizeof(cache_node));        // 캐시 노드 구조체의 크기만큼 메모리 할당
    new_node->key = malloc(strlen(key) + 1);                                // new_node의 key와 value 메모리 할당 + 값 복사
    strcpy(new_node->key, key);
    new_node->value = malloc(size);
    memcpy(new_node->value, value, size);
    new_node->size = size;
    new_node->prev = NULL;                                                  // 이전 노드를 가리키는 포인터 초기화
    new_node->next = NULL;                                                  // 다음 노드를 가리키는 포인터 초기화
    return new_node;
}


// 주어진 키에 해당하는 캐시 노드를 찾는 함수
cache_node *find_cache_node(cache *c, char *key)
{
    cache_node *current = c->root;                  // root를 현재 노드로 설정
    while (current != NULL)
    {
        if (strcasecmp(current->key, key) == 0)     // 찾는 key 값을 탐색
            return current;
        
        current = current->next;                    // 다음 노드를 현재 노드로 설정
    }
    return NULL;
}


// 캐시에 새로운 노드를 삽입하는 함수
void insert_cache_node(cache *c, char *key, char *value, long size)
{
    while (c->size + size > MAX_CACHE_SIZE)     // 최대 캐시 크기를 초과하는 경우,
    {
        delete_cache_node(c, c->tail);          // 가장 오래된 캐시 노드를 삭제
    }

    cache_node *new_node = create_cache_node(key, value, size);     // 새로운 캐시 노드 생성
    if (c->root == NULL)
        c->root = new_node;
    else 
    {
        new_node->next = c->root;
        c->root->prev = new_node;
        c->root = new_node;
    }
    c->size += size;
}


// 캐시에서 노드를 삭제하는 함수
void delete_cache_node(cache *c, cache_node *node)
{
    if (node->prev != NULL)                 // 첫번째 노드가 아닐 때
        node->prev->next = node->next;
    else                                    // 첫번째 노드일 때
        c->root = node->next;
    
    if (node->next != NULL)                 // 마지막 노드가 아닐 때
        node->next->prev = node->prev;
    else                                    // 마지막 노드일 때
        c->tail = node->prev;
    
    c->size -= node->size;                  // 사이즈 변경
    free(node->key);                        // 메모리 해제
    free(node->value);
    free(node);
}
