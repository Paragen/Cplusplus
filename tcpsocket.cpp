#include "tcpsocket.h"



TcpSocket::TcpSocket(string& addres, string& port, CustomTcpServer* host):
    addres(addres),port(port),fd(-1),userFlags(0),flags(0), interFlags(0) ,
    shift(0), toAccept(0) ,host(host),_closed(true),_listen(false) {}

void TcpSocket::setData(string& data) {
        shift = 0;
        this->dataToSend =data;
}

string& TcpSocket::getData() {
    return data;
}

bool TcpSocket::operator==(const TcpSocket& another) {
    return fd==-1 && fd == another.fd;
}

void TcpSocket::addRead() {
    userFlags = userFlags|EPOLLIN;
}

void TcpSocket::addWrite() {
    userFlags = userFlags|EPOLLOUT;
}

void TcpSocket::removeRead() {
    userFlags = userFlags&(~EPOLLIN);
}

void TcpSocket::removeWrite() {
    userFlags = userFlags&(~EPOLLOUT);
}

bool TcpSocket::isClosed() {
    return _closed;
}

bool TcpSocket::isListen() {
    return _listen;
}

void TcpSocket::accept(int count) {
    toAccept = count;
}


