//
// Created by John Zakharov on 29.03.16.
// Copyright (c) 2016 Outlaw Studio. All rights reserved.
//

#ifndef JAKO_JSONVALUEADAPTER_HPP
#define JAKO_JSONVALUEADAPTER_HPP

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include <memory>
#include <cmath>
#include <map>
#include <algorithm>
#include <experimental/optional>
#include <ctime>

struct JsonValueAdapter{
    JsonValueAdapter()=delete;
    
    template<class T>
    JsonValueAdapter(const T &t):
    JsonValueAdapter(t.jsonObject()){}

    JsonValueAdapter(const char *s){
        _data=new std::string(s);
        _type=rapidjson::kStringType;
    }
    
    JsonValueAdapter(double d){
        _data=new decltype(d)(d);
        _type=rapidjson::kNumberType;
    }

    JsonValueAdapter(int i):JsonValueAdapter(double(i)){}

    JsonValueAdapter(bool b){
        _data=new decltype(b)(b);
        _type=(b)?rapidjson::kTrueType:rapidjson::kFalseType;
    }
    
    JsonValueAdapter(const struct tm &timeValue,const std::string &format="%Y-%M-%D"):
    JsonValueAdapter([&]{
        char str[64];
        ::strftime(str, sizeof(str), format.c_str(), &timeValue);
        return std::string(str);
    }()){}
    
    template<class T>
    JsonValueAdapter(std::shared_ptr<T> ptr):_type(rapidjson::kNullType){
        if(ptr){
            JsonValueAdapter other(*ptr);
            *this=std::move(other);
        }else{
            _type=rapidjson::kNullType;
            _data=nullptr;
        }
    }
    
    template<class T>
    JsonValueAdapter(std::experimental::optional<T> ptr):_type(rapidjson::kNullType){
        if(ptr){
            JsonValueAdapter other(*ptr);
            *this=std::move(other);
        }else{
            _type=rapidjson::kNullType;
            _data=nullptr;
        }
    }

    typedef std::vector<std::pair<std::string,JsonValueAdapter>> Object_t;
    typedef std::vector<JsonValueAdapter> Array_t;

    JsonValueAdapter(Object_t jsonObject){
        _data=new decltype(jsonObject)(std::move(jsonObject));
        _type=rapidjson::kObjectType;
    }
    
    template<class T>
    JsonValueAdapter(std::map<std::string,T> map):JsonValueAdapter(std::move([&map]{
        Object_t obj;
        obj.reserve(map.size());
        for(auto &p:map){
            obj.push_back({p.first,p.second.jsonObject()});
        }
        return std::move(obj);
    }())){}
    
    template<class T>
    JsonValueAdapter(std::vector<T> v):JsonValueAdapter(std::move([&v]{
        Array_t a;
        a.reserve(v.size());
        for(auto &obj:v){
//            a.push_back(obj.jsonObject());
            a.push_back(JsonValueAdapter(obj));
        }
        return std::move(a);
    }())){}
    
    JsonValueAdapter(Array_t a){
        _data=new decltype(a)(std::move(a));
        _type=rapidjson::kArrayType;
    }

    JsonValueAdapter(std::string s){
        _data=new decltype(s)(std::move(s));
        _type=rapidjson::kStringType;
    }

    JsonValueAdapter(const JsonValueAdapter &other):
            _type(other._type){
        switch(_type){
            case rapidjson::kStringType:{
                _data=new std::string(other.string());
            }break;
            case rapidjson::kNumberType:{
                _data=new double(other.dbl());
            }break;
            case rapidjson::kObjectType:{
                _data=new Object_t(other.object());
            }break;
            case rapidjson::kArrayType:{
                _data=new Array_t(other.array());
            }break;
            case rapidjson::kTrueType:
            case rapidjson::kFalseType:{
                _data=new bool(other.boolean());
            }break;
            case rapidjson::kNullType:{
                _data=nullptr;
            }break;
            default:break;
        }
    }

    JsonValueAdapter& operator=(const JsonValueAdapter &other){
        this->clean();
        _type=other._type;
        switch(_type){
            case rapidjson::kStringType:{
                _data=new std::string(other.string());
            }break;
            case rapidjson::kNumberType:{
                _data=new double(other.dbl());
            }break;
            case rapidjson::kObjectType:{
                _data=new Object_t(other.object());
            }break;
            case rapidjson::kArrayType:{
                _data=new Array_t(other.array());
            }break;
            case rapidjson::kTrueType:
            case rapidjson::kFalseType:{
                _data=new bool(other.boolean());
            }break;
            case rapidjson::kNullType:{
                _data=nullptr;
            }break;
            default:break;
        }
        return *this;
    }

    JsonValueAdapter(JsonValueAdapter &&other):
            _type(other._type),
            _data(other._data){
        other._type=rapidjson::kNullType;
        other._data=nullptr;
    }

    JsonValueAdapter& operator=(JsonValueAdapter &&other){
        this->clean();
        _data=other._data;
        other._data=nullptr;
        _type=other._type;
        other._type=rapidjson::kNullType;
        return *this;
    }

    ~JsonValueAdapter(){
        this->clean();
    }

    bool boolean()const{
        return *(bool*)_data;
    }

    double dbl()const{
        return *(double*)_data;
    }

    const std::string& string()const{
        return *(std::string*)_data;
    }

    const Object_t& object()const{
        return *(Object_t*)_data;
    }
    
    const Array_t& array()const{
        return *(Array_t*)_data;
    }

    rapidjson::Type type()const{
        return _type;
    }

