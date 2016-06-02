//
//  UrlRequest.hpp
//  urldTest
//
//  Created by John on 12.12.15.
//  Copyright © 2015 Outlaw Studio. All rights reserved.
//

#pragma once

#include <string>
#include <vector>
#include <utility>
#include <tuple>
#include <array>
#include <sstream>
#include <netdb.h>      //  gethostbyname,
#include <arpa/inet.h>
#include <iostream>
#include <fcntl.h>
#include "Response.hpp"
#include "JsonValueAdapter.hpp"

using std::cout;
using std::endl;

class UrlRequest{
public:
    struct HostIsNullException{};
    struct HostEntry{
        std::string host;
    };
    struct timeval timeout={30,0};
    struct GetParameter{
        
        template<class T>
        GetParameter(const std::string &name,T t):value(std::move(StringValueAdapter<T>{name,t}())){}
        
        const std::string value;
        
    protected:
        template<class T>
        struct StringValueAdapter{
            const std::string name;
            const T value;
            
            std::string operator()()const{
                std::stringstream ss;
                ss<<this->name<<"="<<this->value;
                return std::move(ss.str());
            }
        };
        
        template<class T>
        struct StringValueAdapter<std::vector<T>>{
            const std::string name;
            const std::vector<T> value;
            
            std::string operator()()const{
                std::stringstream ss;
                for(auto i=0;i<this->value.size();++i){
                    const auto &obj=this->value[i];
                    ss<<this->name<<"[]="<<obj;
                    if(i<this->value.size()-1){
                        ss<<"&";
                    }
                }
                return std::move(ss.str());
            }
        };
    };
protected:
    std::string _host;
    std::string _uri="/";
    short _port=80;
    std::string _method="GET";
    std::string _body;
    std::vector<std::string> _headers={"Connection: close"};
    
    static const std::string& crlf(){
        static std::string res="\r\n";
        return res;
    }
    
    struct MemoryBuffer{
        char *buffer=nullptr;
        int size=0;
        
        MemoryBuffer(decltype(buffer) buffer_,decltype(size) size_):
        buffer(buffer_),
        size(size_){}
        
        MemoryBuffer(const MemoryBuffer&)=delete;
        MemoryBuffer& operator=(const MemoryBuffer&)=delete;
        
        MemoryBuffer(MemoryBuffer &&other){
            this->swap(other);
        }
        
        ~MemoryBuffer(){
            if(this->buffer){
                delete [] this->buffer;
                this->buffer=nullptr;
            }
        }
        
        void swap(MemoryBuffer &other){
            std::swap(this->buffer, other.buffer);
            std::swap(this->size, other.size);
        }
        
        char* begin(){
            return this->buffer;
        }
        
        const char* begin() const{
            return this->buffer;
        }
        
        char* end(){
            return this->buffer+size;
        }
        
        const char* end() const{
            return this->buffer+size;
        }
    };
    typedef std::vector<MemoryBuffer> MemoryBuffers;
    
    static int connectTimeout(int s,sockaddr *address,int addressSize,struct timeval *tv){
        ::connect(s,address,addressSize);
        fd_set fdset;
        FD_ZERO(&fdset);
        FD_SET(s, &fdset);
        return ::select(s + 1, nullptr, &fdset, nullptr, tv);
    }
    
    static int recvtimeout(int s, char *buf, int len, struct timeval *tv){
        fd_set fds;
        int n;
//        struct timeval tv;
        
        // set up the file descriptor set
        FD_ZERO(&fds);
        FD_SET(s, &fds);
        
        // set up the struct timeval for the timeout
//        tv.tv_sec = timeout;
//        tv.tv_usec = 0;
        
        // wait until timeout or data received
        n = ::select(s+1, &fds, NULL, NULL, tv);
        if (n == 0) return -2; // timeout!
        if (n == -1) return -1; // error
        
        // data must be here, so do a normal recv()
        return int(::recv(s, buf, len, 0));
    }
public:
    UrlRequest(decltype(_method) method="GET"):_method(method){};
    
    template<class Host,class Uri>
    UrlRequest(Host host,Uri uri);
    
    template<class Host>
    UrlRequest& host(Host host);
    
    template<class Uri>
    UrlRequest& uri(Uri uri);
    
    template<class Uri>
    UrlRequest& uri(Uri uri,std::vector<GetParameter> getParameters){
        std::stringstream ss;
        ss<<uri;
        const auto getParametersCount=getParameters.size();
        if(getParametersCount){
            ss<<"?";
            for(auto i=0;i<getParametersCount;++i){
                auto &getParameter=getParameters[i];
//                ss<<getParameter.first<<"="<<getParameter.second.value;
                ss<<getParameter.value;
                if(i<getParametersCount-1){
                    ss<<"&";
                }
            }
        }
        _uri=std::move(ss.str());
        return *this;
    }

