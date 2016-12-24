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
    cout << "Usage: myWget [option]... [url]\n -c convert links\n -r recursive load\n -L recurive depth (use with -r)\n -P path to destination directory\n-d allow load frome all domains\n-s load sourse file\n";
}

void errorMessage() {
    cout << "Incorrect enter. \n Try to enter \"-help\" to get some help." << endl;
}




class Url {
public:
    bool absolute;
    string domain, uri, port;
    shared_ptr<string> shptr;

    Url(){}
    Url(string& url, bool outFile = true) {
        setUrl(url, outFile);
    }
    Url(shared_ptr<string> ptr, bool outFile = true ) {
        shptr = ptr;
        setUrl(*ptr, outFile);
    }

    void refresh(string s) {
        if (shptr.get() != nullptr) {
            (*shptr) = s;
        }
        setUrl(s);
    }

    void  setUrl(string& url, bool outFile = true) {
        boost::regex regExp("(?:((?:(?:https?|ftp)://)|(?://))?([\\w\\.-]+)(:[\\d]+)?)?((?:/[\\w/_\\.-]*)(?:[\\?][^\\s#]+)?)?(#[\\w_]*)?");
        boost::smatch result;
        uri = "/";
        if (boost::regex_match(url,result,regExp)) {
            if (result[1].matched) {
                absolute = true;
            } else {
                absolute = false;
            }


            if (result[2].matched) {
                domain = result[2];
            }

            if (result[3].matched) {
                port = result[3];
            }

            if (result[4].matched) {
                uri = result[4];
            }

            if (!absolute && !outFile) {
                if (uri == "/") {
                    uri = "";
                }
                uri = "/" + domain + uri;
                domain = "";
            }

        }
    }

    string getUri() {
        return uri;
    }

    string getUrl() {
        return domain + uri;
    }

    bool isAbsolute() {
        return absolute;
    }
};

class HtmlFrames {
public:

