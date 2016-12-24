#include "customnetworkmanager.h"
#include <stdio.h>
#include <iostream>
CustomNetworkManager::CustomNetworkManager(){}

CustomNetworkManager::~CustomNetworkManager(){}

bool CustomNetworkManager::get(CustomHttpRequest& request) {
    return addRequest(request,"GET");
}

bool CustomNetworkManager::post(CustomHttpRequest& request) {
    return addRequest(request,"POST");
}

bool CustomNetworkManager::addRequest(CustomHttpRequest& request, const std::__cxx11::string type) {
    if (request.addr.empty()) {
        return false;
    }
    request.type = type;
    string port = string(PORT_HTTP);
    TcpSocket* sock = &serv.socketFactory(request.addr, port);
    if (sock == nullptr) {
        errorMessage = serv.errorString();
        return false;
    }
    if (!serv.connect(*sock)) {
        serv.deleteSocket(sock);
        return false;
    }
    requests[sock] = std::pair<CustomHttpRequest,CustomHttpReply>(request,CustomHttpReply());
    return true;
}

string CustomNetworkManager::getError() const {
    return errorMessage;
}

std::vector<std::pair<CustomHttpRequest,CustomHttpReply>> CustomNetworkManager::makeRequests() {
    replys.clear();
    while(replys.size() == 0) {
        if (!serv.wait(TIMEOUT)) {
            errorMessage = serv.errorString();
            return replys;
        }

        auto res = serv.roU();
        for (auto i = res.begin(); i != res.end(); ++i) {
            string data = requests[*i].first.getRequest();
            (*i)->setData(data);
            (*i)->addWrite();
            serv.addSocket(**i);
        }       

        res = serv.roW();
        for (auto i = res.begin(); i != res.end(); ++i) {
            (*i)->removeWrite();
            (*i)->addRead();
            serv.addSocket(**i);
        }

        res = serv.roR();
        for (auto i = res.begin(); i!= res.end(); ++i) {
            auto tmp = &requests[*i];
            if (tmp->second.parse(*i)||(*i)->isClosed()) {
                replys.push_back(*tmp);
                requests.erase(*i);
                serv.deleteSocket(*i);
            } else {
                serv.addSocket(**i);
            }

        }

        res = serv.failed();
        for (auto i = res.begin(); i != res.end(); ++i) {
            //std::cerr << "failed to load " << requests[*i].first.uri << ". Keep trying\n";
            TcpSocket *ptr = *i;
            auto tmp = requests[ptr];
            serv.deleteSocket(ptr);
            if (!addRequest(tmp.first,tmp.first.type)) {
                replys.push_back(tmp);
            }
            requests.erase(*i);

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

CustomHttpRequest::CustomHttpRequest(const std::__cxx11::string url) {
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

    if ((it = headers.find("Transfer-Encoding")) != headers.end() && it->second == "chunked") {
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
    } else if ((it = headers.find("Content-Length")) != headers.end()) {
        if (sizeToLoad == 0) {
            sizeToLoad = std::stoi(headers["Content-Length"]);
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



