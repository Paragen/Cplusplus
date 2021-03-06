#ifndef CUSTOMNETWORKMANAGER_H
#define CUSTOMNETWORKMANAGER_H

#include <string>
#include <vector>
#include <list>
#include <map>
#include <customtcpserver.h>
#include <memory>




using std::list;
using std::map;
using std::vector;
using std::string;


class CustomNetworkManager;


class CustomHttpBase {

    friend class CustomNetworkManager;

protected:
    map<string,string> headers;
    string messageBody;
    CustomHttpBase();
public:

    void setHeader(const string& header, const string& headerValue);
    string getHeaderValue(const string& header) const;
    vector<std::pair<string,string>> headerList() const;
    bool hasHeader(const string& header) const;
    void setMessageBody(const string& s);
    string getMessageBody() const;

};


class CustomHttpRequest:public CustomHttpBase {
    friend class CustomNetworkManager;
    string addr,type,uri, port = "80";
    int timeOfSilence = 0;

    string getRequest();
public:
    CustomHttpRequest();
    CustomHttpRequest(const string url, string port = "80");
    void setUrl(const string url);
    string getUrl() const;
    string getAddr() const;
};


class CustomHttpReply:public CustomHttpBase {
    friend class CustomNetworkManager;

    unsigned  statusCode;
    int sizeToLoad;
    string startingLine;
    bool prepareToBody;

    bool parse(TcpSocket* ptr);

public:
    CustomHttpReply();
    unsigned int getStatusCode() const;
    string getStartingLine() const;
    string getData() const;


};



class CustomNetworkManager
{

    string errorMessage;
    map<TcpSocket*,std::pair<CustomHttpRequest, CustomHttpReply>> requests;
    std::vector<std::pair<CustomHttpRequest,CustomHttpReply>> replys;
    //std::map<string, std::list<TcpSocket*>> openToUse;
    CustomTcpServer serv;
    bool addRequest(CustomHttpRequest& request,const string type);
   // TcpSocket* getOpenToUse(string& addr);
    void removeRequest(TcpSocket* sock);

public:
    CustomNetworkManager();
    ~CustomNetworkManager();
    bool get(CustomHttpRequest& request);
    //void head(CustomHttpRequest& request);
    //void put(CustomHttpRequest& request);
    bool post(CustomHttpRequest& request);
    std::vector<std::pair<CustomHttpRequest,CustomHttpReply>> makeRequests();

    string getError() const;
};

#endif // CUSTOMNETWORKMANAGER_H
