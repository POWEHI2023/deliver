#include "deliver/src/include/internet/internet.h"

#include <iostream>
#include <cstring>
#include <thread>
#include <tuple>

#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#ifndef __Z_CONNECTION
#define __Z_CONNECTION

struct z_task;
struct z_response;

template <typename T>
class z_queue;

template <typename Task, typename Response, typename ReqQue = z_queue<Task>, typename RspQue = z_queue<Response>>
class z_socket {
public:
     /**
      * used by client
      * 
      * connect with server who calls bind to listen connections from clients
      * 
      * @param ip ip address of the server
      * @param port post of the server
      * @param request consume tasks use this queue type
      * @param result return results to the client
      * @param response return responses from server to client
      * @param _stop signal for stopping current connection
      * 
      * @return none
      */
     void
     connect(const std::string& ip, uint16_t port, ReqQue& request, RspQue& result, RspQue& response, sem_t _stop) noexcept;

     /**
      * listen sockfd to send message to server or receive message from server
      * 
      * @param sockfd socket descriptor
      * @param request consume tasks use this queue type
      * @param result return results to the client
      * @param response return responses from server to client
      * 
      * @return one sockfd, two threads, [sender[requese, result], receiver[response]]
      */
     static std::tuple<int, std::thread, std::thread>
     listen(int sockfd, ReqQue& request, RspQue& result, RspQue& response) noexcept;

     /**
      * kill the two threads and close sockfd
      */
     static bool kill_thread(int sockfd, std::thread& t1, std::thread& t2) noexcept;
};

template <typename Task, typename Response, typename ReqQue, typename RspQue>
inline bool z_socket<Task, Response, ReqQue, RspQue>::kill_thread(int sockfd, std::thread &t1, std::thread &t2) noexcept
{
     pthread_t id1 = t1.native_handle(), id2 = t2.native_handle();
     pthread_cancel(id1);
     pthread_cancel(id2);
     t1.join(); t2.join();

     close(sockfd);
     return true;
}

template <typename Task, typename Response, typename ReqQue, typename RspQue>
void
z_socket<Task, Response, ReqQue, RspQue>::connect(const std::string& ip, uint16_t port, ReqQue& request, RspQue& result, RspQue& response, sem_t _stop) noexcept
{
     int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
     if (sockfd == -1) 
     {
          return {sockfd, std::thread, std::thread};
     }

     sockaddr_in addr;
     addr.sin_family = AF_INET;
     addr.sin_port = ntohs(port);
     addr.sin_addr = inet_addr(ip.c_str());

     ::connect(sockfd, (sockaddr*)&addr, sizeof(addr));

     auto [_, sender, recver] = z_socket::listen(sockfd, request, result, response);

     sem_wait(_stop);
     z_socket::kill_thread(sockfd, sender, recver);
}

template <typename Task, typename Response, typename ReqQue, typename RspQue>
inline std::tuple<int, std::thread, std::thread> 
z_socket<Task, Response, ReqQue, RspQue>::listen(int sockfd, ReqQue &request, RspQue &result, RspQue &response) noexcept
{
     return 
     {
          sockfd,
          // sender
          std::thread([&sockfd, &request, &result]() -> void{
               // TODO after finish z_task
               // parse z_task and send message to server
          }),
          // recver
          std::thread([&sockfd, &response]() -> void {
               // TODO after finish z_response
               // parse response and return as z_response
          })
     };
}

#endif