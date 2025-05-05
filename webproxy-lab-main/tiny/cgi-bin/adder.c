/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  char *buf, *p, *method;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;
  if ((buf = getenv("QUERY_STRING")) != NULL){
    //QUERY_STRING="num1=12&num2=34" 로 넘어오면 첫n이 buf 시작점, &가 p의 시작점

    p = strchr(buf, '&');
    *p = '\0';
    // strcpy(arg1, buf);
    // strcpy(arg2, p+1);
    // n1 = atoi(arg1);
    // n2 = atoi(arg2);
    sscanf(buf, "num1=%d", &n1);
    sscanf(p+1, "num2=%d", &n2);
  }
  
  method = getenv("REQUEST_METHOD");

  sprintf(content, "Welcome to add.com: ");
  sprintf(content + strlen(content),
          "THE Internet addition portal.\r\n<p>");
  sprintf(content + strlen(content),
          "The answer is: %d + %d = %d\r\n<p>", n1, n2, n1 + n2);
  sprintf(content + strlen(content),
          "Thanks for visiting!\r\n");

  /* Generate the HTTP response */
  printf("Connection: close\r\n");
  printf("Content-length: %lu\r\n", strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  
  if (strcasecmp(method, "HEAD") != 0)
    printf("%s", content);
  fflush(stdout); //지금까지 쌓은 버퍼 모두 출력하기(없으면 꽉 찰떄까지 대기함;)
  
  exit(0);
}
/* $end adder */
