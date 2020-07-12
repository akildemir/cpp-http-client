#ifndef HTTP_HPP
#define HTTP_HPP

//include asio
#include <boost/asio.hpp>
//include beast
#include <boost/beast/core.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/fields.hpp>
//include certify for ssl
#include <boost/certify/extensions.hpp>
#include <boost/certify/https_verification.hpp>
//include other
#include <boost/lexical_cast.hpp>
#include <map>
#include <iostream>

namespace beast = boost::beast;
namespace asio = boost::asio;
namespace ssl = asio::ssl;
namespace http = boost::beast::http;
using tcp = boost::asio::ip::tcp;

class HttpClient {
    private:
        auto getSocket(asio::io_context& io, const std::string& host, const char* type);
        auto connect_with_ssl(const std::string& host);
        auto connect(const std::string& host);
        auto parse_url(std::string& url, std::string& type, std::string& host, std::string& path);
        template<class requestType>
        auto execute_request(
            const std::string& type, 
            const std::string& host, 
            const http::request<requestType>& request
        );
        template<class requestType>
        void send_request(
            std::unique_ptr<ssl::stream<tcp::socket>> socket_ptr, 
            const http::request<requestType>& request, 
            http::response<http::string_body>& response
        );
        template<class requestType>
        void send_request(
            std::unique_ptr<tcp::socket> socket_ptr, 
            const http::request<requestType>& request, 
            http::response<http::string_body>& response
        );

    public:
        HttpClient(){}  //empty to make the client stateless
        auto get(std::string url, const std::map<std::string, std::string> &headers);
        auto post(std::string url, const char *body, const std::map<std::string, std::string> &headers);
        auto delete_(std::string url, const std::map<std::string, std::string> &headers);
        auto put(std::string url, const char *body, const std::map<std::string, std::string> &headers);
};


auto 
HttpClient::parse_url(
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

auto
HttpClient::getSocket(
    asio::io_context& io, 
    const std::string& host, 
    const char* type
){   
    tcp::resolver resolver{io};
    tcp::socket socket{io};
    asio::connect(socket, resolver.resolve(host, type));
    return socket;
}

auto 
HttpClient::connect_with_ssl(
    const std::string& host
){
    //Create io context
    asio::io_context io;
        
    //create ssl context
    ssl::context ssl{ssl::context::tls_client};
    ssl.set_verify_mode(ssl::context::verify_peer | ssl::context::verify_fail_if_no_peer_cert);
    ssl.set_default_verify_paths();
    boost::certify::enable_native_https_server_verification(ssl);

    //get the socket and make ssl handsake
    auto socket_ptr = boost::make_unique<ssl::stream<tcp::socket>>(getSocket(io, host, "https"), ssl);
    boost::certify::set_server_hostname(*socket_ptr, host); 
    boost::certify::sni_hostname(*socket_ptr, host);
    socket_ptr->handshake(ssl::stream_base::handshake_type::client);

    return socket_ptr;
}

auto
HttpClient::connect(
    const std::string& host
){

    //Create io context and socket
    asio::io_context io;
    return boost::make_unique<tcp::socket>(getSocket(io, host, "http"));
}


template<class requestType>
void
HttpClient::send_request(
    std::unique_ptr<ssl::stream<tcp::socket>> socket_ptr, 
    const http::request<requestType>& request,
    http::response<http::string_body>& response
){

    //send the request
    http::write(*socket_ptr, request);

    //get the response
    beast::flat_buffer buffer;
    http::read(*socket_ptr, buffer, response);

    //Close the connection
    beast::error_code ec;
    socket_ptr->shutdown(ec);
    socket_ptr->next_layer().close(ec);
    if(ec){
        throw beast::system_error{ec};
    }
}

template<class requestType>
void
HttpClient::send_request(
    std::unique_ptr<tcp::socket> socket_ptr, 
    const http::request<requestType>& request,
    http::response<http::string_body>& response
){

    //send the request
    http::write(*socket_ptr, request);

    //get the response
    beast::flat_buffer buffer;
    http::read(*socket_ptr, buffer, response);

    //Close the connection
    beast::error_code ec;
    socket_ptr->shutdown(tcp::socket::shutdown_both, ec);
    if(ec){
        throw beast::system_error{ec};
    }
}

template<class requestType>
auto
HttpClient::execute_request(
    const std::string& type, 
    const std::string& host, 
    const http::request<requestType>& request
){

    http::response<http::string_body> response;

    if(type == "https"){

        //construct the socket and connect
        auto socket_ptr = connect_with_ssl(host);
        //send the request
        send_request<requestType>(std::move(socket_ptr), request, response);

    }else if(type == "http"){

        //construct the socket and connect
        auto socket_ptr = connect(host);
        //send the request
        send_request<requestType>(std::move(socket_ptr), request, response);
        
    }else{
        std::cout << "ONLY HTTP/HTTPS SUPPORTED! \n";
        std::terminate();
    }

    if(response.result() != http::status::ok){
        std::cout << "HTTP ERROR: " << response.result() << "\n";
        std::cout << "Status Code: " << response.result_int() << "\n";
        std::cout << "Response Body: " << response.body() << "\n";
    }

    return response.body();
}

/*
Make a Http GET request.
@param url: The URL for the request
@param headers: Http request headers if any
@returns response body
*/
auto
HttpClient::get(
    std::string url, 
    const std::map<std::string, std::string> &headers = {}
){   
    //parse the url
    std::string host;
    std::string type;
    std::string path;
    parse_url(url, type, host, path);


    //construct request object
    http::request<http::empty_body> request(http::verb::get, path, 11);

    //insert headers
    request.set(http::field::host, host);
    for(auto &pair : headers){
        request.insert(pair.first, pair.second);
    }


    return execute_request<http::empty_body>(type, host, request);
}


/*
Make a Http POST request.
@param url: The URL for the request
@param body: Request body in plain text.
@param headers: Http request headers if any
@returns response body
*/
auto 
HttpClient::post(
    std::string url, 
    const char *body, 
    const std::map<std::string, std::string> &headers = {}
){   

    //parse the url
    std::string host;
    std::string type;
    std::string path;
    parse_url(url, type, host, path);


    //construct request object
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
    
    return execute_request<http::string_body>(type, host, request);
}


/*
Make a Http PUT request.
@param url: The URL for the request
@param body: Request body in plain text.
@param headers: Http request headers if any
@returns response body
*/
auto 
HttpClient::put(
    std::string url, 
    const char *body, 
    const std::map<std::string, std::string> &headers = {}
){   
    //parse the url
    std::string host;
    std::string type;
    std::string path;
    parse_url(url, type, host, path);


    //construct request object
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
    
    return execute_request<http::string_body>(type, host, request);
}


/*
Make a Http DELETE request.
@param url: The URL for the request
@param headers: Http request headers if any
@returns response body
*/
auto 
HttpClient::delete_(
    std::string url, 
    const std::map<std::string, std::string> &headers = {}
){

    //parse the url
    std::string host;
    std::string type;
    std::string path;
    parse_url(url, type, host, path);


    //construct request object
    http::request<http::empty_body> request(http::verb::delete_, path, 11);

    //insert headers
    request.set(http::field::host, host);
    for(auto &pair : headers){
        request.insert(pair.first, pair.second);
    }

    return execute_request<http::empty_body>(type, host, request);    
}
#endif //HTTP_HPP