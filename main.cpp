#include <fstream>
#include <string>
#include <stdio.h>
#include <map>
#include <vector>
#include <customnetworkmanager.h>
#include <iostream>
#include <boost/regex.hpp>
#include <stdexcept>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
using namespace std;


#define SCALE_INT 5
#define REQUEST_IN_TIME 20

void help() {
    cout << "Usage: myWget [option]... [url]\n -c convert links (use with -P) \n -r recursive load\n -L recurive depth (use with -r)\n -P path to destination directory\n-d allow load frome all domains\n-s load sourse file\n";
}

void errorMessage() {
    cout << "Incorrect enter. \n Try to enter \"-help\" to get some help." << endl;
    exit(1);
}




class Url {
public:
    bool absolute = false , correct = false;
    string domain, uri, port = "80", tail, format;
    shared_ptr<string> shptr;



    void refresh(string s) {
        if (shptr.get() != nullptr) {
            (*shptr) = s;
        }
    }

    string getUri() {
        return uri;
    }

    string getUrl() {
        return domain + uri + tail;
    }

    bool isAbsolute() {
        return absolute;
    }
};


void getDestDir(string& s, int counter) {
    int pos = s.length() - 1;
    while (pos >= 0 && s[pos] != '/') {
        --pos;
    }

    for (int i = 0; i < counter; ++i) {
        --pos;
        while (pos >= 0 && s[pos] != '/' ) {
            --pos;
        }
    }

    s = s.substr(0,pos + 1);
    if (s == "") {
        s = "/";
    }



}

Url getUrlByLink(Url& pageUrl, string rawUrl) {

    string s1 = R"((?:(?:(\w{0,6}):)?//)([\w\.-]+)(:\d+)?(.*))"; // check and cut absolute addres
    string s2 = R"(([^#\?]+)(\?[^#]*)?(?:#.*)?)"; // check uri
    boost::regex regExp(s1);

    string pageUri = pageUrl.uri , linkUri;


    boost::smatch result;
    Url linkUrl;


    linkUrl.correct = true;
    if (boost::regex_match(rawUrl,result,regExp)) {

        if (result[1].matched && result[1] != "http") {
            linkUrl.correct = false;
        } else {

            linkUrl.absolute = true;
            linkUrl.domain = result[2];

            if (result[3].matched) {
                linkUrl.port = result[3];
                linkUrl.port = linkUrl.port.substr(1);
            }

            linkUri  = result[4];
        }

    } else {

        linkUrl.absolute = false;
        linkUrl.domain = pageUrl.domain;
        linkUri = rawUrl;
    }

    boost::regex subRegExp(s2);
    boost::smatch subResult;

    if (linkUrl.correct && boost::regex_match(linkUri,subResult,subRegExp)) {


        if (subResult[2].matched) {
            linkUrl.tail = subResult[2];
        }

        linkUri = subResult[1];

        if ( !linkUrl.absolute && linkUri[0] != '/' ) {

            int counter = 0;
            while (linkUri.length() > 2 && linkUri.substr(0,2) == "./") {
                ++counter;
                linkUri = linkUri.substr(2);
            }
            getDestDir(pageUri, counter);
            linkUri =  pageUri +  linkUri;
        }

        linkUrl.uri = linkUri;

        int pos = linkUri.length();
        while (pos >=0 && linkUri[pos] != '/') {
            if (linkUri[pos] == '.') {
                linkUrl.format = linkUri.substr(pos + 1);
                break;
            }
            --pos;
        }

    } else {

        linkUrl.correct = false;

    }


    return linkUrl;


}



class HtmlFrames {
public:

    std::vector<std::shared_ptr<string>> parts;
    HtmlFrames(){}
    HtmlFrames( const string& target) {
        std::vector<int> partsNum;
        partsNum.push_back(0);
        boost::regex regExp("<(([aA])|([lL][iI][nN][kK])|([sS][cC][rR][iI][pP][tT])|(?:[iI][mM][gG])).*?((href)|(src))=[\"\']([^\"\']+)[\"\'].*?>");
        boost::smatch match;
        boost::sregex_iterator iter(target.begin(),target.end(),regExp),invalideIter;
        while (iter != invalideIter) {
            match = *iter;
            ++iter;
            if (!match[0].matched ) {
                continue;
            }
            int beg = match.position(8);
            partsNum.push_back(beg);
            partsNum.push_back(beg + match[8].length());
        }
        partsNum.push_back(target.length());
        for (int i = 1; i < partsNum.size(); ++i) {
            parts.push_back(std::make_shared<string>(target.substr(partsNum[i -1], partsNum[i] - partsNum[i - 1])));
        }
    }

