#ifndef HTTP_ASYNC_HPP
#define HTTP_ASYNC_HPP
//include certify for ssl
#include <boost/certify/extensions.hpp>
#include <boost/certify/https_verification.hpp>
//include session clients
#include "asyncSslSession.hpp"
#include "asyncSession.hpp"
//include other
#include <boost/lexical_cast.hpp>
#include <iostream>
#include <string>
#include <map>
#include <thread>

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

template<class requestType>
void execute_request(
    const std::string type, 
    const std::string host, 
    const http::request<requestType> request, 
    std::function<void(std::string)> callback
){
    //create io context
    asio::io_context io;

    if(type == "https"){

        //create ssl context
        ssl::context ssl{ssl::context::tls_client};
        ssl.set_verify_mode(ssl::context::verify_peer | ssl::context::verify_fail_if_no_peer_cert);
        ssl.set_default_verify_paths();
        boost::certify::enable_native_https_server_verification(ssl);

        //create a async ssl session and run the io 
        std::make_shared<AsyncSslSession>(asio::make_strand(io), ssl)->run(host.c_str(), "https", request, callback);
        io.run();

    }else if(type == "http"){

        //create a async  session and run the io 
        std::make_shared<AsyncSession>(io)->run(host.c_str(), "http", request, callback);
        io.run();
        
    }else{
        std::cout << "ONLY HTTP/HTTPS SUPPORTED! \n";
        std::terminate();
    }

}

class AsyncHttpClient {

    private:
        template<class requestType>
        static http::request<requestType> request_;
        std::string request_type_;
        std::string host_;
        std::string type_;
        auto parse_url(std::string& url, std::string& type, std::string& host, std::string& path);

    public:
        AsyncHttpClient(){}
        auto get(std::string url, const std::map<std::string, std::string> &headers);
        auto post(std::string url, const char* body, const std::map<std::string, std::string> &headers);
        auto put(std::string url, const char* body, const std::map<std::string, std::string> &headers);
        auto delete_(std::string url, const std::map<std::string, std::string> &headers);
        void then(const std::function<void(std::string)>& lamda);

};

template<class requestType>
http::request<requestType> AsyncHttpClient::request_;

auto 
AsyncHttpClient::parse_url(
    std::string& url, 
    std::string& type, 
    std::string& host, 
    std::string& path
){

    unsigned short column_index = url.find(':');
    type = url.substr(0, column_index);

    url = url.substr(column_index + 3);
    try{
        unsigned short slash_index =  url.find('/');
        host = url.substr(0, slash_index);
        path = url.substr(slash_index);
    }catch(std::out_of_range ex){ //user doesn't have to put "/" at the end of domain
        path = "/";
        host = url;
    }
}

/*
Make a Http GET request.
@param url: The URL for the request
@param headers: Http request headers if any
@returns the client instance. You should call the .then() 
to provide the callback to be invoked when the response recieved.
*/
auto 
AsyncHttpClient::get(
    std::string url, 
    const std::map<std::string, std::string> &headers = {}
){

    //parse the url
    std::string host;
    std::string type;
    std::string path;
    parse_url(url, type, host, path);

    //create a get request
    http::request<http::empty_body> request(http::verb::get, path, 11);

    //insert headers
    request.set(http::field::host, host);
    for(auto &pair : headers){
        request.insert(pair.first, pair.second);
    }

    //save varibles to be used at .then()
    request_type_ = "empty";
    request_<http::empty_body> = request;
    type_ = type;
    host_ = host;
    return *this;
}


/*
Make a Http POST request.
@param url: The URL for the request
@param body: Request body in plain text.
@param headers: Http request headers if any
@returns the client instance. You should call the .then() 
to provide the callback to be invoked when the response recieved.
*/
auto 
AsyncHttpClient::post(
    std::string url,
    const char* body,
    const std::map<std::string, std::string> &headers = {}
){

    //parse the url
    std::string host;
    std::string type;
    std::string path;
    parse_url(url, type, host, path);

    //create a get request
    http::request<http::string_body> request(http::verb::post, path, 11);

    //insert headers
    request.set(http::field::host, host);
    for(auto &pair : headers){
        request.insert(pair.first, pair.second);
    }

    //insert request body
    request.body() = body;
    request.prepare_payload();
    request.set(http::field::content_length, boost::lexical_cast<std::string>(strlen(body)));

    //save varibles to be used at .then()
    request_type_ = "loaded";
    request_<http::string_body> = request;
    type_ = type;
    host_ = host;
    return *this;
}


/*
Make a Http PUT request.
@param url: The URL for the request
@param body: Request body in plain text.
@param headers: Http request headers if any
@returns the client instance. You should call the .then() 
to provide the callback to be invoked when the response recieved.
*/
auto 
AsyncHttpClient::put(
    std::string url,
    const char* body,
    const std::map<std::string, std::string> &headers = {}
){

    //parse the url
    std::string host;
    std::string type;
    std::string path;
    parse_url(url, type, host, path);

    //create a get request
    http::request<http::string_body> request(http::verb::put, path, 11);

    //insert headers
    request.set(http::field::host, host);
    for(auto &pair : headers){
        request.insert(pair.first, pair.second);
    }


    //insert request body
    request.body() = body;
    request.prepare_payload();
    request.set(http::field::content_length, boost::lexical_cast<std::string>(strlen(body)));

    //save varibles to be used at .then()
    request_type_ = "loaded";
    request_<http::string_body> = request;
    type_ = type;
    host_ = host;
    return *this;
}


/*
Make a Http DELETE request.
@param url: The URL for the request
@param headers: Http request headers if any
@returns the client instance. You should call the .then() 
to provide the callback to be invoked when the response recieved.
*/
auto 
AsyncHttpClient::delete_(
    std::string url, 
    const std::map<std::string, std::string> &headers = {}
){

    //parse the url
    std::string host;
    std::string type;
    std::string path;
    parse_url(url, type, host, path);

    //create a get request
    http::request<http::empty_body> request(http::verb::delete_, path, 11);

    //insert headers
    request.set(http::field::host, host);
    for(auto &pair : headers){
        request.insert(pair.first, pair.second);
    }

    //save varibles to be used at .then()
    request_type_ = "empty";
    request_<http::empty_body> = request;
    type_ = type;
    host_ = host;
    return *this;
}

/*
Function to be called to provide the callback function
that will be invoked when the response received.
@param callback: callback to be invoked when the response
received.
*/
void
AsyncHttpClient::then(const std::function<void(std::string)>& callback){
    if(request_type_ == "empty"){
        std::thread(
            execute_request<http::empty_body>, 
            type_, 
            host_, 
            request_<http::empty_body>,
            callback
        ).detach();
    }

    if(request_type_ == "loaded"){
        std::thread(
            execute_request<http::string_body>, 
            type_, 
            host_, 
            request_<http::string_body>, 
            callback
        ).detach();
    }

}
#endif // HTTP_ASYNC_HPP