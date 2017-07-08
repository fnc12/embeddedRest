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

#ifdef _WIN32

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPCRECATED
#include <winsock2.h>

#pragma comment(lib, "Ws2_32.lib")

#else

#include <netdb.h>      //  gethostbyname,
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <unistd.h>

#endif

#include <iostream>
#include <fcntl.h>
#include <functional>
#include <fstream>
#include "Response.hpp"
#include "JsonValueAdapter.hpp"

using std::cout;
using std::endl;

#ifdef _WIN32

int gettimeofday(struct timeval * tp, struct timezone * tzp)
{
    // Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
    static const uint64_t EPOCH = ((uint64_t)116444736000000000ULL);
    
    SYSTEMTIME  system_time;
    FILETIME    file_time;
    uint64_t    time;
    
    GetSystemTime(&system_time);
    SystemTimeToFileTime(&system_time, &file_time);
    time = ((uint64_t)file_time.dwLowDateTime);
    time += ((uint64_t)file_time.dwHighDateTime) << 32;
    
    tp->tv_sec = (long)((time - EPOCH) / 10000000L);
    tp->tv_usec = (long)(system_time.wMilliseconds * 1000);
    return 0;
}

/*void SetNonBlocking(int filehandle)
 {
	int fhFlags;
 
	fhFlags = fcntl(filehandle, F_GETFL);
	if (fhFlags < 0)
	{
 perror("fcntl(F_GETFL)");
 exit(1);
	}
 
	fhFlags |= O_NONBLOCK;
	if (fcntl(filehandle, F_SETFL, fhFlags) < 0)
	{
 perror("fcntl(F_SETFL)");
 exit(1);
	}
 
	return;
 }*/
#endif

class UrlRequest{
public:
    struct HostIsNullException{};
    struct HostEntry{
        std::string host;
    };
    struct timeval timeout;
    struct GetParameter{
        
        template<class T>
        GetParameter(const std::string &name,T t):value(std::move(StringValueAdapter<T>{name,t}())){}
        
        std::string value;
        
        /*GetParameter& operator=(GetParameter &&other){
         std::swap(this->value, other.value);
         }*/
        
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
    struct MultipartAdapter{
        
        std::string body(){
            return std::move(this->stream.str());
        }
        
        const std::string &boundary() const{
            return _boundary;
        }
        
        void addFormField(const std::string &name,const std::string &value){
            this->stream<<"--"<<_boundary<<crlf();
            this->stream<<"Content-Disposition: form-data; name=\""<<name<<"\""<<crlf();
            this->stream<<"Content-Type: text/plain; charset="<<this->charset<<crlf();
            this->stream<<crlf();
            this->stream<<value<<crlf();
        }
        
        void addFilePart(const std::string &fieldName,
                         const std::string &filepath,
                         const std::string &fileName,
                         const std::string &mimeType)
        {
            auto count=fileSize(filepath);
            std::ifstream file(filepath);
            if(file){
                this->stream<<"--"<<_boundary<<crlf();
                this->stream<<"Content-Disposition: form-data; name=\""<<fieldName<<"\"; filename=\""<<fileName<<"\""<<crlf();
                this->stream<<"Content-Type: "<<mimeType<<crlf();
                this->stream<<"Content-Transfer-Encoding: binary"<<crlf();
                this->stream<<crlf();
                stream_copy_n(file, count, this->stream);
                file.close();
                this->stream<<crlf();
            }else{
                std::cerr<<"failed to open file at *"<<filepath<<"*"<<std::endl;
            }
        }
        
    protected:
        friend class UrlRequest;
        
        MultipartAdapter():charset("UTF-8"){}
        
        std::stringstream stream;
        std::string charset;
        std::string _boundary=std::move(generateBoundary());
        
        static std::string generateBoundary(){
            struct timeval tv;
            ::gettimeofday(&tv, nullptr);
            std::stringstream ss;
            ss<<"==="<<tv.tv_sec<<"_"<<tv.tv_usec<<"===";
            return std::move(ss.str());
        }
        
        void finish(){
            this->stream<<crlf();
            this->stream<<"--"<<_boundary<<"--"<<crlf();
        }
        
        size_t fileSize(const std::string &filepath){
            std::ifstream file( filepath, std::ios::binary | std::ios::ate);
            if(file){
                return file.tellg();
            }else{
                return 0;
            }
        }
        
