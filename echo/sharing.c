#include "csapp.h"
#define N 2
void *thread(void *vargp);
char **ptr;

int main()
{
    int i;
    pthread_t tid;                                          // 스레드 식별자를 저장할 변수 선언
    char *msgs[N] = {                                       // 스레드에서 출력할 메세지 저장
        "Hello from foo",
        "Hello from bar"
    };

    ptr = msgs;                                             // ptr이라는 전역 변수에 msgs 배열의 주소 저장
    for (i = 0; i < N; i++)
        Pthread_create(&tid, NULL, thread, (void *)i);      // 새로운 스레드 생성
    Pthread_exit(NULL);                                     // 메인 스레드 종료 (생성된 모든 스레드도 함께 종료)
}

void *thread(void *vargp)
{
    int myid = (int)vargp;                                  // 전달된 인자를 정수로 변환하여 myid 변수(각 스레드의 ID)에 저장
    static int cnt = 0;
    printf("[%d]: %s (cnt=%d)\n", myid, ptr[myid], ++cnt);  
    return NULL;
}