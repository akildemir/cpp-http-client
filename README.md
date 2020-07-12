# Definition
Simple, easy to use, header only http-client library that uses boost asio and beast under the hood.
Brings boost beast to application level usage.

## Dependencies
- [Boost 1.72](https://www.boost.org/) (You can install boost using apt in Linux and Homebrew in Mac OS X)
- [Boost certify](https://github.com/djarek/certify) (boost 1.72 doesn't come with Certify. Certify is also header-only library. Clone Certfiy repo and place the header files under boost install directory.)

## Features
- Supports HTTP/HTTPS 1.1
- Supoorts both synchronous and asynchronous http calls

## Examples

#### ASynchronous call example

````
#include "httpasync.hpp" //include http async client
#include <iostream>
#include <map>

int main(){

    //define http client
    AsyncHttpClient http_async;

    // create headers with std::map
    std::map<std::string, std::string> headers;
    headers.insert(std::pair<std::string, std::string>("Content-Type", "application/json"));

    // make a http get request. Async calls are javascript promise like
    // although underlying concept has nothing to do with it.
    // get function will return immeditaly and the result will be passed
    // to the lamda supplied to the .then()
    http_async.get("https://postman-echo.com/get?foo=Bar", headers)
    .then([&](std::string result){
        std::cout << result << "\n";
    });

    // prevent program closing immediately    
    char a;
    std::cin >> a;
    
    return 0;
}
````
#### Synchronous call example

````
#include "http.hpp" //include http client
#include <iostream>
#include <map>

int main(){

    //define http client
    HttpClient http;   

    //create a json body to be sent
    const char* body = "{\"key1\":\"val1\", \"key2\":\"val2\"}";

    // create headers with std::map
    std::map<std::string, std::string> headers;
    headers.insert(std::pair<std::string, std::string>("Content-Type", "application/json"));

    //make a http get request
    auto result =  http.post("https://postman-echo.com/post", body, headers);

    //print result
    std::cout << result << "\n";

    return 0;
}
````

## Upcoming

- Webscoket Client
- Configuration support for clients

## Contribution

Clone the repo. Create a dev branch and create a pull request.

## LICENCE

[MIT](https://opensource.org/licenses/MIT)
