// This autogenerated skeleton file illustrates how to build a server.
// You should copy it to another filename to avoid overwriting it.

#include "match_server/Match.h"
#include "save_client/Save.h"
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>


#include <iostream>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;

using namespace ::match_service; //match.thrift的命名空间
using namespace ::save_service;  //save.thrift的命名空间
using namespace std;

struct Task
{
    User user;
    string type;
};

struct MessageQueue
{
    queue<Task> q;
    mutex m;
    condition_variable cv;
}message_queue;


//user 匹配池
class Pool {
    public:
        // save match result
        void save_result(int a, int b)
        {
            printf("Match Result: %d %d\n", a, b);
            std::shared_ptr<TTransport> socket(new TSocket("123.57.47.211", 9090)); //远程服务器
            std::shared_ptr<TTransport> transport(new TBufferedTransport(socket));
            std::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
            SaveClient client(protocol);

            try {
                transport->open();

                int res = client.save_data("acs_4464", "c7b88681", a, b);

                if(!res) puts("success");
                else puts("failed");

                transport->close();
            } catch (TException& tx) {
                cout << "ERROR: " << tx.what() << endl;
            }
        }

        // match users version 2.0
        void match()
        {
            while (users.size() > 1)
            {
                sort(users.begin(), users.end(), [&](User &a, User &b){
                    return a.score < b.score;
                        });

                bool flag = true;

                for (uint32_t i = 1; i < users.size(); i ++)
                {
                    auto a = users[i - 1], b = users[i];
                    if(b.score - a.score <= 50)
                    {
                        users.erase(users.begin() + i - 1, users.begin() + i + 1);  //左闭右开
                        save_result(a.id, b.id);
                        flag = false;
                        break;
                    }
                }

                if (flag) break; //没有能匹配到的，退出匹配的循环
            }
        }

        // add user
        void add(User user)
        {
            users.push_back(user);
        }

        // remove user
        void remove(User user)
        {
            for (uint32_t i = 0; i < users.size(); i ++)
            {
                if (users[i].id == user.id)
                {
                    users.erase(users.begin() + i );
                    break;
                }
            }
        }

    private:
        vector<User> users;

}pool;


class MatchHandler : virtual public MatchIf {
    public:
        MatchHandler() {
            // Your initialization goes here
        }

        /**
         * user: 添加的用户信息
         * info: 附加信息
         * 在匹配池中添加一个名用户
         * 
         * @param user
         * @param info
         */
        int32_t add_user(const User& user, const std::string& info) {
            // Your implementation goes here
            printf("add_user\n");

            unique_lock<mutex> lck(message_queue.m); // 加锁 执行完函数后自动解锁
            message_queue.q.push({user, "add"});
            message_queue.cv.notify_all(); //唤醒被条件变量阻塞的线程

            return 0;
        }

        /**
         * user: 删除的用户信息
         * info: 附加信息
         * 从匹配池中删除一名用户
         * 
         * @param user
         * @param info
         */
        int32_t remove_user(const User& user, const std::string& info) {
            // Your implementation goes here
            printf("remove_user\n");

            unique_lock<mutex> lck(message_queue.m);
            message_queue.q.push({user, "remove"});
            message_queue.cv.notify_all();

            return 0;
        }

};


void consume_task() {
    while(true)
    {
        unique_lock<mutex> lck(message_queue.m);
        if(message_queue.q.empty())
        {
            //队列中没有任务 等待其他线程操作(唤醒)
            message_queue.cv.wait(lck);
        }
        else
        {
            auto task = message_queue.q.front();
            message_queue.q.pop();

            lck.unlock(); //已操作消息队列， 显式解锁。提高效率

            //do task
            if (task.type == "add") pool.add(task.user);  //把user加入匹配池
            else if (task.type == "remove") pool.remove(task.user); //把user移出匹配池

            pool.match(); //有user时，匹配
        }
    }
}

int main(int argc, char **argv) {
    int port = 9090;
    ::std::shared_ptr<MatchHandler> handler(new MatchHandler());
    ::std::shared_ptr<TProcessor> processor(new MatchProcessor(handler));
    ::std::shared_ptr<TServerTransport> serverTransport(new TServerSocket(port));
    ::std::shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
    ::std::shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());

    TSimpleServer server(processor, serverTransport, transportFactory, protocolFactory);

    cout << "Start Match Server" << endl;

    thread matching_thread(consume_task); //匹配的线程

    server.serve();
    return 0;
}



