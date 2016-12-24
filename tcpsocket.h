#include "customtcpserver.h"
#ifndef TCPSOCKET_H
#define TCPSOCKET_H
using std::string;
class CustomTcpServer;

class TcpSocket
{
    friend class CustomTcpServer;
    string addres, port, data, dataToSend;
    int fd,userFlags, flags, interFlags, shift , toAccept;
    CustomTcpServer* host;
    bool _closed, _listen;

    std::vector<TcpSocket*> _acpt;

    TcpSocket(string& addres, string& port, CustomTcpServer* host);
    TcpSocket(TcpSocket& another) ;//= delete;
public:
    void setData(string& addres);
    string& getData();

    void addRead();
    void addWrite();
    void removeRead();
    void removeWrite();
    void accept(int count);

    bool isClosed();
    bool isListen();
    bool operator==(const TcpSocket& another);
    void close();

};

#endif // TCPSOCKET_H
