//
//  Response.hpp
//  urldTest
//
//  Created by John on 12.12.15.
//  Copyright © 2015 Outlaw Studio. All rights reserved.
//

#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <iostream>

class Response{
public:
    struct IncorrectStartLineException{
        const std::string startLine;
    };
protected:
    std::string _body;
    std::vector<std::string> _headers;
    std::string _httpVersion;
    int _statusCode;
    std::string _statusDescription;
    
    static void parseStartLine(const std::string &startLine,
                               decltype(_httpVersion) &httpVersion,
                               decltype(_statusCode) &statusCode,
                               decltype(_statusDescription) &statusDescription) throw(IncorrectStartLineException)
    {
        std::stringstream stream(startLine);
        if(stream.good()){
            stream>>httpVersion;
            if(stream.good()){
                stream>>statusCode;
                if(stream.good()){
                    while(stream.good()){
                        std::string word;
                        stream>>word;
                        statusDescription+=word+" ";
                    }
                }else{
                    throw IncorrectStartLineException{startLine};
                }
            }else{
                throw IncorrectStartLineException{startLine};
            }
        }else{
            throw IncorrectStartLineException{startLine};
        }
    }
public:
    Response(const std::string &startLine,decltype(_headers)&&headers,decltype(_body)&&body) throw(IncorrectStartLineException):
    _headers(std::move(headers)),
    _body(std::move(body))
    {
        parseStartLine(startLine, _httpVersion, _statusCode, _statusDescription);
    }
    
    Response(decltype(_statusCode)statusCode_,decltype(_statusDescription)&&statusDescription_,decltype(_body)&&body_):
    _statusCode(statusCode_),
    _statusDescription(std::move(statusDescription_)),
    _body(std::move(body_)){}
    
    const decltype(_body)& body() const{
        return _body;
    }
    
    const decltype(_headers)& headers() const{
        return _headers;
    }
    
    const decltype(_httpVersion)& httpVersion() const{
        return _httpVersion;
    }
    
    decltype(_statusCode) statusCode() const{
        return _statusCode;
    }
    
    const decltype(_statusDescription)& statusDescription() const{
        return _statusDescription;
    }
};
