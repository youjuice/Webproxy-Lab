#include "csapp.h"

void echo(int connfd);          // 클라이언트 통신하는 함수 echo 선언

/*
 * Listening 소켓 : 클라이언트의 연결을 수신하기 위한 소켓 (listenfd)
 * 1. 지정된 포트 번호에 대한 연결을 받을 수 있는 소켓을 연다.
 * 2. 서버는 리스닝 소켓을 사용하여 클라이언트의 연결 요청을 받고
 * 3. 수락 후 실제 데이터 통신을 위한 소켓을 연다.
 * 
 * Connected 소켓 : 실제 데이터 통신을 위한 소켓 (connfd)
 * 1. 클라이언트와의 연결 요청이 수락되면 
 * 2. 서버와 클라이언트 간에 데이터를 주고 받음.
 */
int main(int argc, char **argv)
{
    int listenfd, connfd;                                   // 서버 소켓을 위한 listenfd와 client와 연결하는 connfd 선언
    socklen_t clientlen;                                    // client의 주소 길이
    struct sockaddr_storage clientaddr;                     // client의 주소 정보를 저장하는 구조체
    char client_hostname[MAXLINE], client_port[MAXLINE];    // client의 host 이름과 포트 번호를 저장하기 위한 문자열 선언

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);     // 인수의 개수 체크
        exit(0);
    }

    /*
     * getnameinfo: 주어진 소켓 주소를 호스트명과 포트 번호로 변환하여 해당 정보를 버퍼에 저장하는 함수
     * getaddrinfo: 주어진 호스트명과 서비스명에 대한 정보(네트워크 주소)를 버퍼에 저장하는 함수
     * 
     * 1. 클라이언트가 서버에 연결하기 위해 getaddrinfo 함수 호출 (호스트명과 포트 번호를 입력)
     * 2. 반환된 정보는 addrinfo 구조체에 저장
     * 3. 이를 통해 클라이언트 소켓과 서버 소켓이 통신할 준비가 됨
     * 4. 서버가 클라이언트의 소켓 주소를 getnameinfo 함수에 전달하여 호스트명과 포트 번호를 가져옴
     * 5. 이 정보를 사용하여 서버는 클라이언트와의 연결을 설정하거나 수신한 데이터 처리
     */
    listenfd = Open_listenfd(argv[1]);                                  // 지정된 포트 번호에 대한 연결을 받을 수 있는 소켓을 연다. (bind & listen)
    while (1)                                                           // 무한 루프 시작
    {
        clientlen = sizeof(struct sockaddr_storage);                    // 클라이언트 주소 구조체의 크기 설정 (IPv4, IPv6 주소를 동시에 다룰 수 있음. 즉, 유연성을 위함)
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);       // 클라이언트로부터의 연결을 수락하고 클라이언트와 통신하기 위한 connfd 반환 (accept)
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);  // 클라이언트 주소를 변환하여 저장
        printf("Connected to (%s %s)\n", client_hostname, client_port);
        echo(connfd);       // 클라이언트와의 연결에 대해 echo 함수 호출
        Close(connfd);      // 클라이언트와의 연결 종료
    }
    exit(0);
}

void echo(int connfd)
{
    size_t n;
    char buf[MAXLINE];
    rio_t rio;

    Rio_readinitb(&rio, connfd);
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0)
    {
        printf("server received %d bytes\n", (int)n);
        printf("data: %s", buf);
        Rio_writen(connfd, buf, n);
    }
}