/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);                                                                          // 클라이언트와의 연결을 처리하고 요청을 해석하여 적절한 응답 생성
void read_requesthdrs(rio_t *rp);                                                           // 클라이언트로부터 받은 요청 헤더를 읽고 처리
int parse_uri(char *uri, char *filename, char *cgiargs);                                    // 클라이언트의 요청 URI를 분석하여 요청된 자원의 경로와 CGI 프로그램에 전달할 인자를 결정
void serve_static(int fd, char *filename, int filesize, char *method);                      // 정적 파일을 클라이언트에게 제공
void get_filetype(char *filename, char *filetype);                                          // 파일의 MIME TYPE 결정
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);                    // 동적 CGI 프로그램을 실행하고 클라이언트에게 결과 제공
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);         // 클라이언트에게 오류 메세지 제공


int main(int argc, char **argv) {                     // argc: 인자의 개수, argv: 인자 배열
  int listenfd, connfd;                               // Listening 소켓과 Connected 소켓 디스크립터
  char hostname[MAXLINE], port[MAXLINE];              // 클라이언트의 호스트명과 포트 번호
  socklen_t clientlen;                                // 클라이언트의 주소 구조체의 길이
  struct sockaddr_storage clientaddr;                 // 클라이언트의 주소 정보를 저장하는 구조체

  /* Check command line args */
  if (argc != 2) {                                    // 입력 인자가 2개가 아니라면,
    fprintf(stderr, "usage: %s <port>\n", argv[0]);   // 종료
    exit(1);
  }

  printf("현재 port 번호 : %s\n", argv[1]);
  listenfd = Open_listenfd(argv[1]);                  // Listening 소켓 오픈
  while (1) {
    clientlen = sizeof(clientaddr);                   // accept 함수 인자에 넣기 위한 주소 길이 계산
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // 클라이언트로부터의 연결을 수락하고 클라이언트와 통신하기 위한 Connected 소켓 생성
    printf("Connfd: %d\n", connfd);
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);   // 클라이언트의 주소를 변환하여 저장
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
}


/*
 * doit() - 클라이언트의 요청 라인을 확인해 정적, 동적 컨텐츠를 확인하고 처리하는 함수
 * 1. 클라이언트로부터의 요청 라인을 읽어와서 파싱 (정보 추출)
 * 2. 요청한 메서드가 "GET"이 아닌 경우 -> 501 오류 전송
 * 3. 요청 헤더를 읽어들임
 * 4. 요청된 URI를 분석하여 정적 파일인지 동적 CGI 프로그램인지 확인
 * 5. 요청된 파일이 존재하지 않는 경우 -> 404 오류 전송
 * 6. 요청된 파일에 대한 권한이 없는 경우 -> 403 오류 전송
 * 7. 요청된 파일이 정적 파일인 경우 해당 파일 전송
 * 8. 요청된 파일이 동적 파일인 경우 해당 CGI 프로그램 실행 후 결과 전송
 */
