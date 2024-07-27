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

#include "serializable.h"

#ifndef __Z_CONNECTION
#define __Z_CONNECTION

struct z_task;
struct z_response;

template <typename T>
class z_queue;

template <typename T, typename string_type = std::string>
concept can_to_string = requires (T __case, string_type __str) 
{
     // have function to_string and return type is string_type
     std::is_same_v<decltype(__case.to_string()), string_type>;

     // string_type should have c_str() to transfer to const char* or char*
     std::is_same_v<decltype(__str.c_str()), const char*> || std::is_same_v<decltype(__str.c_str()), char*>;
};

template <typename T>
concept is_container_type = is_serializable<T> && is_deserializable<T>;

template <typename Task, typename Response, typename ReqQue = z_queue<Task>, typename RspQue = z_queue<Response>>
// requires can_to_string<Task> && can_to_string<Response>
requires is_container_type<Task> && is_container_type<Response>
class z_socket 
{
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
     addr.sin_addr.s_addr = inet_addr(ip.c_str());

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
               while (1) 
               {
                    if (request.size() != 0)
                    {
                         Task task = request.pop();

                         size_t msg_len = 0;
                         char* _msg = task.serialize(&msg_leg);

                         char* msg = (char*)malloc(msg_len + sizeof(size_t));
                         memcpy(msg, &msg_len, sizeof(size_t));
                         memcpy(msg + sizeof(size_t), _msg, msg_len);

                         int ret = send(sockfd, msg, msg_len + sizeof(size_t), 0);
                         if (ret == -1)
                         {
                              request.push(std::move(task));
                              // push into task queue and wait next time to send
                         }

#pragma region TODO
                         // TODO: return result into result queue
                         /*result.push({
                              .task_ip = task.id,
                              .status = Task::OK,
                              .message = ""
                         });*/

                         free(msg);
                    }
               }
          }),
          // recver
          std::thread([&sockfd, &response]() -> void {
               // TODO after finish z_response
               // parse response and return as z_response
               std::string message {""};
               message.reserve(4096);
               char buffer[4096] = {0};  // 4KB buffer
               size_t remain = 0;

               char num[sizeof(size_t)];
               size_t nump = 0;
               
               auto to_response = [&response, &message]()
               {
                    char* data = message.c_str();

                    Response resp = Response::deserilize(data);
                    response.push(std::move(resp));

                    message.clear();
               };

               while (1) 
               {
                    size_t ret = recv(sockfd, buffer, 4096, 0);

                    size_t ptr = 0;
                    while (ptr < ret)
                    {
                         if (remain != 0)
                         {
                              message += buffer[ptr];
                              remain--;
                         }
                         else 
                         {
                              if (message.size() != 0)
                              {
                                   to_response();
                              }

                              if (nump < sizeof(size_t))
                              {
                                   num[nump] = buffer[ptr];
                                   nump++;
                              }

                              if (nump == sizeof(size_t))
                              {
                                   remain = *static_cast<size_t*>(num);
                                   memset(num, 0, sizeof(size_t));
                                   nump = 0;
                              }

                         }

                         ptr++;
                    }

                    if (remain == 0 && message.size() != 0)
                    {
                         to_response();
                    }

                    memset(buffer, 0, 4096);
               }
          })
     };
}

#endif

#pragma region Describtion of queue

/**
 * task queue & response queue
 * => functions
 * 
 * return the first element if exist, or throw an exception
 * template <typename T>
 * T queue::pop();
 * 
 * push an element into queue
 * template <typename T>
 * void queue::push(T&&);
 * 
 * check the size of current queue
 * size_t queue::size();
 */