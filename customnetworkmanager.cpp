#include "customnetworkmanager.h"
#include <stdio.h>
#include <iostream>

const int TIMEOUT = 1000 , LIMIT = 5;
CustomNetworkManager::CustomNetworkManager(){

}

CustomNetworkManager::~CustomNetworkManager(){}

bool CustomNetworkManager::get(CustomHttpRequest& request) {
    return addRequest(request,"GET");
}

bool CustomNetworkManager::post(CustomHttpRequest& request) {
    return addRequest(request,"POST");
}

//TcpSocket* CustomNetworkManager::getOpenToUse(string& addr) {
//    auto iter = openToUse.find(addr);
//    if (iter != openToUse.end()) {
//        auto it = iter->second.begin();
//        while (it != iter->second.end()) {
//            if ((*it)->isClosed()) {
//                serv.deleteSocket(*it);
//                it = iter->second.erase(it);
//            } else {
//                TcpSocket* buf = *it;
//                iter->second.erase(it);
//                return buf;
//            }
//        }
//    }
//    return nullptr;
//}

bool CustomNetworkManager::addRequest(CustomHttpRequest& request, const std::__cxx11::string type) {
    if (request.addr.empty()) {
        return false;
    }
    request.type = type;

    TcpSocket*   sock = &serv.socketFactory(request.addr, request.port);


    if (sock == nullptr) {
        errorMessage = serv.errorString();
        return false;
    }

    if (!serv.connect(*sock)) {
        serv.deleteSocket(sock);
        return false;
    }

    try {
        requests[sock] = std::pair<CustomHttpRequest,CustomHttpReply>(request,CustomHttpReply());
    } catch (...) {
        serv.deleteSocket(sock);
        throw;
    }

    return true;
}

string CustomNetworkManager::getError() const {
    return errorMessage;
}

void CustomNetworkManager::removeRequest(TcpSocket *sock) {
    requests.erase(sock);
    serv.deleteSocket(sock);

}

std::vector<std::pair<CustomHttpRequest,CustomHttpReply>> CustomNetworkManager::makeRequests() {
    replys.clear();
    while(replys.size() == 0 && requests.size() > 0) {
        if (!serv.wait(TIMEOUT)) {
            errorMessage = serv.errorString();
            return replys;
        }

        std::vector<TcpSocket*> res;

        int totalCounter = 0;
        try {
            res = serv.roU();
        } catch(...) {
            //ignore
        }

        totalCounter += res.size();
        for (auto i = res.begin(); i != res.end(); ++i) {
            string data = requests[*i].first.getRequest();
            (*i)->setData(data);
            (*i)->addWrite();
            if (!serv.addSocket(**i)) {
                removeRequest(*i);
            }
        }
        try {
            res = serv.roW();
        } catch(...) {
            //ignore
        }

        totalCounter += res.size();
        for (auto i = res.begin(); i != res.end(); ++i) {
            (*i)->removeWrite();
            (*i)->addRead();
            if (!serv.addSocket(**i)) {
                removeRequest(*i);
            }
        }

        try {
            res = serv.roR();
        } catch(...)  {
            //ignore
        }

        totalCounter += res.size();
        for (auto i = res.begin(); i!= res.end(); ++i) {
            try {
                auto tmp = requests.find(*i);
                tmp->second.first.timeOfSilence = 0;
                if (tmp->second.second.parse(*i)||(*i)->isClosed()) {
                    replys.push_back(tmp->second);
                    requests.erase(*i);

                    serv.deleteSocket(*i);

                } else {
                    if (!serv.addSocket(**i)) {
                        removeRequest(*i);
                    }
                }
            } catch (...) {
                removeRequest(*i);
            }

        }

        try {
            res = serv.failed();
        } catch (...) {
            //ignore
        }

        totalCounter += res.size();
        for (auto i = res.begin(); i != res.end(); ++i) {
            //std::cerr << "failed to load " << requests[*i].first.uri << ". Keep trying\n";

            TcpSocket *ptr = *i;
            auto tmp = requests.find(ptr);
            serv.deleteSocket(ptr);
            try {
                if (!addRequest(tmp->second.first,tmp->second.first.type)) {
                    replys.push_back(tmp->second);
                }
            } catch (...) {
                //ignore
            }

            requests.erase(*i);

        }
        if (totalCounter == 0) {
            auto it = requests.begin();
            while (it != requests.end()) {
                it->second.first.timeOfSilence++;
                if (it->second.first.timeOfSilence == LIMIT) {
                    serv.deleteSocket(it->first);
                    it = requests.erase(it);
                } else {
                    ++it;
                }
            }
        }

    }
    return replys;
}





