#include "csapp.h"

typedef struct {
    int maxfd;                          // 가장 큰 fd 번호 (총 개수 = maxfd + 1)
    fd_set read_set;                    // 읽기 가능한 상태를 확인할 fd 세트
    fd_set ready_set;                   // 실제 읽기 가능한 상태가 확인된 fd 세트
    int nready;                         // ready_set 개수
    int maxi;                           // 현재 활성화 된 가장 큰 인덱스 값
    int clientfd[FD_SETSIZE];           // 클라이언트 fd를 저장하는 배열
    rio_t clientrio[FD_SETSIZE];        // 클라이언트와 관련된 버퍼 및 입출력 관련 정보 저장
} pool;

int byte_cnt = 0;

int main(int argc, char **argv)
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    static pool pool;

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }
    listenfd = Open_listenfd(argv[1]);
    init_pool(listenfd, &pool);             // pool 구조체를 초기화하고 첫번째 리스닝 소켓을 세트에 추가

    while(1)
    {
        /*
         * select(nfds, readfds, writefds, exceptfds, timeout)
         * - 함수 인자: nfds(모니터링할 fd의 최대값+1), readfds(읽기 가능한 상태를 확인할 fd_set), 
         *   writefds(쓰기 가능한 상태를 확인할 fd_set), exceptfds(예외 상태를 확인할 fd_set), timeout(블록되는 최대 시간 지정)
         * - 성공하면 감시 중인 파일 디스크립터의 상태 변경을 확인하고, 실패하면 -1 반환
         */
        pool.ready_set = pool.read_set;     
        pool.nready = Select(pool.maxfd + 1, &pool.ready_set, NULL, NULL, NULL);

        if(FD_ISSET(listenfd, &pool.ready_set))                         // 리스닝 소켓에 이벤트가 발생했는지 (리스닝 소켓이 준비되었다는 것을 의미) 확인하고
        {
            clientlen = sizeof(struct sockaddr_storage);                
            connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);   // 연결 수락하고 Connected 소켓 연결 
            add_client(connfd, &pool);                                  // Pool에 추가
        }

        check_clients(&pool);                                           // 현재 pool에 있는 모든 클라이언트에 대해 읽기 가능한 소켓이 있는지 체크
    }
}

void init_pool(int listenfd, pool *p)
{
    int i;
    p->maxi = -1;                       // 활성화된 클라이언트 소켓 중 가장 큰 인덱스를 -1로 초기화

    for (i = 0; i < FD_SETSIZE; i++)    // 클라이언트 소켓을 나타내는 배열을 모두 -1로 초기화
        p->clientfd[i] = -1;
    
    p->maxfd = listenfd;                // 현재 관찰 중인 fd의 최대값을 listenfd로 설정 (select 함수가 관찰할 fd의 범위 설정 가능)
    FD_ZERO(&p->read_set);              // read_set 초기화
    FD_SET(listenfd, &p->read_set);     // listenfd를 read_set에 추가 (select 함수가 이벤트 감지 가능)
}

void add_client(int connfd, pool *p)
{
    int i;
    p->nready--;                                            // ready socket 수를 줄임
    for (i = 0; i < FD_SETSIZE; i++)                        // fd_set 순회하면서 체크
    {
        if (p->clientfd[i] < 0)                             // clientfd 배열에서 비어 있는 위치를 찾음
        {
            p->clientfd[i] = connfd;                        // 비어있는 위치에 connfd 저장
            Rio_readinitb(&p->clientrio[i], connfd);        // 해당 클라이언트에 대한 rio 구조체 초기화

            FD_SET(connfd, &p->read_set);                   // connfd를 read_set에 추가

            if (connfd > p->maxfd)                          // connfd가 maxfd보다 크다면
                p->maxfd = connfd;                          // maxfd 업데이트
            if (i > p->maxi)                                // connfd의 인덱스가 maxi보다 크다면
                p->maxi = i;                                // maxi 업데이트
            break;
        }
    }
    if (i == FD_SETSIZE)                                    // 모든 클라이언트 슬롯이 이미 사용중이라면
        app_error("add_client error: Too many clients");    // 오류 발생
}

void check_clients(pool *p)
{
    int i, connfd, n;
    char buf[MAXLINE];
    rio_t rio;

    for (i = 0; (i <= p->maxi) && (p->nready > 0); i++)         // 모든 소켓들에 대해
    {
        connfd = p->clientfd[i];
        rio = p->clientrio[i];
        if((connfd > 0) && (FD_ISSET(connfd, &p->ready_set)))   // 만약 소켓이 읽기 가능하다면
        {
            p->nready--;
            if((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0)    // 데이터를 읽고
            {
                byte_cnt += n;
                printf("Server received %d (%d total) bytes on fd %d\n", n, byte_cnt, connfd);  
                Rio_writen(connfd, buf, n);                     // 읽은 데이터를 클라이언트에게 다시 보냄
            }
            else
            {
                Close(connfd);                                  // 읽은 데이터가 없다면 소켓을 닫고
                FD_CLR(connfd, &p->read_set);                   // read_set에서 소켓 제거
                p->clientfd[i] = -1;                            // 클라이언트 소켓 배열에서 해당 소켓 초기화
            }
        }
    }
}