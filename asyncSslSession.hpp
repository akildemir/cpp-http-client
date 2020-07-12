#ifndef ASYNC_SSl_SESSON_HPP
#define ASYNC_SSL_SESSON_HPP
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/strand.hpp>


#include <iostream>
#include <string>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

//------------------------------------------------------------------------------

// Report a failure
void
fail(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

// Performs an HTTP GET and prints the response
class AsyncSslSession : public std::enable_shared_from_this<AsyncSslSession>
{
    tcp::resolver resolver_;
    beast::ssl_stream<beast::tcp_stream> stream_;
    beast::flat_buffer buffer_;
    http::request<http::empty_body> empty_req_;
    http::request<http::string_body> loaded_req_;
    std::string req_type_;
    http::response<http::string_body> res_;
    std::function<void(std::string)> callback_;

public:
    explicit AsyncSslSession(
        net::executor ex,
        ssl::context& ctx
    ) : resolver_(ex) , stream_(ex, ctx){}

    // Start the asynchronous operation
    void
    run(
        char const* host,
        char const* port,
        const http::request<http::empty_body>& request,
        std::function<void(std::string)> callback
    ){

        empty_req_ = request;
        callback_ = callback;
        req_type_ = "empty";

        // Look up the domain name
        resolver_.async_resolve(host, port,
            beast::bind_front_handler(
                &AsyncSslSession::on_resolve,
                shared_from_this()
            )
        );
    }

    void
    run(
        char const* host,
        char const* port,
        const http::request<http::string_body>& request,
        std::function<void(std::string)> callback
    ){

        loaded_req_ = request;
        callback_ = callback;
        req_type_ = "loaded";

        // Look up the domain name
        resolver_.async_resolve(host, port,
            beast::bind_front_handler(
                &AsyncSslSession::on_resolve,
                shared_from_this()
            )
        );
    }

    void
    on_resolve(
        beast::error_code ec,
        tcp::resolver::results_type results)
    {
        if(ec)
            return fail(ec, "resolve");
        
        // Set a timeout on the operation
        beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

        // Make the connection on the IP address we get from a lookup
        beast::get_lowest_layer(stream_).async_connect(
            results,
            beast::bind_front_handler(
                &AsyncSslSession::on_connect,
                shared_from_this()
            )
        );
    }

    void
    on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type)
    {
        if(ec)
            return fail(ec, "connect");

        // Perform the SSL handshake
        stream_.async_handshake(
            ssl::stream_base::client,
            beast::bind_front_handler(
                &AsyncSslSession::on_handshake,
                shared_from_this()
            )
        );
    }

    void
    on_handshake(beast::error_code ec)
    {
        if(ec)
            return fail(ec, "handshake");

        // Set a timeout on the operation
        beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

        if(req_type_ == "empty"){

            // Send the HTTP request to the remote host
            http::async_write(stream_, empty_req_,
                beast::bind_front_handler(
                    &AsyncSslSession::on_write,
                    shared_from_this()
                )
            );

        }

        if(req_type_ == "loaded"){

            // Send the HTTP request to the remote host
            http::async_write(stream_, loaded_req_,
                beast::bind_front_handler(
                    &AsyncSslSession::on_write,
                    shared_from_this()
                )
            );

        }
    }

    void
    on_write(
        beast::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if(ec)
            return fail(ec, "write");

        // Receive the HTTP response
        http::async_read(stream_, buffer_, res_,
            beast::bind_front_handler(
                &AsyncSslSession::on_read,
                shared_from_this()
            )
        );
    }

    void
    on_read(
        beast::error_code ec,
        std::size_t bytes_transferred
    ){
        boost::ignore_unused(bytes_transferred);

        if(ec)
            return fail(ec, "read");

        //check for the error
        if(res_.result() != http::status::ok){
            std::cout << "HTTP ERROR: " << res_.result() << "\n";
            std::cout << "Status Code: " << res_.result_int() << "\n";
            std::cout << "Response Body: " << res_.body() << "\n";
        }

        //Send the message to the callback
        callback_(res_.body());

        // Set a timeout on the operation
        beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

        // Gracefully close the stream
        stream_.async_shutdown(
            beast::bind_front_handler(
                &AsyncSslSession::on_shutdown,
                shared_from_this()
            )
        );

    }

    void
    on_shutdown(beast::error_code ec)
    {   
        

        if(ec == net::error::eof || ec == net::ssl::error::stream_truncated)
        {
            // Rationale:
            // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
            ec = {};
        }
        if(ec)
            return fail(ec, "shutdown");

    }
};

#endif // ASYNC_SSL_SESSON_HPP