#include "csapp.h"

void echo(int connfd);
void *thread(void *vargp);

int main(int argc, char **argv)
{
    int listenfd, *connfdp;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;                          // 새로운 스레드 ID를 저장하기 위한 변수

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }
    listenfd = Open_listenfd(argv[1]);

    while(1)
    {
        clientlen = sizeof(struct sockaddr_storage);                    
        connfdp = Malloc(sizeof(int));                                  // 새로운 클라이언트와 통신을 위한 fd를 할당하기 위해 메모리 동적 할당 
        *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);     // 클라이언트의 연결 요청을 수락하고 저장
        Pthread_create(&tid, NULL, thread, connfdp);                    // 새로운 스레드를 생성하여 클라이언트와의 통신을 담당하는 함수 thread 실행
    }
}

void *thread(void *vargp)
{
    int connfd = *((int *)vargp);           // 스레드의 인자로 전달된 클라이언트와의 통신을 위한 fd를 가져옴
    Pthread_detach(pthread_self());         // 현재 스레드를 떼어냄 (해당 스레드의 종료 상태를 부모 스레드가 기다리지 않고 자동으로 회수하도록 함)
    Free(vargp);                            // 메모리 해제
    echo(connfd);                           // echo 함수 호출
    Close(connfd);                          // 통신이 끝났으므로 소켓 닫음
    return NULL;
}