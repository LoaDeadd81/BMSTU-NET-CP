cmake_minimum_required(VERSION 3.26)
project(BMSTU_NET_CP C)

set(CMAKE_C_STANDARD 11)

set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -fPIE -pie -g")

add_executable(BMSTU_NET_CP main.c
        http/server.c
        http/server.h
        http/net.c
        http/net.h
        tpool/tpool.c
        tpool/tpool.h
        tpool/queue.c
        tpool/queue.h
        tpool/task.h
        http/request.c
        http/request.h
        http/responses_str.h
        http/content_type.h
        http/log.c
        http/log.h
        http/content_type.c
)

add_executable(client
        client.c
)
