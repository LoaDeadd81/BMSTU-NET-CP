#ifndef BMSTU_NET_CP_REQUEST_H
#define BMSTU_NET_CP_REQUEST_H

#define GET_STR "GET"
#define HEAD_STR "HEAD"

#define HTTP11_STR "HTTP/1.1"
#define HTTP10_STR "HTTP/1.0"

#define URL_LEN 128

typedef enum request_method_t {
    BAD, GET, HEAD
} request_method_t;


typedef struct request_t {
    request_method_t method;
    char url[URL_LEN];
} request_t;

int parse_req(request_t *req, char *buff);

#endif //BMSTU_NET_CP_REQUEST_H
