/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) 
{
  char *buf, *p, *method;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;

  if ((buf = getenv("QUERY_STRING")) != NULL) {
    p = strchr(buf, '&');       // 문자열에서 &의 위치를 읽어옴
    *p = '\0';                  // & 문자를 NULL 문자로 변경
    // strcpy(arg1, buf);          // 첫번째 인자 읽어와서 저장
    // strcpy(arg2, p+1);          // 두번째 인자 읽어와서 저장
    
    // n1 = atoi(arg1);            // string을 정수로 변환하는 작업
    // n2 = atoi(arg2);
    sscanf(buf, "n1=%d", &n1);
    sscanf(p+1, "n2=%d", &n2);
  }

  method = getenv("REQUEST_METHOD");
  sprintf(content, "QUERY_STRING=%s", buf);
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n2, n1+n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);

  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n"); 
  
  if (strcasecmp(method, "GET") == 0)
    printf("%s", content);
  
  fflush(stdout);               // 표준 출력 버퍼를 비워주는 함수

  exit(0);
}