void doit(int fd)
{
  int is_static;                                                          // 요청된 자원이 정적인지 동적인지 나타내는 플래그
  struct stat sbuf;                                                       // 파일 정보를 저장하는 구조체
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];     // 요청 메세지를 저장하기 위한 문자열 배열
  char filename[MAXLINE], cgiargs[MAXLINE];                               // 요청된 파일의 경로, CGI 프로그램의 인자를 저장
  rio_t rio;                                                              // 버퍼링된 입력 스트림을 나타내는 구조체

  // Rio = Robust I/O (오류 처리를 내장하고 있어 입출력 동작 중 발생할 수 있는 다양한 예외 상황 처리 가능)
  // 요청 라인을 읽고 분석
  Rio_readinitb(&rio, fd);                          // 버퍼를 초기화하고 fd(데이터 소스)와 연결 -> 데이터를 버퍼에 쌓아두고 한번에 처리하기 위함. 
  Rio_readlineb(&rio, buf, MAXLINE);                // 버퍼에서 한줄 읽어들이고 이를 buf에 저장
  printf("Request headers:\n");
  printf("%s", buf);                                // 요청된 라인을 보여줌 (ex. GET / HTTP / 1.1)
  sscanf(buf, "%s %s %s", method, uri, version);    // buf의 내용을 각각 method, uri, version에 저장

  // 요청된 메서드가 GET이 아니면 (과제 11.11 "HEAD" 메서드 추가)
  if (!(strcasecmp(method, "GET") || strcasecmp(method, "HEAD")))            // strcasecmp: 문자열을 대소문자 구분 없이 비교하는 함수 (같으면 0, 다르면 0이 아닌 값 반환)
  {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");   // 501 오류
    return;
  }

  /*
   * ▶ 요청 라인
   * - HTTP 요청의 첫번째 줄로, 클라이언트가 서버로 보내는 요청에 대한 기본 정보를 포함
   * - HTTP 메서드, 요청 URI, HTTP 버전
   * ▶ 요청 헤더
   * - HTTP 요청의 두번째 줄부터 빈 줄이 나오기 전까지의 모든 줄, 클라이언트가 요청에 추가 정보를 제공할 때 사용
   * - Host, User-Agent(클라이언트 프로그램의 정보), Accept(클라이언트가 원하는 응답 콘텐츠 타입), Content Type 등
   */
  read_requesthdrs(&rio);     // 요청 라인을 제외한 요청 헤더를 읽음 (tiny에서는 요청 라인만 읽음)

  // URI를 filename과 CGI argument를 파싱하고
  // request가 static인지 dynamic인지 확인하는 플래그를 리턴 (1이면 static, 0이면 dynamic)
  is_static = parse_uri(uri, filename, cgiargs);

  /*
   * stat() : 주어진 파일의 정보를 가져오는 함수
   * - 파일의 경로를 입력받아서 파일 정보를 stat 구조체에 저장
   * - stat 구조체는 파일의 다양한 속성(크기, 권한, 생성 시간 등)을 포함함
   * - 성공하면 0, 실패하면 -1 반환
   */
  // 파일 정보를 가져오고, 파일이 존재하지 않으면
  if (stat(filename, &sbuf) < 0)
  {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");              // 404 오류
    return;
  }

  /*
   * 파일 모드 조작 및 테스트 관련 매크로
   * S_ISERG : 파일 모드 확인 -> 일반(regular) 파일인지 판별
   * S_IRUSR : 파일의 소유자가 읽기 권한을 가지고 있는지 판별
   * S_IXUSR : 파일의 소유자가 해당 파일을 실행할 수 있는지 판별
   */
  // 정적 자원을 요청했을 때
  if (is_static)
  {
    // 일반 파일이 아니거나 읽기 권한이 없다면
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");             // 403 오류
      return;
    }
    serve_static(fd, filename, sbuf.st_size, method);       // 정적 파일 제공
  }

  // 동적 자원을 요청했을 때
  else
  {
    // 일반 파일이 아니거나 실행 권한이 없다면
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");       // 403 오류
      return;
    }
    serve_dynamic(fd, filename, cgiargs, method);           // 동적 파일 제공
  }
}


// 웹 서버에서 오류가 발생했을 때 클라이언트에게 오류를 알리는 함수
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  // sprintf() - 형식화된 문자열을 생성하여 주어진 버퍼에 저장
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server<\em>\r\n", body);

  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));   // 위의 body 내용 출력
}


/*
 * read_requesthdrs() : 클라이언트로부터 수신된 HTTP 요청 헤더를 읽고 출력하는 함수
 * - Tiny 서버는 request header의 정보를 사용하지 않음
 * - 요청 라인 한줄만 저장하고 요청 헤더들은 그냥 출력
 */
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  // 버퍼의 한줄을 읽어들이고 buf에 저장
  Rio_readlineb(rp, buf, MAXLINE);

  // buf에 "\r\n"과 같은 문자열이 있다면(마지막 줄에 도달했다는 뜻!!) while문 탈출
  // 그 전까지 한 줄씩 출력 (buf는 새로운 문자열을 쓸 때마다 기존의 내용을 덮어씀)
  while(strcmp(buf, "\r\n"))
  {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}


/*
 * parse_uri() : 주어진 URI를 분석하여 해당하는 파일 이름과 CGI 인자를 추출하는 함수
 * - Tiny 서버는 정적 컨텐츠를 위한 홈 디렉토리가 자신의 현재 디렉토리이고, 실행 파일의 홈 디렉토리는 /cgi-bin이라 가정
 * - 인자는 uri(분석할 URI 문자열), filename(URI에 해당하는 파일 이름이 저장될 문자열), cgiargs(URI에 포함된 CGI 인자가 저장될 문자열)
 */
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  // URI에 "cgi-bin" 문자열이 없는 경우 -> 정적 파일 요청
  /* 
   * uri: /index.html
   * filename: ./index.html
   * cgiargs: 
   */
  if (!strstr(uri, "cgi-bin"))        // strstr() : 문자열에서 특정 부분 문자열이 처음으로 나타나는 위치를 반환
  {
    strcpy(cgiargs, "");              // cgi 인자를 빈 문자열로 설정

    strcpy(filename, ".");            // filename에 현재 디렉토리 경로 설정
    strcat(filename, uri);            // filename -> "." + "/index.html"
    
    if (uri[strlen(uri) - 1] == '/')  // URI가 "/" 문자열로 끝난다면,
      strcat(filename, "home.html");  // 기본 파일 이름인 home.html을 추가
    
    return 1;                         // static이므로 1 return
  }

  // URI에 "cgi-bin" 문자열이 존재한다면 -> 동적 파일 요청
  /* 
   * uri: /cgi-bin/adder?param1=value1&param2=value2
   * filename: ./cgi-bin/adder
   * cgiargs: param1=value1&param2=value2
   */
  else
  {
    ptr = index(uri, '?');          // index() : 문자열에서 특정 문자가 처음으로 나타나는 위치를 찾아주는 함수
    if (ptr)                  
    {
      strcpy(cgiargs, ptr + 1);     // "?" 문자 다음 위치부터 cgi 인자에 복사
      *ptr = '\0';                  // ptr 포인터에 NULL 문자를 넣어서 URI에서 cgi 인자 제거
    }
    else
      strcpy(cgiargs, "");          // ptr이 NULL이라면 cgi 인자를 빈 문자열로 설정

    strcpy(filename, ".");          // filename에 경로 설정
    strcat(filename, uri);

    return 0;                       // dynamic이므로 0 return
  }
}