    std::string toString()const{
        rapidjson::Document d;
        switch(_type){
            case rapidjson::kStringType:
//                d.SetString(this->string().c_str(),int(this->string().length()));
                d.SetString(this->string().c_str(),d.GetAllocator());
                break;
            case rapidjson::kNumberType:{
                const auto doubleValue=this->dbl();
                if(double_is_int(doubleValue)){
                    d.SetInt(int(doubleValue));
                }else{
                    d.SetDouble(doubleValue);
                }
            }break;
            case rapidjson::kArrayType:
                d.SetArray();
                encodeJsonArray(this->array(), d, d.GetAllocator());
                break;
            case rapidjson::kObjectType:
                d.SetObject();
                encodeJsonObject(this->object(), d,d.GetAllocator());
                break;
            case rapidjson::kTrueType:
            case rapidjson::kFalseType:
                d.SetBool(this->boolean());
                break;
            case rapidjson::kNullType:{
                d.SetNull();
            }break;
            default:
                break;
        }
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<decltype(buffer)> writer(buffer);
        d.Accept(writer);
        return buffer.GetString();
    }
    
    static rapidjson::Document stringToJson(const std::string &jsonString){
        rapidjson::Document d;
        d.Parse(jsonString.c_str());
        return std::move(d);
    }
protected:
    rapidjson::Type _type;
    void *_data;

    void clean(){
        switch(_type){
            case rapidjson::kStringType:{
                auto stringPointer=(std::string*)_data;
                delete stringPointer;
            }break;
            case rapidjson::kNumberType:{
                auto doublePointer=(double*)_data;
                delete doublePointer;
            }break;
            case rapidjson::kArrayType:{
                auto arrayPointer=(Array_t*)_data;
                delete arrayPointer;
            }break;
            case rapidjson::kObjectType:{
                auto jsonObjectPointer=(Object_t*)_data;
                delete jsonObjectPointer;
            }break;
            case rapidjson::kTrueType:
            case rapidjson::kFalseType:{
                auto boolPointer=(bool*)_data;
                delete boolPointer;
            }break;
            case rapidjson::kNullType:{
                //..
            }break;
            default:break;
        }
        _type=rapidjson::kNullType;
    }
    
    static void encodeJsonArray(const Array_t &a,rapidjson::Value &d,rapidjson::Document::AllocatorType &allocator){
        for(auto &value:a){
            switch(value.type()){
                case rapidjson::kStringType:{
                    d.PushBack(rapidjson::Value(value.string().c_str(),allocator).Move(), allocator);
                }break;
                case rapidjson::kNumberType:{
                    const auto doubleValue=value.dbl();
                    if(double_is_int(doubleValue)){
                        d.PushBack(rapidjson::Value(int(value.dbl())).Move(), allocator);
                    }else{
                        d.PushBack(rapidjson::Value(value.dbl()).Move(), allocator);
                    }
                }break;
                case rapidjson::kObjectType:{
                    rapidjson::Value d2;
                    d2.SetObject();
                    encodeJsonObject(value.object(), d2,allocator);
                    d.PushBack(d2.Move(), allocator);
                }break;
                case rapidjson::kArrayType:{
                    rapidjson::Value d2;
                    d2.SetArray();
                    encodeJsonArray(value.array(), d2, allocator);
                    d.PushBack(d2.Move(), allocator);
                }break;
                case rapidjson::kTrueType:
                case rapidjson::kFalseType:{
                    rapidjson::Value d2;
                    d2.SetBool(value.boolean());
                    d.PushBack(d2.Move(), allocator);
                }break;
                case rapidjson::kNullType:{
                    rapidjson::Value d2;
                    d2.SetNull();
                    d.PushBack(d2.Move(), allocator);
                }break;
            }
        }
    }

    static void encodeJsonObject(const Object_t &obj,rapidjson::Value &d,rapidjson::Document::AllocatorType &allocator){
        for(auto &p:obj){
            const auto &value=p.second;
            switch(value.type()){
                case rapidjson::kStringType:{
                    d.AddMember(rapidjson::Value(p.first.c_str(),allocator).Move(), rapidjson::Value(value.string().c_str(),allocator).Move(), allocator);
                }break;
                case rapidjson::kNumberType:{
                    const auto doubleValue=value.dbl();
                    if(double_is_int(doubleValue)){
                        d.AddMember(rapidjson::Value(p.first.c_str(),allocator).Move(), rapidjson::Value(int(value.dbl())).Move(), allocator);
                    }else{
                        d.AddMember(rapidjson::Value(p.first.c_str(),allocator).Move(), rapidjson::Value(value.dbl()).Move(), allocator);
                    }
                }break;
                case rapidjson::kObjectType:{
                    rapidjson::Value d2;
                    d2.SetObject();
                    encodeJsonObject(value.object(), d2,allocator);
                    d.AddMember(rapidjson::Value(p.first.c_str(),allocator).Move(), d2.Move(), allocator);
                }break;
                case rapidjson::kArrayType:{
                    rapidjson::Value d2;
                    d2.SetArray();
                    encodeJsonArray(value.array(), d2, allocator);
                    d.AddMember(rapidjson::Value(p.first.c_str(),allocator).Move(), d2.Move(), allocator);
                }break;
                case rapidjson::kTrueType:
                case rapidjson::kFalseType:{
                    rapidjson::Value d2;
                    d2.SetBool(value.boolean());
                    d.AddMember(rapidjson::Value(p.first.c_str(),allocator).Move(), d2.Move(), allocator);
                }break;
                case rapidjson::kNullType:{
                    rapidjson::Value d2;
                    d2.SetNull();
                    d.AddMember(rapidjson::Value(p.first.c_str(),allocator).Move(), d2.Move(), allocator);
                }break;
            }
        }
    }
    
    static bool double_is_int(double trouble){
        double absolute = std::abs( trouble );
        return absolute == floor(absolute);
    };
};


#endif //JAKO_JSONVALUEADAPTER_HPP