CustomHttpBase::CustomHttpBase() {}



void CustomHttpBase::setHeader(const std::__cxx11::string& header, const std::__cxx11::string& headerValue) {
    headers[header] = headerValue;
}

string CustomHttpBase::getHeaderValue(const string& header) const {
    auto it = headers.find(header);
    if (it == headers.end()) {
        return "";
    } else {
        return it->second;
    }
}

vector<std::pair<string,string>> CustomHttpBase::headerList() const {
    vector<std::pair<string,string>> vec;
    for (auto it = headers.begin(); it != headers.end(); ++it) {
        vec.push_back(std::pair<string,string>(it->first,it->second));
    }
    return vec;
}

bool CustomHttpBase::hasHeader(const string& header) const {
    return headers.find(header) != headers.end();
}

void CustomHttpBase::setMessageBody(const string& s) {
    messageBody = s;
}

string CustomHttpBase::getMessageBody() const {
    return messageBody;
}

CustomHttpRequest::CustomHttpRequest(){}

string CustomHttpRequest::getRequest() {
    string res;

    res.append(type + " " + uri + " HTTP/1.1\r\n" );
    for (auto it = headers.begin(); it != headers.end(); ++it) {
        res.append(it->first +": " + it->second + "\r\n");
    }
    res += "\r\n";
    res.append(messageBody);
    return res;
}

CustomHttpRequest::CustomHttpRequest(const std::__cxx11::string url, string port) {
    this->port = port;
    setUrl(url);
}

void CustomHttpRequest::setUrl(const std::__cxx11::string url) {
    auto pos = url.find('/');

    if (pos == string::npos) {
        uri = "/";
        addr = url;
    } else {
        uri = url.substr(pos);
        addr = url.substr(0,pos);
    }

    headers["Host"] = addr;
    //headers["Connection"] = "Keep-Alive";
}

string CustomHttpRequest::getUrl() const {
    return addr + uri;
}

string CustomHttpRequest::getAddr() const {
    return addr;
}

CustomHttpReply::CustomHttpReply():statusCode(0),sizeToLoad(0),prepareToBody(true){}

unsigned CustomHttpReply::getStatusCode() const {
    return statusCode;
}

string CustomHttpReply::getStartingLine() const {
    return startingLine;
}


bool CustomHttpReply::parse(TcpSocket* ptr) {

    string s = ptr->getData();

    if (startingLine == "") {
        int num = s.find("\r\n");
        if (num == string::npos) {
            ptr->getData() = s;
            return false;
        }
        startingLine = s.substr(0,num);
        s = s.substr(num + 2);
        std::sscanf(startingLine.c_str(),"%*s %d ",&statusCode);
    }


    int num;
    while (prepareToBody && (num = s.find("\r\n")) != 0) {
        if (num == string::npos) {
            ptr->getData() = s;
            return false;
        }
        string line = s.substr(0,num);
        s = s.substr(num + 2);
        num = line.find(' ');
        headers[line.substr(0,num - 1)] = line.substr(num + 1);
    }

    if (prepareToBody) {
        prepareToBody = false;
        s = s.substr(2);
    }
    std::map<string,string>::iterator it;

    if (((it = headers.find("Transfer-Encoding")) != headers.end()||(it = headers.find("transfer-encoding")) != headers.end()) && it->second == "chunked") {
        while(true) {
            if (sizeToLoad == 0) {
                num = s.find("\r\n");
                if (num == string::npos) {
                    ptr->getData() = s;
                    return false;
                }
                string line = s.substr(0,num);
                ptr->getData() = s = s.substr(num + 2);
                sizeToLoad = std::stoi(line,0,16);
                if (sizeToLoad == 0) {
                    return true;
                }
            }

            if (s.length() >= sizeToLoad + 2) {
                messageBody += s.substr(0,sizeToLoad);
                ptr->getData() = s = s.substr(sizeToLoad + 2);
                sizeToLoad = 0;
            } else {
                return false;
            }

        }
    } else if ((it = headers.find("Content-Length")) != headers.end()||(it = headers.find("content-length")) != headers.end()) {
        if (sizeToLoad == 0) {
            sizeToLoad = std::stoi(it->second);
        }
        messageBody += s;
        ptr->getData() = "";

        if (messageBody.length() >= sizeToLoad) {
            return true;
        } else {
            return false;
        }
    } else {
        return true;
    }



}