    std::vector<std::shared_ptr<string>> parts;
    HtmlFrames(){}
    HtmlFrames( const string& target) {
        std::vector<int> partsNum;
        partsNum.push_back(0);
        boost::regex regExp("<(([aA])|([lL][iI][nN][kK])|([sS][cC][rR][iI][pP][tT])).*?((href)|(src))=[\"\']([^\"\']+)[\"\'].*?>");
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

    std::vector<Url> getUrlList() {
        std::vector<Url> answer;
        for (int i = 1; i < parts.size(); i+=2) {
            answer.push_back(Url(parts[i],false));
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
    string uri, domain, prefix, port, rawData;
    int deep, redirect;
    bool recursive, convert, raw, success, typeAll, domainAll;
    HtmlFrames data;

    task():uri("/"),prefix("./"), redirect(1), deep(10), recursive(false), convert(false), raw(true), success(false), typeAll(true), domainAll(false) {}

    bool load(Url& url) {
         return (domainAll||url.domain == ""||domain ==url.domain)&&(typeAll||url.uri[url.uri.length() - 1]== '/' || url.uri.find(".html") != string::npos);
    }



    task getChildTask(Url url) {
        task ans(*this);
        ans.uri = url.getUri();
        if (url.domain != "") {
            ans.domain = url.domain;
        }
        ans.deep--;

        return ans;
    }

    bool isTerminate() {
        return !recursive||deep == 0;
    }

    void write() {
        string la = prefix + "/" + domain + uri;

        int l,n = 0;
        while (n < la.length() && (n = la.find('/',n)) != string::npos) {
            string tmp = la.substr(0,l);
            if (opendir(tmp.c_str()) == NULL) {
            mkdir(la.substr(0,l).c_str(),ACCESSPERMS) ;
            }
            l = n++;
        }
       // string sbuf("mkdir -p " + la.substr(0,l));
       //system(sbuf.c_str());
        if (la[la.length() - 1] == '/') {
            la+="index.html";
        }

           // cout << strerror(errno) << la.substr(0,l) << endl;
        cout << la << endl;
        ofstream fout(la.c_str(),ios_base::out);
        if (raw) {
            fout << rawData;
        } else {
            fout << data.build();
        }
        fout.close();
    }


};

map<string, task> loaded;

void finalwrite() {
for (auto iter = loaded.begin(); iter != loaded.end(); ++iter) {
    if (iter->second.success) {
        if (!iter->second.raw && iter->second.convert) {
            auto buf  = iter->second.data.getUrlList();
            for (auto it = buf.begin(); it != buf.end(); ++it) {
                string s = it->getUrl();
                if (!it->isAbsolute()) {
                    s = iter->second.domain + s;
                }
                if (loaded.find(s)!= loaded.end()) {
                    it->refresh(string(iter->second.prefix + '/' + s));
                }
            }
        }
        iter->second.write();
    }
}
}
void my_handler(int sig,siginfo_t *siginfo,void *context) {
    finalwrite();
    exit(0);
}

int main(int argc,char* args[]) {

    task initTask;
    vector<task> newTasks;
    Url initUrl;
    struct sigaction sa;
    memset(&sa,0,sizeof(sa));
        sa.sa_sigaction = &my_handler;
        sigset_t ss;
        sigemptyset(&ss);
        sigaddset(&ss,SIGQUIT);
        sa.sa_mask = ss;
        sa.sa_flags = SA_SIGINFO;
        sigaction(SIGQUIT,&sa,NULL);


    for (int i = 1; i < argc; ++i) {
        switch (*args[i]) {
        case '-':
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
                sscanf(args[++i],"%d",&(initTask.deep));
                break;
            case 'r':
                initTask.recursive = true;
                break;
            case 'P':
                initTask.prefix = string(args[++i]);
                break;
            case 'd':
                initTask.domainAll = true;
                break;
            case 's':
                initTask.typeAll = true;
                break;
            default:
                errorMessage();
            }
            break;
        default:
            if (argc > i + 1) {
                errorMessage();
                return 1;
            } else {
                initUrl.refresh(string(args[i]));
            }
        }
    }

    initTask.domain = initUrl.domain;
    initTask.uri = initUrl.uri;

    newTasks.push_back(initTask);
    loaded[initUrl.getUrl()] = newTasks.front();

    try {
        CustomNetworkManager manager;


    int counter = 0;
    while (newTasks.size() != 0 || counter > 0) {
        int min = (SCALE_INT < newTasks.size())?SCALE_INT : newTasks.size();
        if (counter < REQUEST_IN_TIME) {
            counter += min;
            for (int j = 0; j < min; ++j) {
                auto i = newTasks.back();
                newTasks.pop_back();
                string s = i.domain + i.uri;
                CustomHttpRequest request(s);
                if (!manager.get(request)) {
                    //some issues
                    --counter;
                    std::cout << "Cannot make request to' " << s << std::endl;
                    continue;
                }
                std::cout << "Loading "<< s << "..." <<  std::endl;
            }
        }

        auto ans = manager.makeRequests();

        if (ans.size() == 0) {
            std::cout << manager.getError() << std::endl;//temporaly
            if (newTasks.size() == 0) {
                break;
            }
        }

        counter-=ans.size();

        for (auto iter = ans.begin(); iter != ans.end(); ++iter) {

            int code = iter->second.getStatusCode();

            if (code==200 || code == 203) {
                //succses

                string s = iter->first.getUrl();
                auto ptr = loaded.find(s);
                std::cout << "Page " << iter->first.getUrl() << " was downloaded" <<  std::endl;
                if (iter->second.getHeaderValue("Content-Type").find("html")!= string::npos) {

                    HtmlFrames frame(iter->second.getMessageBody());

                    if (!ptr->second.isTerminate()) {
                        auto buf = frame.getUrlList();
                        for (auto i = buf.begin(); i != buf.end(); ++i) {
                            //if ((!i->isAbsolute()||i->domain == ptr->second.domain) && loaded.find(ptr->second.domain + i->getUri()) == loaded.end()) {
                            auto tmp = ptr->second.getChildTask(*i);
                            if (ptr->second.load(*i) && loaded.find(tmp.domain + tmp.uri) == loaded.end()) {
                                loaded[tmp.domain + tmp.uri] = tmp;
                                newTasks.push_back(tmp);
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
                    Url url(s);
                     if (it->second.domainAll || url.domain == "" || it->second.domain == url.domain) {
                    task redirectTask(it->second);
                    string sourseUri = it->second.domain + it->second.uri;
                    loaded.erase(it);
                    --redirectTask.redirect;
                    redirectTask.uri = url.getUri();
                    if (url.domain != "") {
                        redirectTask.domain = url.domain;
                    }
                    loaded[redirectTask.domain + redirectTask.uri] = redirectTask;
                    std::cout << " Redirect from " << sourseUri << " to" << redirectTask.domain + redirectTask.uri << std::endl;
                    newTasks.push_back(redirectTask);

                    }
                }


            } else {
                string explain = "Unknown";
                if (iter->second.getStartingLine() != "") {
                    explain = iter->second.getStartingLine().substr(iter->second.getStartingLine().find(" "));
                }
                std::cout << "Get reply with status code :" << explain << " from " << iter->first.getUrl() << std::endl;
            }

        }

    }

    finalwrite();
    cout << "Total: " << loaded.size() << endl;
    } catch (runtime_error& e) {
        cout << "Program execution stoped. Error ocurred." << endl;
    }
}


