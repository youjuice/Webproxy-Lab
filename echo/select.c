#include "csapp.h"
void echo(int connfd);
void command(void);

int main(int argc, char **argv)
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    fd_set read_set, ready_set;

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }
    listenfd = Open_listenfd(argv[1]);

    FD_ZERO(&read_set);                     // read_set 초기화
    FD_SET(STDIN_FILENO, &read_set);        // 표준 입력을 read_set에 추가
    FD_SET(listenfd, &read_set);            // 리스닝 소켓을 read_set에 추가

    while(1)
    {
        ready_set = read_set;                                               // read_set : select 함수에 의해 감시될 fd 집합 (감시 대상 지정)
        Select(listenfd+1, &ready_set, NULL, NULL, NULL);                   // ready_set : select 함수 호출 후에 fd 상태를 나타내는 집합 (감시한 대상 중에 어떤 것들이 입력을 기다리고 있는지)
        if (FD_ISSET(STDIN_FILENO, &ready_set))                             // 만약 표준 입력에서 이벤트가 발생하면 command 함수 호출
            command();
        if (FD_ISSET(listenfd, &ready_set))                                 // 만약 리스닝 소켓에서 이벤트가 발생하면 
        {
            clientlen = sizeof(struct sockaddr_storage);
            connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);       // 클라이언트 연결을 수락하고
            echo(connfd);                                                   // echo 함수 호출
            Close(connfd);
        }
    }
}

void command(void)
{
    char buf[MAXLINE];
    if (!Fgets(buf, MAXLINE, stdin))    
        exit(0);
    printf("%s", buf);
}