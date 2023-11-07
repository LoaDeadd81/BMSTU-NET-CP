#ifndef BMSTU_NET_CP_HTTP_ERR_H
#define BMSTU_NET_CP_HTTP_ERR_H

enum tpool_err_code{
    HTTP_NO_ERR = 0,
    SOCK_CREATE_ERR = 1,
    BIND_ERR = 2,
    LISTEN_ERR = 3,
    ACCEPT_ERR = 3,
};

#endif //BMSTU_NET_CP_HTTP_ERR_H