        void stream_copy_n(std::istream & in, std::size_t count, std::ostream & out){
            const std::size_t buffer_size = 4096;
            char buffer[buffer_size];
            while(count > buffer_size)
            {
                in.read(buffer, buffer_size);
                out.write(buffer, buffer_size);
                count -= buffer_size;
            }
            
            in.read(buffer, count);
            out.write(buffer, count);
        }
    };
protected:
    std::string _host;
    std::string _uri="/";
    short _port=80;
    std::string _method="GET";
    std::string _body;
    std::vector<std::string> _headers;
    
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
    
    static int sendInLoop(int s,const char *buf,size_t len){
        auto bytesToWrite=len;
        size_t bytesWrote=0;
        while(bytesWrote != bytesToWrite){
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(s, &fds);
            
            auto n = ::select(s+1, nullptr, &fds, nullptr, nullptr);
            switch(n){
                case 0:return -2;
                default:return -1;
                case 1:
#ifdef _WIN32
                    typedef const char *SendPointer_t;
#else
                    typedef const void *SendPointer_t;
#endif
                    auto bytesWroteThisTime = ::send(s, (SendPointer_t)(buf + bytesWrote), bytesToWrite, 0);
                    //                    cout<<"bytesWroteThisTime = "<<bytesWroteThisTime<<endl;
                    if(bytesWroteThisTime > 0){
                        bytesWrote += bytesWroteThisTime;
                        bytesToWrite -= bytesWroteThisTime;
                        if(0==bytesToWrite){
                            return 0;
                        }
                    }else{
                        return -1;
                    }
                    break;
            }
        }
        return 0;
        /*bytesWrote=::send(fd, _body.c_str()+bytesWrote, bytesToWrite, 0);
         while(bytesWrote!=bytesToWrite){
         //                                    cout<<"wrote not whole body"<<endl;
         bytesToWrite -= bytesWrote;
         cout<<"bytesWrote "<<bytesWrote<<" instead of "<<bytesToWrite<<endl;
         bytesWrote=::send(fd, _body.c_str()+bytesWrote, bytesToWrite, 0);
         }*/
    }
    
    static int recvtimeout(int s, char *buf, int len, struct timeval *tv, bool *receivedAll){
        (void)receivedAll;
        fd_set fds;
        int n;
        
        // set up the file descriptor set
        FD_ZERO(&fds);
        FD_SET(s, &fds);
        
        // wait until timeout or data received
        n = ::select(s+1, &fds, nullptr, nullptr, tv);
        if (n == 0) return -2; // timeout!
        if (n == -1) return -1; // error
        
        /*int bytesToRead;
         ::ioctl(s,FIONREAD,&bytesToRead);
         cout<<"bytesToRead = "<<bytesToRead<<endl;*/
        
        // data must be here, so do a normal recv()
        auto res=int(::recv(s, buf, len, 0));
        /*if(receivedAll){
         *receivedAll = bytesToRead==res;
         }*/
        return res;
    }
public:
    UrlRequest(decltype(_method) method = "GET") :_method(method) {
        this->timeout.tv_sec = 30;
        this->timeout.tv_usec = 0;
        _headers.emplace_back("Connection: close");
    };
    
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
        const size_t getParametersCount=getParameters.size();
        if(getParametersCount){
            ss<<"?";
            for(size_t i=0;i<getParametersCount;++i){
                auto &getParameter=getParameters[i];
                ss<<getParameter.value;
                if(i<getParametersCount-1){
                    ss<<"&";
                }
            }
        }
        _uri=std::move(ss.str());
        return *this;
    }
    
    /**
     *  This is for settings body like "key=value&key2=value2&...".
     */
    /*UrlRequest& bodyKeyValue(std::vector<GetParameter> getParametersInBody){
     std::stringstream ss;
     const auto getParametersCount=getParametersInBody.size();
     for(auto i=0;i<getParametersCount;++i){
     auto &getParameter=getParametersInBody[i];
     ss<<getParameter.value;
     if(i<getParametersCount-1){
     ss<<"&";
     }
     }
     _body=std::move(ss.str());
     return *this;
     }*/
    
