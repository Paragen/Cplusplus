#ifndef CUSTOMTCPSERVER_H
#define CUSTOMTCPSERVER_H

#include <vector>
#include <set>
#include <memory>
#include <string>
#include <sys/epoll.h>
#include "tcpsocket.h"
#define CTS_BUF_SIZE 4096
#define CTS_READ EPOLLIN
#define CTS_WRITE EPOLLOUT
using std::string;



class CustomTcpServer
{
    friend class TcpSocket;
    std::vector<TcpSocket*> __roR;
    std::vector<TcpSocket*> __roW;
    std::vector<TcpSocket*> __roU;
    std::vector<TcpSocket*> __acpt;
    std::vector<TcpSocket*> __failed;

    std::set<TcpSocket*> socketList;

    int interror;
    int efd;
    int ebufSize;

    bool epollChange(TcpSocket* ptr,int flag);
    void read(TcpSocket* tcpSocket);
    void write(TcpSocket* tcpSocket);
    void fail(TcpSocket* tcpSocket);
public:




    CustomTcpServer();
    ~CustomTcpServer();

    string errorString();

    TcpSocket* listen(string& port);

    bool connect(TcpSocket& tcpSocket,string soursePort = "");

    const std::vector<TcpSocket*>& roR();
    const std::vector<TcpSocket*>& roW();
    const std::vector<TcpSocket*>& roU();
    const std::vector<TcpSocket*>& accepted();
    const std::vector<TcpSocket*>& failed();

    bool addSocket(TcpSocket& tcpSocket);

    bool wait(int msec =-1);

    TcpSocket& socketFactory(string& addres, string& port);
    void deleteSocket(TcpSocket* ptr);

};

#endif // CUSTOMTCPSERVER_H
