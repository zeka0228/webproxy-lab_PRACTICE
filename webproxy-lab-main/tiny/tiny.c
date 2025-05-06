/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"
#include <strings.h>

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv) {
  signal(SIGPIPE, SIG_IGN); // 끊김 방지지
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
}


void doit(int fd){
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  //요청 라인 + 헤더 읽기
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);

  printf("리퀘스트 헤더 : \n");
  printf("%s", buf);  
  sscanf(buf, "%s %s %s", method, uri, version);
  if(strcasecmp(method, "GET") * strcasecmp(method, "HEAD")){ //대소문자 구분 없이 두 인자 비교, 같으면 0
    clienterror(fd, method, "501", "Not implemented", "tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio);
  
  // GET 요청에서 URI 추출하기
  is_static = parse_uri(uri, filename, cgiargs);
  if(stat(filename, &sbuf) < 0){ // 파일 정보 가져와 지는지 체크, 실패 시 -1
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  //정적 콘텐츠 요청일 때
  if(is_static){

    //S_ISREG : 정규 파일(regular file = .html or .txt or .jpg)인지 검사, 디렉토리,소캣,장치파일 등은 FALSE
    //S_IRUSR : 400 설정, 소유자에게 읽기 권한이 있는지 확인 
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    //정규 콘텐츠 반환
    serve_static(fd, filename, sbuf.st_size, method);
  }
  
  //동적 콘텐츠일 때
  else{
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }

    //동적 콘텐츠 반환
    serve_dynamic(fd, filename, cgiargs, method);
  }
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg){
  char buf[MAXLINE], body[MAXBUF];
  
  //sprintf = 출력 X, 우측을 좌측 버퍼에 덮어쓰기 방식으로 저장, 앞에 매번 body를 두는것도 그 때문문
  sprintf(body, "<html><title>Tiny error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s : %s\r\n",body, errnum,shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

 /* Print the HTTP response */
 sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
 Rio_writen(fd, buf, strlen(buf));
 sprintf(buf, "Content-type: text/html\r\n");
 Rio_writen(fd, buf, strlen(buf));
 sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
 Rio_writen(fd, buf, strlen(buf));
 Rio_writen(fd, body, strlen(body));
}



//헤더 내용 무시 함수 : 바로 body에서 시작할 수 있게 빈 줄 나올때까지 패스 
void read_requesthdrs(rio_t *rp){
  char buf[MAXLINE];
  Rio_readlineb(rp,buf,MAXLINE);
  //빈 줄은 '\r \n'으로만 구성
  while(strcmp(buf, "\r\n")){
    Rio_readlineb(rp,buf,MAXLINE);
    printf("%s",buf);
  }
  return;

}


int parse_uri(char *uri, char *filename, char *cgiargs){
  char *ptr;

  //strstr : 우측 부분 문자열이 들어있는 포인터 째로 반환
  if(!strstr(uri, "cgi-bin")){
    //cgiargs 초기화
    strcpy(cgiargs, "");
    // . + /images/cat.png = ./images/cat.png
    strcpy(filename, ".");
    strcat(filename, uri);

    //uri에 아무것도 없으면  
    if (uri[strlen(uri)-1] == '/')
      strcat(filename, "home.html");
    return 1;
  }

  //동적 콘텐츠일 때
  else{
    //우측 문자열의 첫 위치 포인터를 반환환
    ptr = index(uri, '?');
    if(ptr) {
      strcpy(cgiargs, ptr+1);
      *ptr = '\0';
    }

    //?가 없을 경우 CGI 인자가 없다는 뜻 -> 파일까지만 만들기, cgiargs 빈 문자열로 설정
    else
      strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;


  }
}

//정적 콘텐츠 처리
void serve_static(int fd, char *filename, int filesize, char *method){
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];
  int len = 0;

  //응답 헤더 만들기
  //get filetype => MIME 타입 찾아옴
  get_filetype(filename, filetype);
  len += sprintf(buf + len, "HTTP/1.0 200 OK\r\n");
  len += sprintf(buf + len, "Server: Tiny Web Server\r\n");
  len += sprintf(buf + len, "Connection: close\r\n");
  len += sprintf(buf + len, "Content-length: %d\r\n", filesize);
  len += sprintf(buf + len, "Content-type: %s\r\n\r\n", filetype);

  Rio_writen(fd, buf, len);
  printf("Response headers:\n%s", buf);

  if (strcasecmp(method, "HEAD") == 0)
      return;
  //응답 바디 만들기

  //filenmae 경로로, O_RDONLY = 읽기 전용으로 열기, 최우측은 파일 생성 모드(=읽기 전용이라 0)
  srcfd = Open(filename, O_RDONLY, 0);

  //0 : 주소 지정 x(커널이 자동 선택) filsize만큼 READ 가능하게 매핑, 읽기 전용 사본 매핑(PRIVATE = 디스크 반영 X)으로 srcfd 매핑 (오프셋 0 = 처음부터 매핑)
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  
  srcp = (char*)malloc(filesize);
  Rio_readn(srcfd, srcp, filesize);


  Close(srcfd);

  Rio_writen(fd, srcp, filesize);

  //매핑 해제
  // Munmap(srcp, filesize);

  free(srcp);
}


void get_filetype(char *filename, char *filetype){
  
//포함된 문자열로 타입 판단
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


void serve_dynamic(int fd, char *filename, char *cgiargs, char *method){

  char buf[MAXLINE], *emptylist[] = {NULL};

  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));
  

  //자식 프로세스 생성
  if(Fork() == 0){

    //환경변수 설정 : QUERY_STRING에 cgiargs 저장, 덮어쓰기 여부(1이면 덮어쓰기)
    setenv("QUERY_STRING", cgiargs, 1);
    setenv("REQUEST_METHOD", method, 1);

    //oldfile(fd)를 newfile(STDOUT_FILENO(1로 매크로))로 덮어쓰기 + 기존 newfile이 가리키던 파일 디스크립터는 닫힘 = 리다이랙션
    //기존 1이 터미널이어서, 터미널(콘솔) 출력이 이루어졌다면, 해당 fd가 덮어씌워졌으니 클라에게 직접 전송됨
    Dup2(fd, STDOUT_FILENO);
    //CGI 프로그램 실행
    Execve(filename, emptylist, environ);
  }
  //자식 프로세스 reap 될때까지 대기
  Wait(NULL);
}