    UrlRequest& url(const std::string &value){
        std::string prefix="://";
        auto prefixPos=value.find(prefix);
        auto prefixEndPos=prefixPos+prefix.length();
        auto urlWithoutProtocol=value.substr(prefixEndPos,value.length()-prefixEndPos);
        auto firstSlashPos=urlWithoutProtocol.find('/');
        this->host(urlWithoutProtocol.substr(0,firstSlashPos));
        this->uri(urlWithoutProtocol.substr(firstSlashPos,urlWithoutProtocol.length()-firstSlashPos));
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
    
    UrlRequest& bodyJson(JsonValueAdapter::Object_t jsonArguments){
        _body=JsonValueAdapter(std::move(jsonArguments)).toString();
        return *this;
    }
    
    UrlRequest& bodyMultipart(std::function<void(MultipartAdapter&)> f){
        MultipartAdapter multipartAdapter;
        f(multipartAdapter);
        multipartAdapter.finish();
        _body=std::move(multipartAdapter.body());
        _headers.push_back("Content-Type: multipart/form-data; boundary="+multipartAdapter.boundary());
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
#ifdef _WIN32
            {
                unsigned long on = 1;
                ::ioctlsocket(fd, FIONBIO, &on);
            }
#else
            ::fcntl(fd, F_SETFL, O_NONBLOCK);
#endif
            sockaddr_in sockAddr;
            sockAddr.sin_port=htons(_port);
            sockAddr.sin_family=AF_INET;
            sockAddr.sin_addr.s_addr = decltype(sockAddr.sin_addr.s_addr)(*((unsigned long*)host->h_addr));
            auto connectionTimeoutHappened=false;
            if(connectTimeout(fd, (sockaddr*)(&sockAddr), sizeof(sockAddr), &this->timeout)==1){
                int so_error;
#ifdef _WIN32
                typedef int socklen_t;
                typedef char *SockOpt_t;
#else
                typedef void *SockOpt_t;
#endif
                socklen_t len = sizeof so_error;
                ::getsockopt(fd, SOL_SOCKET, SO_ERROR, (SockOpt_t)&so_error, &len);
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
                ssize_t bytesToWrite= (ssize_t)requestString.length();
                ssize_t bytesWrote=::send(fd,requestString.c_str(), bytesToWrite,0);
                if(bytesWrote==bytesToWrite){
                    if(_body.length()){
                        sendInLoop(fd, _body.c_str(), int(_body.length()));
                    }
                    char buffer[10000];
                    do{
                        bool receivedAll=false;
                        auto bytesReceived=recvtimeout(fd, buffer, 10000, &this->timeout, &receivedAll);
//                        cout<<"bytesReceived = "<<bytesReceived<<", receivedAll = "<<receivedAll<<endl;
                        if(bytesReceived==0){
                            break;
                        }else if(bytesReceived==-2){
                            return std::move(Response(408, std::string("Request Timeout"),std::string("{\"message\":\"Request Timeout\",\"status_code\":408}")));
                        }else if(bytesReceived>0){
                            auto newBuffer=new char[bytesReceived];
                            ::memcpy((void*)newBuffer, (const void*)buffer, size_t(bytesReceived));
                            buffers.emplace_back((char*)newBuffer,int(bytesReceived));
                            /*if(receivedAll){
                             break;
                             }*/
                        }
                    }while(true);
                }else{
                    std::cerr<<"wrote not whole request"<<std::endl;
                }
            }
#ifdef _WIN32
            ::closesocket(fd);
#else
            close(fd);
#endif
            
            if(!connectionTimeoutHappened){
                char prevByte=-1;
                auto bufferIt=buffers.begin();
                auto charIt=bufferIt->begin();
                auto iterateResponseLambda=[&bufferIt,&charIt,&buffers,&prevByte](std::function<void(char,bool&)> f){
                    auto stop=false;
                    while (!stop) {
                        // "while" instead of "if" in case of empty buffer
                        while (charIt == bufferIt->end()) {
                            if (bufferIt == buffers.end()) {
                                // FIXME: shouldn't happen -- throw exception?
                                return;
                            }
                            ++bufferIt;
                        charIt=bufferIt->begin();
                    }
                        // cout<<"buffer index = "<<bufferIt-buffers.begin()<<endl;

                        // at this point, charIt points to a valid char.
                        // so we can pass it into the user function.
                        // We increment charIt since the user function
                        // consumed the character.

                        auto c = *charIt;
                        ++charIt;
                        f(c, stop);
                    }
                };
                
                //  read start line..
                std::string startLine;
                prevByte = -1;
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
                
                //  read headers..
                std::string line;
                std::vector<std::string> headers;
                do{
                    prevByte = -1;
                    iterateResponseLambda([&prevByte, &line](char c, bool &stop){
                        line += c;
                        if (c == 10){
                            if (prevByte == 13){
                                line = line.substr(0, line.length() - 2);
                                prevByte = c;
                                stop = true;
                            }
                        }
                        prevByte = c;
                    });
                    if (line.length()){
                        headers.emplace_back(std::move(line));
                        line.clear();
                    }
                    else{
                        break;
                    }
                } while (true);
                
                //  read body..
                std::string body;
                if (std::find(headers.begin(), headers.end(), "Transfer-Encoding: chunked") == headers.end()){
                    std::stringstream bodyStream;
                    for (; bufferIt != buffers.end();){
                        bodyStream.write(&*charIt, bufferIt->end() - charIt);
                        ++bufferIt;
                        if (bufferIt == buffers.end())break;
                        charIt = bufferIt->begin();
                    }
                    body = std::move(bodyStream.str());
                    bodyStream.flush();
                }
                else{
                    auto hexToInt = [](const std::string &s)->int{
                        std::stringstream ss;
                        ss << std::hex << s;
                        int res;
                        ss >> res;
                        return res;
                    };
                    std::stringstream bodyStream;
                    do{
                        line.clear();
                        prevByte = -1;
                        iterateResponseLambda([&prevByte, &line](char c, bool &stop){
                            line += c;
                            if (c == 10){
                                if (prevByte == 13){
                                    line = line.substr(0, line.length() - 2);
                                    prevByte = c;
                                    stop = true;
                                }
                            }
                            prevByte = c;
                        });
                        int chunkSize = hexToInt(line);
                        if (!chunkSize){
                            break;
                        }
                        
                        for (; bufferIt != buffers.end();){
                            auto bytesToRead = bufferIt->end() - charIt;
                            if (bytesToRead>chunkSize){
                                bytesToRead = chunkSize;
                            }
                            bodyStream.write(&*charIt, bytesToRead);
                            charIt += bytesToRead;
                            if (charIt == bufferIt->end()){
                                ++bufferIt;
                                charIt = bufferIt->begin();
                            }
                            chunkSize -= bytesToRead;
                            if (!chunkSize){
                                break;
                            }
                        }
                        
                        //  skip crlf..
                        prevByte = -1;
                        iterateResponseLambda([&prevByte, &line](char c, bool &stop){
                            if (c == 10){
                                if (prevByte == 13){
                                    stop = true;
                                }
                            }
                            prevByte = c;
                        });
                        if (charIt == bufferIt->end()){
                            ++bufferIt;
                            charIt = bufferIt->begin();
                        }
                    } while (true);
                    body = std::move(bodyStream.str());
                    bodyStream.flush();
                    std::string chuckSuffix = crlf() + '0' + crlf() + crlf();
                    if (body.length() >= chuckSuffix.length()){
                        const std::string bodySuffix = std::move(body.substr(body.length() - chuckSuffix.length()));
                        if (bodySuffix == chuckSuffix){
                            body = std::move(body.substr(0, body.length() - chuckSuffix.length()));
                        }
                    }
                }
                return std::move(Response(startLine,
                                          std::move(headers),
                                          std::move(body)));
            }else{
                return std::move(Response(408,
                                          std::string("Request Timeout"),
                                          std::string("{\"message\":\"Request Timeout\",\"status_code\":408}")));
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
                    
#ifndef _WIN32
inline UrlRequest::HostEntry operator "" _host(const char *s, size_t l){
    return UrlRequest::HostEntry{std::string(s,l)};
}
#endif
                    
//#pragma mark - Implementation
                    
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
                    UrlRequest::UrlRequest(Host host, Uri uri) :
//                    UrlRequest(),
                    _host(host),
                    _uri(uri)
                    {
                        this->timeout.tv_sec = 30;
                        this->timeout.tv_usec = 0;
                        _headers.emplace_back("Connection: close");
                    }
