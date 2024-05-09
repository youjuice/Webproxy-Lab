#include "csapp.h"

int main (int argc, char **argv)                                    // argc: 입력받은 인자의 수 argv: 입력받은 인자들의 배열 (포인터 배열)
{
    int clientfd;
    char *host, *port, buf[MAXLINE];
    rio_t rio;

    if (argc != 3)                                                  // 파일 실행 시 인자를 제대로 넘겨주지 않았다면,
    {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);      // 에러 출력
        exit(0);
    }

    host = argv[1];     // 첫번째 인자: host
    port = argv[2];     // 두번째 인자: port

    clientfd = Open_clientfd(host, port);           // open_clientfd 함수를 호출하여 서버와 연결하고, 리턴받은 소켓 식별자를 clientfd에 저장 (connect)
    Rio_readinitb(&rio, clientfd);                  // rio 구조체를 초기화하고, rio를 통해 파일 디스크립터 clientfd에 대한 읽기 작업을 수행할 수 있도록 설정

    while (Fgets(buf, MAXLINE, stdin) != NULL)      // 유저에게 받은 입력을 buf에 저장
    {
        Rio_writen(clientfd, buf, strlen(buf));     // 파일 디스크립터를 통해 buf에 저장된 데이터를 서버로 전송
        Rio_readlineb(&rio, buf, MAXLINE);          // 서버로부터 응답을 읽어서 buf에 저장
        Fputs(buf, stdout);                         // 응답을 표준 출력에 출력
    }
    Close(clientfd);    // 서버와의 연결 닫음
    exit(0);
}