    std::vector<pair<Url, shared_ptr<string>>> getUrlList(Url& parent) {
        std::vector<pair<Url, shared_ptr<string>>> answer;
        for (int i = 1; i < parts.size(); i+=2) {
            answer.push_back({getUrlByLink(parent, *parts[i]), parts[i]});
        }
        return answer;
    }

    string build() {
        string s;
        for (auto iter = parts.begin(); iter != parts.end(); ++iter) {
            s.append(**iter);
        }
        return s;
    }
};

struct task {
    string prefix, rawData;
    Url url;
    int deep = 1, redirect = 10;
    bool recursive , convert, raw, success, typeAll, domainAll;
    HtmlFrames data;

    task():prefix("./"),  recursive(false), convert(false), raw(true), success(false), typeAll(true), domainAll(false) {}

    bool forLoad(Url& url) {
        return (domainAll||url.domain == this->url.domain||this->url.domain == url.domain)&&(typeAll|| url.format == "php" || url.format == "html");
    }


    bool isTerminate() {
        return !recursive||deep == 0;
    }

    void write() {
        string la = prefix + "/" + url.domain + url.uri;

        int n = 0;
        while (n < la.length() && (n = la.find('/',n)) != string::npos) {
            string tmp = la.substr(0,n);
            mkdir(tmp.c_str(),0777) ;
            n++;
        }
        if (la[la.length() - 1] == '/') {
            la += "index.html";
        }
        la += url.tail;
        //cout << la << endl;

        ofstream fout(la);

        if (fout.is_open()) {
            if (raw) {
                fout << rawData;
            } else {
                fout << data.build();
            }
            fout.close();
        }
    }


};

map<string, task> loaded;

void finalwrite() {
    int counter = 0;
    for (auto iter = loaded.begin(); iter != loaded.end(); ++iter) {
        if (iter->second.success) {
            if (!iter->second.raw && iter->second.convert) {
                try {
                    auto buf  = iter->second.data.getUrlList(iter->second.url);
                    for (auto it = buf.begin(); it != buf.end(); ++it) {
                        string s = it->first.getUrl();
                        if (loaded.find(s)!= loaded.end()) {
                            (*it->second) = iter->second.prefix + '/' + s;
                        }
                    }
                } catch (...) {
                    //ignore
                }
            }
            iter->second.write();
            ++counter;
        }
    }
    cout << "Total: " << counter << endl;
}

void my_handler(int sig,siginfo_t *siginfo,void *context) {
    finalwrite();
    exit(0);
}

