#include "customtcpserver.h"
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <string.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdexcept>

#include <iostream>

CustomTcpServer::CustomTcpServer():interror(0),ebufSize(0) {
    if ((efd = epoll_create(10)) < 0) {
        throw std::runtime_error("CustomTcpServer error");
    }
}

string CustomTcpServer::errorString() {
    return string(strerror(interror));
}

TcpSocket* CustomTcpServer::listen(string& port) {
    addrinfo *__addr,hints,*curr;
    int tfd;
    memset(&hints,0,sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if (getaddrinfo(NULL,port.c_str(),&hints,&__addr) < 0) {
        interror = errno;
        return nullptr;
    }
    for (curr = __addr; curr != nullptr; curr = curr->ai_next) {
        if ((tfd = socket(curr->ai_family,curr->ai_socktype,curr->ai_socktype)) < 0) {
            continue;
        }
        if (bind(tfd,curr->ai_addr,curr->ai_addrlen) < 0 || fcntl(tfd,F_SETFL,O_NONBLOCK) < 0 ){
            ::close(tfd);
            continue;
        }
        if (::listen(tfd,10) < 0) {
            ::close(tfd);
            continue;
        }

        break;
    }
    freeaddrinfo(__addr);
    if (curr == nullptr) {
        interror = errno;
        return nullptr;
    }
    TcpSocket* ptr;
    try {
        ptr = new TcpSocket(port,port,this);
    } catch(...) {
        close(tfd);
        throw;
    }

    ptr->_listen = true;
    ptr->_closed = false;
    ptr->fd = tfd;
    socketList.insert(ptr);
    return ptr;
}


const std::vector<TcpSocket*>& CustomTcpServer::roR() {
    return __roR;
}
const std::vector<TcpSocket*>& CustomTcpServer::roW() {
    return __roW;
}
const std::vector<TcpSocket*>& CustomTcpServer::roU() {
    return __roU;
}

const std::vector<TcpSocket*>& CustomTcpServer::accepted() {
    return __acpt;
}

const std::vector<TcpSocket*>& CustomTcpServer::failed() {
    return __failed;
}



bool CustomTcpServer::epollChange(TcpSocket* tcpSock, int param) {
    if (tcpSock->interFlags == param) {
        return true;
    }
    bool flag = true;
    int flag1, inc = 0;
    if (tcpSock->interFlags == 0) {
        flag1 = EPOLL_CTL_ADD;
        inc = 1;
    } else if (param == 0) {
        flag1 = EPOLL_CTL_DEL;
        inc = -1;
    } else {
        flag1 = EPOLL_CTL_MOD;
    }
    epoll_event ev;
    memset(&ev,0,sizeof(ev));
    ev.events = param;
    ev.data.ptr = tcpSock;
    if (epoll_ctl(efd,flag1,tcpSock->fd,&ev) < 0){
        flag = false;
        interror = errno;
    } else {
        tcpSock->interFlags = param;
        ebufSize += inc;
    }

    return flag;
}

void CustomTcpServer::fail(TcpSocket* tcpSocket) {
    epollChange(tcpSocket,0);
    __failed.push_back(tcpSocket);

}

TcpSocket& CustomTcpServer::socketFactory(string& addres, string& port) {
    TcpSocket* ptr = new TcpSocket(addres,port,this);
    socketList.insert(ptr);
    return *ptr;
}

void CustomTcpServer::deleteSocket(TcpSocket* tcpSocket) {
    if (tcpSocket->host != this) {
        return;
    }
    if (socketList.erase(tcpSocket) > 0) {
        epollChange(tcpSocket,0);
        if (tcpSocket->fd != -1) {
            close(tcpSocket->fd);
        }
        delete tcpSocket;
    }
}

bool CustomTcpServer::addSocket(TcpSocket& tcpSock) {

    if (tcpSock.isClosed() && !tcpSock.isListen()) {
        return false;
    } else if (tcpSock.isListen()) {
        return epollChange(&tcpSock, EPOLLIN);
    }
    tcpSock.flags = tcpSock.userFlags;
    return epollChange(&tcpSock,tcpSock.flags);
}



bool CustomTcpServer::connect(TcpSocket& tcpSock,string soursePort) {
    if (!tcpSock.isClosed() || (tcpSock.interFlags&EPOLLOUT) != 0) {
        return true;
    }
    string destAddr = tcpSock.addres, destPort = tcpSock.port;
    addrinfo *__addr,hints,*curr,*__saddr,*scurr;
    memset(&hints,0,sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(destAddr.c_str(),destPort.c_str(),&hints,&__addr) < 0) {
        interror = errno;
        return false;
    }
    if (soursePort != "") {
        hints.ai_flags = AI_PASSIVE;
        if (getaddrinfo(NULL,soursePort.c_str(),&hints,&__saddr) < 0) {

        }
    }
    int tfd;
    for (curr = __addr,scurr = __saddr; curr!= nullptr; curr = curr->ai_next, scurr = (soursePort == "")?nullptr:scurr->ai_next) {
        if ((tfd = socket(curr->ai_family,curr->ai_socktype,curr->ai_protocol)) <0) {
            continue;
        }
        if (soursePort != "") {
            if (bind(tfd,scurr->ai_addr,scurr->ai_addrlen) < 0) {
                ::close(tfd);
                continue;
            }
        }
        if (fcntl(tfd,F_SETFL,O_NONBLOCK) < 0) {
            ::close(tfd);
            continue;
        }
        if (( ::connect(tfd,curr->ai_addr,curr->ai_addrlen) < 0 ) && (errno != EINPROGRESS)) {
            ::close(tfd);
            continue;
        }
        break;
    }
    freeaddrinfo(__addr);
    if (soursePort != "") {
        freeaddrinfo(__saddr);
    }
    if (curr == nullptr) {
        interror = errno;
        return false;
    }

    tcpSock.fd = tfd;
    return epollChange(&tcpSock,EPOLLOUT);
}



void CustomTcpServer::read(TcpSocket* ptr) {
    int sz;
    char buf[1024];
    for(;;) {

        if ((sz = ::recv(ptr->fd,buf,1024,MSG_DONTWAIT)) <= 0) {
            if (sz == 0) {
                //printLog("server closed the connection on the socket " + ptr->fd);
                ptr->_closed = true;
                epollChange(ptr,ptr->interFlags&(~EPOLLIN));
                break;
            }
            else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else {
                fail(ptr);
                return;
            }
        }
        ptr->data.append(string(buf,sz));
    }


    __roR.push_back(ptr);
}

void CustomTcpServer::write(TcpSocket* ptr) {
    int sz, total = ptr->shift;
    while (ptr->dataToSend.length() > total) {
        if((sz = ::send(ptr->fd,ptr->dataToSend.c_str() + total,ptr->dataToSend.length() - total,MSG_DONTWAIT)) <=0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || sz == 0) {
                ptr->shift = total;
                return;
            }
            fail(ptr);
        }
        total += sz;
    }
    epollChange(ptr,ptr->interFlags&(~EPOLLOUT));
    __roW.push_back(ptr);
}

