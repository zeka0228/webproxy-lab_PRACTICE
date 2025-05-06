#include <stdio.h>
#include "csapp.h"
#include <strings.h>
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

typedef struct _Cache_node
{
  char uri[MAXLINE];
  int total_length;
  char *response_ptr;
  struct _Cache_node *next;
  struct _Cache_node *prev;
}cache_node;


void parse_uri(char *uri, char *sitename, char *prtn, char *path);
void doit(int fd);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void insert_cache(char *buf, char *uri, int total_len);
cache_node *find_cache(char *uri);



extern cache_node *root;
extern cache_node *tail;
cache_node *root = NULL;
cache_node *tail = NULL;

static int can_inesrt_size;

int main(int argc, char **argv) {
  printf("%s", user_agent_hdr);

  can_inesrt_size = MAX_CACHE_SIZE;

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
  if (strcasecmp(method, "GET") != 0 && strcasecmp(method, "HEAD") != 0){ //대소문자 구분 없이 두 인자 비교, 같으면 0
    clienterror(fd, method, "501", "Not implemented", "tiny does not implement this method");
    return;
  }

  //uri 파싱하기
  parse_uri(uri,sitename,port_num,path);

  //캐시 조회하기
  cache_node *rp_cache = find_cache(uri);
  //있으면
  if(rp_cache){
    Rio_writen(fd, rp_cache->response_ptr, rp_cache->total_length);
  }

  //없으면
  else{
    //서버 접속해서 뜯어오기  
    int RP_clientfd = open_clientfd(sitename, port_num);
    
    if (RP_clientfd < 0){
        clienterror(fd, method, "502", "Bad Gateway", "Failed to establish connection with the end server");
        return;
      }
      
    int n;
    int content_len = 0;
    int total_len = 0;
    int cache_flag = 1;
    char RP_buf[MAXLINE];
    char *CH_buf = (char*)malloc(MAX_OBJECT_SIZE);
    rio_t RP_rio;

    //메모리 확보 실패 시 예외처리
    if (CH_buf == NULL) 
      cache_flag = 0;

    Rio_readinitb(&RP_rio, RP_clientfd);

    //리퀘스트 헤더 제작작
    //GET / home.html /HTTP/1.0 \r\ 으로 재설정
    sprintf(buf, "%s %s HTTP/1.0\r\n", method, path);
    sprintf(buf + strlen(buf), "Host: %s\r\n", sitename);
    //문제에서 요구한 user_agent로 삽입
    strcat(buf, user_agent_hdr);
    strcat(buf, "Connection: close\r\n");
    strcat(buf, "Proxy-Connection: close\r\n\r\n");
    Rio_writen(RP_clientfd , buf, strlen(buf));

    
    //헤더 읽기기
    while((n = Rio_readlineb(&RP_rio, RP_buf, MAXLINE)) > 0  ){
      Rio_writen(fd, RP_buf, n);

      if(cache_flag){
        if(total_len + n > MAX_OBJECT_SIZE){
          cache_flag = 0;
          free(CH_buf);
          CH_buf = NULL;
        }
        else{
          memcpy(CH_buf + total_len, RP_buf, n);
          total_len += n;
        }

      }

      //RP_buf의 앞 15바이트 확인, 같을시 0
      //atoi = str -> ASCII to Integer
      if(strncasecmp(RP_buf, "Content-Length:", 15 ) == 0){
        if (sscanf(RP_buf + 15, "%d", &content_len) != 1) {
        // 에러 처리: content_len 파싱 실패
          content_len = 0;
        }
      }

      if(strcmp(RP_buf, "\r\n") == 0) break;
    }

      //바디 읽기
      if(content_len > 0){
        char *body = malloc(content_len);

      //body 메모리 확보 실패 시
      if (body == NULL) {
        if (CH_buf) 
          free(CH_buf);
        close(RP_clientfd);
        return;
      }



      //Rio_readn(&RP_rio, body, content_len);
      Rio_readn(RP_clientfd, body, content_len);
      Rio_writen(fd, body, content_len);

      if(cache_flag) {
        if(total_len + content_len <= MAX_OBJECT_SIZE){
          memcpy(CH_buf + total_len, body, content_len);
          total_len += content_len;
        }
        else{
          cache_flag = 0;
          free(CH_buf);
          CH_buf = NULL;
        }
      }        
      free(body);  
      body = NULL;
    }



    close(RP_clientfd);
    //캐시 저장조건 체크 - 저장 가능하면
    if(cache_flag){
      insert_cache(CH_buf, uri, total_len);
      free(CH_buf);
    }

  }
  return;
}


//GET http://localhost:15213/home.html HTTP/1.1 같은 느낌으로 날아옴
void parse_uri(char *uri, char *sitename, char *prtn, char *path){
  char *srv_ptr;
  char *prtn_ptr;
  char *path_ptr;
  
  //방문 호스트 파싱
  if(strstr(uri, "//")){
    srv_ptr = strstr(uri, "//") + 2;
  }
  else
    srv_ptr = uri;

  //포트와 패스 포인터 시작 위치 
  prtn_ptr = strchr(srv_ptr,':');
  path_ptr = strchr(srv_ptr,'/');
  


  //패스 파싱
  if(path_ptr == NULL){
    strcpy(path, "/");
    path_ptr = srv_ptr + strlen(srv_ptr);
  }
  else
    strcpy(path, path_ptr);


  //포트 파싱
  if(prtn_ptr != NULL){
    prtn_ptr++;
    //-1은 '/' ':' 저장 방지 요소
    strncpy(prtn, prtn_ptr, path_ptr - prtn_ptr);
    strncpy(sitename, srv_ptr, prtn_ptr - srv_ptr -1);
    prtn[path_ptr - prtn_ptr]='\0';
    sitename[prtn_ptr - srv_ptr -1] ='\0';
  }

  //NULL이면 80 세팅이라는데 GPT 피셜이라 아니면 수정 필요
  else{
    strcpy(prtn, "80");
    strncpy(sitename, srv_ptr, path_ptr - srv_ptr);
    sitename[path_ptr - srv_ptr]='\0';
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

void insert_cache(char *buf, char *uri, int total_len){

  while(can_inesrt_size < total_len && tail != NULL){
    cache_node *deleter = tail;
    free(deleter->response_ptr);
    can_inesrt_size += deleter->total_length;

    tail = deleter->prev;

    if (tail)
        tail->next = NULL;
    else
        root = NULL;
    free(deleter);
    deleter = NULL;
  }
  
  cache_node *new_cache = malloc(sizeof(cache_node));
  if (!new_cache) return;

  
  new_cache->next = root;
  new_cache->prev = NULL;
  new_cache->total_length = total_len;

  new_cache->response_ptr = malloc(total_len);
  if (new_cache->response_ptr == NULL) {
    free(new_cache); 
    return;
  }

  memcpy(new_cache->response_ptr, buf, total_len);
  strncpy(new_cache->uri, uri, MAXLINE - 1);
  new_cache->uri[MAXLINE - 1] = '\0'; 

  if(root) 
    root->prev = new_cache;
  if(tail == NULL)
    tail = new_cache;
  
  root = new_cache;
  can_inesrt_size -= total_len;

}