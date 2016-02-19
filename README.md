# embeddedRest
A little library for making url requests in C++.

It allows to send synchronous HTTP 1.1 requests and gives pretty simpe interface to specify request parameters such as 

* HTTP method
* get parameters
* post parameters (json)
* port
* HTTP headers


# Example

Get request:

For example we need to request countries from `api.vk.com`. 

*Url* = "http://api.vk.com/method/database.getCities?lang=ru&country_id=1&count=1000&need_all=1"

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