bool CustomTcpServer::wait(int msec) {
    epoll_event ebuf[ebufSize];
    __roR.clear();
    __roW.clear();
    __roU.clear();
    for (auto iter = __acpt.begin(); iter != __acpt.end(); ++iter) {
        (*iter)->_acpt.clear();
    }
    __acpt.clear();
    __failed.clear();
    int tmp;
    if ((tmp = epoll_wait(efd,ebuf,ebufSize,msec)) < 0) {
        if (errno == EINTR) {
            return wait(msec);
        }
        return false;
    }

    for (int i = 0; i < tmp; ++i) {
        epoll_event curr_ev(ebuf[i]);
        TcpSocket* curr_ptr = (TcpSocket*)curr_ev.data.ptr;
        if (curr_ev.events & EPOLLIN) {
            if (curr_ptr->isListen()) {
                bool flag = false;
                string tmp = "";
                while (curr_ptr->toAccept != 0) {
                    int tfd;
                    if ((tfd = ::accept(curr_ptr->fd,NULL,NULL)) < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        }
                        flag = true;
                        break;

                    }
                    TcpSocket* ptr;
                    try {
                        ptr = new TcpSocket(tmp,tmp,this);
                    } catch(...) {
                        close(tfd);
                        throw;
                    }
                    --(curr_ptr->toAccept);
                    ptr->fd = tfd;
                    ptr->_closed = false;
                    socketList.insert(ptr);
                    curr_ptr->_acpt.push_back(ptr);
                }
                if (flag) {
                    __failed.push_back(curr_ptr);
                }
                __acpt.push_back(curr_ptr);

                if (curr_ptr->toAccept == 0) {
                    epollChange(curr_ptr, 0);
                }

            } else {
                read(curr_ptr);
            }
        } else if (curr_ev.events & EPOLLOUT){
            if (!curr_ptr->isClosed()){
                write(curr_ptr);
            } else {
                epollChange(curr_ptr,0);
                curr_ptr->_closed = false;
                __roU.push_back(curr_ptr);
            }
        } else {
            fail(curr_ptr);
        }
    }
    return true;
}

CustomTcpServer::~CustomTcpServer() {
    close(efd);
    for (auto iter = socketList.begin(); iter !=socketList.end(); ++iter) {
        close((**iter).fd);
    }
    socketList.clear();
    //std::cout << "i'm done" << std::endl;
}




