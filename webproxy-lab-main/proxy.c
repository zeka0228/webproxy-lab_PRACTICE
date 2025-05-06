#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

void parse_uri(char *uri, char *sitename, char *prtn, char *path);
void doit(int fd);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
cache_node *find_cache(char *uri);

typedef struct _Cache_node
{
  char uri[MAXLINE];
  int content_length;
  char *response_ptr;
  struct _Cache_node *next;
  struct _Cache_node *prev;
}cache_node;

extern cache_node *root ;
extern cache_node *tail;


int main(int argc, char **argv) {
  printf("%s", user_agent_hdr);
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
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);  
    Close(connfd);  
  }
}

void doit(int fd){
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], path[MAXLINE];
  char sitename[MAXLINE], port_num[MAXLINE];
  rio_t rio;
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers:\n %s\n", buf);

  //GET http://localhost:15213/home.html HTTP/1.1 같은 느낌으로 날아옴
  //버전 저장 안할거임, 어떻게 날아오든 우린 1.0으로 쏜다
  sscanf(buf, "%s %s",method, uri);
  if(strcasecmp(method, "GET") * strcasecmp(method, "HEAD")){ //대소문자 구분 없이 두 인자 비교, 같으면 0
    clienterror(fd, method, "501", "Not implemented", "tiny does not implement this method");
    return;
  }

  //uri 파싱하기
  parse_uri(uri,sitename,port_num,path);

  //캐시 조회하기
  cache_node *rp_cache = find_cache(uri);
  //있으면
  if(rp_cache){

  }

  //없으면
  else{
    //서버 접속해서 뜯어오기

    //캐시 저장조건 체크 - 저장 가능하면

    //불가하면 

    //전달
  }
   
}


//GET http://localhost:15213/home.html HTTP/1.1 같은 느낌으로 날아옴
void parse_uri(char *uri, char *sitename, char *prtn, char *path){
  char *srv_ptr;
  char *prtn_ptr;
  char *path_ptr;
  
  if(strstr(uri, "//")){
    srv_ptr = strstr(uri, "//") + 2;
  }
  else
    srv_ptr = uri;

  //포트와 패스 포인터 시작 위치 
  prtn_ptr = strchr(srv_ptr,":") + 1;
  path_ptr = strchr(srv_ptr,"/") + 1;
  

  if(prtn_ptr != NULL){
    //-1은 '/' ':' 저장 방지 요소
    strncpy(prtn, prtn_ptr, path_ptr - prtn_ptr -1);
    strncpy(sitename, srv_ptr, prtn_ptr - srv_ptr -1);
  }

  //NULL이면 80 세팅이라는데 GPT 피셜이라 아니면 수정 필요
  else{
    strcpy(prtn, '80');
    strncpy(sitename, srv_ptr, path_ptr - srv_ptr -1);
  }

  strcpy(path, path_ptr);
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

cache_node *find_cache(char *uri){

  cache_node *cur = root;
  while (cur != NULL){
    if(strcmp(uri, cur->uri) == 0){
      if(cur == root)
        return cur;

      //tail일시 최신화
      if(cur == tail){
        tail = cur->prev;
        cur->prev->next = NULL;
      }
      //아닐 시, prev next 연결
      else{
        cur->next->prev = cur->prev;
        cur->prev->next = cur->next;
      }

      //root 최신화
      cur->prev = NULL;
      cur->next = root;
      root->prev = cur;
      root = cur;

      return cur;
    }
    cur = cur->next;
  }
  
  return NULL;
}