    void port(decltype(_port)value){
        _port=value;
    }
    
    template<class Method>
    UrlRequest& method(Method method){
        _method=std::move(method);
        return *this;
    }
    
    UrlRequest& body(std::vector<std::pair<std::string,JsonValueAdapter>> jsonArguments){
        _body=JsonValueAdapter(std::move(jsonArguments)).toString();
        return *this;
    }
    
    template<class Header>
    UrlRequest& addHeader(Header header){
        _headers.emplace_back(std::move(header));
        return *this;
    }
    
    Response perform() throw(HostIsNullException,Response::IncorrectStartLineException){
        auto fd=::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
        struct hostent *host;
        host = ::gethostbyname(_host.c_str());
        if(host){
            ::fcntl(fd, F_SETFL, O_NONBLOCK);
            sockaddr_in sockAddr;
            sockAddr.sin_port=htons(_port);
            sockAddr.sin_family=AF_INET;
            sockAddr.sin_addr.s_addr = decltype(sockAddr.sin_addr.s_addr)(*((unsigned long*)host->h_addr));
            auto connectionTimeoutHappened=false;
            if(connectTimeout(fd, (sockaddr*)(&sockAddr), sizeof(sockAddr), &this->timeout)==1){
                int so_error;
                socklen_t len = sizeof so_error;
                ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &len);
                if (so_error == 0) {
//                    std::cout<<"connected"<<std::endl;
                    }else{
                        std::cerr<<"error = "<<so_error<<std::endl;
                        connectionTimeoutHappened=true;
                    }
                    }else{
                        connectionTimeoutHappened=true;
                    }
                    
                    MemoryBuffers buffers;
                    if(!connectionTimeoutHappened){
                        auto requestString=_method+" "+_uri+" HTTP/1.1"+crlf()+"Host: "+_host;
                        for(const auto &header:_headers){
                            requestString+=crlf()+header;
                        }
                        if(_body.length()){
                            std::stringstream ss;
                            ss<<_body.length();
                            const auto bodyLengthString=std::move(ss.str());
                            ss.flush();
                            requestString+=crlf()+"Content-Length: "+bodyLengthString;
                        }
                        requestString+=crlf()+crlf();
                        auto bytesToWrite=requestString.length();
                        auto bytesWrote=::send(fd,requestString.c_str(), bytesToWrite,0);
                        if(bytesWrote==bytesToWrite){
                            if(_body.length()){
                                bytesToWrite=_body.length();
                                bytesWrote=::send(fd, _body.c_str(), _body.length(), 0);
                                if(bytesWrote!=bytesToWrite){
                                    cout<<"wrote not whole body"<<endl;
                                }
                            }
                            char buffer[10000];
                            do{
                                auto bytesReceived=recvtimeout(fd, buffer, 10000, &this->timeout);
                                if(bytesReceived==0){
                                    break;
                                }else if(bytesReceived==-2){
                                    return std::move(Response(408, "Request Timeout","{\"message\":\"Request Timeout\",\"status_code\":408}"));
                                }else if(bytesReceived>0){
                                    auto newBuffer=new char[bytesReceived];
                                    ::memcpy((void*)newBuffer, (const void*)buffer, size_t(bytesReceived));
                                    buffers.emplace_back((char*)newBuffer,int(bytesReceived));
                                }
                            }while(true);
                        }else{
                            cout<<"wrote not whole request"<<endl;
                        }
                    }
                    ::close(fd);
                    
                    if(!connectionTimeoutHappened){
                        char prevByte=-1;
                        decltype(buffers)::iterator bufferIt=buffers.begin();
                        decltype(decltype(buffers)::value_type::buffer) charIt=bufferIt->begin();
                        auto iterateResponseLambda=[&bufferIt,&charIt,&buffers,&prevByte](std::function<void(char,bool&)> f){
                            auto stop=false;
                            for(;bufferIt!=buffers.end();++bufferIt){
                                //                cout<<"buffer index = "<<bufferIt-buffers.begin()<<endl;
                                for(;charIt!=bufferIt->end();++charIt){
                                    auto c=*charIt;
                                    f(c,stop);
                                    if(stop){
                                        return;
                                    }
                                }
                                charIt=bufferIt->begin();
                            }
                        };
                        
                        //  read start line..
                        std::string startLine;
                        iterateResponseLambda([&prevByte,&startLine](char c,bool &stop){
                            startLine+=c;
                            if(c==10){
                                if(prevByte==13){
                                    startLine=startLine.substr(0,startLine.length()-2);
                                    prevByte=c;
                                    stop=true;
                                }
                            }
                            prevByte=c;
                        });
                        ++charIt;
                        
                        //  read headers..
                        std::string line;
                        std::vector<std::string> headers;
                        do{
                            iterateResponseLambda([&prevByte,&line](char c,bool &stop){
                                line+=c;
                                if(c==10){
                                    if(prevByte==13){
                                        line=line.substr(0,line.length()-2);
                                        prevByte=c;
                                        stop=true;
                                    }
                                }
                                prevByte=c;
                            });
                            ++charIt;
                            if(line.length()){
                                headers.emplace_back(std::move(line));
                                line.clear();
                            }else{
                                break;
                            }
                        }while(true);
                        
                        //  read body..
                        std::string body;
                        if(std::find(headers.begin(), headers.end(), "Transfer-Encoding: chunked")==headers.end()){
                            std::stringstream bodyStream;
                            for(;bufferIt!=buffers.end();){
                                bodyStream.write(&*charIt, bufferIt->end()-charIt);
                                ++bufferIt;
                                charIt=bufferIt->begin();
                            }
                            body=std::move(bodyStream.str());
                            bodyStream.flush();
                        }else{
                            auto hexToInt=[](const std::string &s)->int{
                                std::stringstream ss;
                                ss<<std::hex<<s;
                                int res;
                                ss>>res;
                                return res;
                            };
                            std::stringstream bodyStream;
                            do{
                                line.clear();
                                iterateResponseLambda([&prevByte,&line](char c,bool &stop){
                                    line+=c;
                                    if(c==10){
                                        if(prevByte==13){
                                            line=line.substr(0,line.length()-2);
                                            prevByte=c;
                                            stop=true;
                                        }
                                    }
                                    prevByte=c;
                                });
                                int chunkSize=hexToInt(line);
                                if(!chunkSize){
                                    break;
                                }
                                ++charIt;
                                
                                for(;bufferIt!=buffers.end();){
                                    size_t bytesToRead=bufferIt->end()-charIt;
                                    if(bytesToRead>chunkSize){
                                        bytesToRead=chunkSize;
                                    }
                                    bodyStream.write(&*charIt, bytesToRead);
                                    charIt+=bytesToRead;
                                    if(charIt==bufferIt->end()){
                                        ++bufferIt;
                                        charIt=bufferIt->begin();
                                    }
                                    chunkSize-=bytesToRead;
                                    if(!chunkSize){
                                        break;
                                    }
                                }
                                
                                //  skip crlf..
                                iterateResponseLambda([&prevByte,&line](char c,bool &stop){
                                    if(c==10){
                                        if(prevByte==13){
                                            stop=true;
                                        }
                                    }
                                    prevByte=c;
                                });
                                ++charIt;
                                if(charIt==bufferIt->end()){
                                    ++bufferIt;
                                    charIt=bufferIt->begin();
                                }
                            }while(true);
                            body=std::move(bodyStream.str());
                            bodyStream.flush();
                            std::string chuckSuffix=crlf()+'0'+crlf()+crlf();
                            if(body.length()>=chuckSuffix.length()){
                                const std::string bodySuffix=std::move(body.substr(body.length()-chuckSuffix.length()));
                                if(bodySuffix==chuckSuffix){
                                    body=std::move(body.substr(0,body.length()-chuckSuffix.length()));
                                }
                            }
                        }
                        return std::move(Response(startLine, std::move(headers), std::move(body)));
                    }else{
                        return std::move(Response(408, "Request Timeout","{\"message\":\"Request Timeout\",\"status_code\":408}"));
                    }
                    }else{
                        throw HostIsNullException{};
                    }
                    }
    
    UrlRequest& operator+(const HostEntry &hostEntry){
        this->host(hostEntry.host);
        return *this;
    }
};

inline UrlRequest::HostEntry operator "" _host(const char *s, size_t l){
    return UrlRequest::HostEntry{std::string(s,l)};
}

#pragma mark - Implementation

//template<class Method>
//UrlRequest& UrlRequest::method(Method method)

template<class Uri>
UrlRequest& UrlRequest::uri(Uri uri){
    _uri=std::move(uri);
    return *this;
}

//template<class Uri>
//UrlRequest& UrlRequest::uri
                    
template<class Host>
UrlRequest& UrlRequest::host(Host host){
    _host=std::move(host);
    return *this;
}

template<class Host,class Uri>
UrlRequest::UrlRequest(Host host, Uri uri):
_host(host),
_uri(uri)
{
    //..
}