// 정적 파일을 클라이언트에게 제공하는 함수
void serve_static(int fd, char *filename, int filesize, char *method)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");                          // HTTP 응답 헤더 생성 (요청 성공)
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);           // 서버 정보를 헤더에 추가
  sprintf(buf, "%sConnection: close\r\n", buf);                 // "연결을 닫음" 헤더에 추가
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);      // 파일의 크기를 헤더에 추가
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);    // 파일의 유형을 헤더에 추가

  Rio_writen(fd, buf, strlen(buf));                             // 생성된 헤더를 클라이언트에 보냄 (클라이언트 소켓에 저장)
  printf("Response headers:\n");                                // 헤더 출력
  printf("%s", buf);

  if (strcasecmp(method, "GET") == 0)
  {
    /*
    * Mmap() : 파일을 메모리에 직접 매핑함으로써 입출력 오버헤드를 줄이고, 파일에 대한 접근을 간편하게 함
    * O_RDONLY : 파일을 읽기 전용으로 열기
    * PROT_READ : 메모리를 읽기 전용으로 매핑
    * MAP_PRIVATE : 파일을 메모리에 private으로 매핑 (다른 프로세스와 메모리를 공유하지 않고 현재 프로세스의 가상 주소 공간에만 파일 매핑)
    */
    srcfd = Open(filename, O_RDONLY, 0);                             // 주어진 파일을 열고 srcfd에 저장 (srcfd - 서버가 파일을 읽기 위해 사용하는 fd)
    // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);   // 파일을 메모리로 매핑하고 매핑된 메모리의 시작주소를 저장
    srcp = (char *)malloc(filesize);                                 // filesize 만큼 메모리 동적 할당
    Rio_readn(srcfd, srcp, filesize);                                // srcp 버퍼에 filesize 만큼 srcfd의 데이터를 저장

    Close(srcfd);                                                    // srcfd를 닫음
    Rio_writen(fd, srcp, filesize);                                  // 메모리 매핑한 파일을 클라이언트에게 보냄 (클라이언트 소켓에 저장)
    // Munmap(srcp, filesize);                                       // 메모리 매핑 해제
    free(srcp);                                                      // 메모리 해제
  }
}


// filename으로부터 filetype을 추출하는 함수 (Tiny는 4개의 파일 형식만 지원)
void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");
}


// 동적 CGI 프로그램을 실행하고 클라이언트에게 결과를 전송하는 함수
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method)
{
  char buf[MAXLINE], *emptylist[] = { NULL };

  sprintf(buf, "HTTP/1.0 200 OK\r\n");            // HTTP 응답 헤더 생성
  Rio_writen(fd, buf, strlen(buf));               // 클라이언트에게 보냄 
  sprintf(buf, "Server: Tiny Web Server\r\n");    // 서버 정보 헤더에 추가
  Rio_writen(fd, buf, strlen(buf));               // 클라이언트에게 보냄

  if (Fork() == 0)                                // 자식 프로세스 생성 (별개의 프로세스 작업 & 자원 공유를 위해)
  {
    setenv("QUERY_STRING", cgiargs, 1);           // QUERY_STRING 환경변수를 cgiargs로 설정
    setenv("REQUEST_METHOD", method, 1);          // REQUEST_METHOD 환경변수를 method로 설정
    Dup2(fd, STDOUT_FILENO);                      // 표준 출력을 클라이언트 소켓으로 재지정 (CGI 프로그램의 출력이 클라이언트에게 바로 전송)
    Execve(filename, emptylist, environ);         // filename으로 지정된 CGI 프로그램 실행 
  }
  Wait(NULL);                                     // 부모 프로세스가 자식 프로세스의 종료를 기다림
}