int main(int argc,char* args[]) {


    task initTask;
    vector<task> newTasks;
    struct sigaction sa;
    memset(&sa,0,sizeof(sa));
    sa.sa_sigaction = &my_handler;
    sigset_t ss;
    sigemptyset(&ss);
    sigaddset(&ss,SIGQUIT);
    sa.sa_mask = ss;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGQUIT,&sa,NULL);

    try {
        int i = 1;
        for ( ; i < argc && args[i][0] == '-'; ++i) {

            switch (args[i][1]) {
            case '-':
                if (string(args[i] + 2) == "help") {
                    help();
                    return 0;
                }
                break;
            case 'c':
                initTask.convert = true;
                break;
            case 'L':
                if (i + 1 < argc) {
                    initTask.deep = stoi(args[++i]);
                } else {
                    errorMessage();
                }
                break;
            case 'r':
                initTask.recursive = true;
                break;
            case 'P':
                if (i + 1 < argc) {
                    initTask.prefix = string(args[++i]);
                } else {
                    errorMessage();
                }
                break;
            case 'd':
                initTask.domainAll = true;
                break;
            case 's':
                initTask.typeAll = true;
                break;
            default:
                cout << "Unknown flag " << args[i] << endl;
                return 1;
            }
        }


        if (argc - i <= 0) {
            errorMessage();
        } else {
            Url empty;
            for (; i < argc; ++i) {
                initTask.url = getUrlByLink(empty,args[i]);
                if (initTask.url.uri == "") {
                    initTask.url.uri = "/";
                }
                newTasks.push_back(initTask);
                loaded[initTask.url.getUrl()] = initTask;
            }
        }




    } catch(...) {
        errorMessage();
    }



    try {
        CustomNetworkManager manager;


        int counter = 0;
        while (newTasks.size() != 0 || counter > 0) {
            int min = (SCALE_INT < newTasks.size())?SCALE_INT : newTasks.size();
            if (counter < REQUEST_IN_TIME) {
                counter += min;
                for (int j = 0; j < min; ++j) {
                    try {
                        auto i = newTasks.back();
                        newTasks.pop_back();
                        CustomHttpRequest request(i.url.getUrl(), i.url.port);

                        if (!manager.get(request)) {
                            //some issues
                            --counter;
                            std::cout << "Cannot make request to " << i.url.getUrl() << std::endl;
                            continue;
                        }
                        std::cout << "Loading "<< i.url.getUrl() << "..." <<  std::endl;
                    } catch(...) {
                        counter--;
                    }
                }
            }

            auto ans = manager.makeRequests();

            if (ans.size() == 0) {
                counter = 0;
            }

            counter-=ans.size();

            for (auto iter = ans.begin(); iter != ans.end(); ++iter) {
                try {

                    int code = iter->second.getStatusCode();

                    if (code==200 || code == 203) {
                        //succses

                        string s = iter->first.getUrl();
                        auto ptr = loaded.find(s);
                        std::cout << "Page " << iter->first.getUrl() << " was downloaded" <<  std::endl;
                        if (ptr->second.url.uri.back() == '/' || ptr->second.url.format == "html" || ptr->second.url.format == "php") {

                            HtmlFrames frame(iter->second.getMessageBody());

                            if (!ptr->second.isTerminate()) {
                                auto buf = frame.getUrlList(ptr->second.url);
                                for (auto i = buf.begin(); i != buf.end(); ++i) {
                                    if (i->first.correct && ptr->second.forLoad(i->first) && loaded.find(i->first.getUrl()) == loaded.end()) {
                                        task tmp = ptr->second;
                                        tmp.deep--;
                                        tmp.url = i->first;
                                        loaded[i->first.getUrl()] = tmp;
                                        try {
                                            newTasks.push_back(tmp);
                                        } catch (...) {
                                            loaded.erase(i->first.getUrl());
                                        }
                                    }
                                }
                            }
                            ptr->second.data = frame;
                            ptr->second.raw = false;

                        } else {
                            ptr->second.raw = true;
                            ptr->second.rawData = iter->second.getMessageBody();
                        }
                        ptr->second.success = true;
                    } else if (code / 100 == 3) {
                        //redirect
                        auto it = loaded.find(iter->first.getUrl());
                        if (it->second.redirect > 0) {
                            string s = iter->second.getHeaderValue("Location");
                            Url url = getUrlByLink(it->second.url, s);
                            if (it->second.domainAll ||  it->second.url.domain == url.domain) {
                                task redirectTask(it->second);
                                redirectTask.url  = url;
                                string sourseUrl = it->second.url.getUrl();
                                loaded.erase(it);
                                --redirectTask.redirect;
                                if (loaded.find(url.getUrl()) == loaded.end()) {
                                    loaded[url.getUrl()] = redirectTask;
                                    try {
                                        newTasks.push_back(redirectTask);
                                    } catch (...) {
                                        loaded.erase(url.getUrl());
                                    }

                                    std::cout << " Redirect from " << sourseUrl << " to " << url.getUrl() << std::endl;

                                }

                            }
                        }


                    } else {
                        string explain = "Disconected";
                        if (iter->second.getStartingLine() != "") {
                            explain = iter->second.getStartingLine().substr(iter->second.getStartingLine().find(" "));
                        }
                        std::cout << "Get reply with status code :" << explain << " from " << iter->first.getUrl() << std::endl;
                    }

                } catch (...) {

                }
            }

        }

        finalwrite();

    } catch (runtime_error& e) {
        cout << "Program execution stoped. Error ocurred." << endl;
    }
}


