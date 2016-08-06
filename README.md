# embeddedRest
A little library for making url requests in C++.

# warning: do not use embeddedRest on iOS if you are gonna post your app to the App Store - embeddedRest doesn't have IPv6 support so your app will be rejected.

It allows to send synchronous HTTP 1.1 requests and gives pretty simple interface to specify request parameters such as 

* HTTP method
* get parameters
* post parameters (json)
* port
* HTTP headers

embeddedRest is a lightweight header-only library designed especially for mobile apps (iOS and Android) but it also can compile at any platform familiar with C++11.

# GET request example

For example we need to request cities from `api.vk.com`. 

**Url** = "http://api.vk.com/method/database.getCities?lang=ru&country_id=1&count=1000&need_all=1"

Code:
```
#include "UrlRequest.hpp"

//...

UrlRequest request;
request.host("api.vk.com");
const auto countryId=1;
const auto count=1000;
request.uri("/method/database.getCities",{
    {"lang","ru"},
    {"country_id",countryId},
    {"count",count},
    {"need_all","1"},
});
request.addHeader("Content-Type: application/json");
auto response=std::move(request.perform());
if(response.statusCode()==200){
  cout<<"status code = "<<response.statusCode()<<", body = *"<<response.body()<<"*"<<endl;
}else{
  cout<<"status code = "<<response.statusCode()<<", description = "<<response.statusDescription()<<endl;
}      
```

# POST request example

We need to register a user and pass some arguments (login, email, password and age) to body as a json object.
```
UrlRequest request;
request.host("api.my-domain.com");
request.uri("/users/registration");
const auto login="Deadpool";
const auto email="dead@pool.com";
const auto password="dead_mf_pool123";
const auto age=24;
request.body({
    {"login",login},
    {"email",email},
    {"password",password},
    {"age",age},
});
request.method("POST");
request.addHeader("Content-Type: application/json");
auto response=std::move(request.perform());
if(response.statusCode()==200){
  cout<<"status code = "<<response.statusCode()<<", body = *"<<response.body()<<"*, user is registered!"<<endl;
}else{
  cout<<"status code = "<<response.statusCode()<<", description = "<<response.statusDescription()<<endl;
} 
```

# Add embeddedRest to your project.

Just add "*.hpp" files from root folder into your project header directory and `#include` them. Also `embeddedRest` has dependency - [rapidjson](https://github.com/miloyip/rapidjson/) json-processor. `rapidjson` is also a header-only library so it is very easy to include it to your project.

# Advanced

**Timeout**

Every `UrlRequest` instance has 30 seconds default timeout. If you want to change it just assign `.timeout` value to your request.
```
UrlRequest request;
request.timeout={10,0};     //  10 seconds and 0 microseconds..
```
`timeout` property has `struct timeval` type. This type is declared in C standard library and represents time with microseconds precision.

**Passing array as get parameter**

If you want to pass array to url as get parameter using square braces notation (example: *jako.online/api/v1/subscribes/my?lang=ru&type[]=vk&type[]=company*) you have to pass std::vector of your values to *uri* member function arguments. Example:

```
UrlRequest request;
request.host("jako.online").uri("/api/v1/subscribes/my",{
    {"lang",Language::current()},
    {"type",std::vector<std::string>{
        "vk",
        "company",
    }},
}).addHeader("Content-Type: application/json");
```
Request url will be parsed to *jako.online/api/v1/subscribes/my?lang=ru&type[]=vk&type[]=company*
