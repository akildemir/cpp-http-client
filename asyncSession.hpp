#ifndef ASYNC_SESSON_HPP
#define ASYNC_SESSON_HPP
//include beast
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
//include strand
#include <boost/asio/strand.hpp>
//include others
#include <iostream>
#include <string>

namespace beast = boost::beast;         
namespace http = beast::http;
namespace net = boost::asio;            
using tcp = boost::asio::ip::tcp;     


// Performs an HTTP GET and prints the response
class AsyncSession : public std::enable_shared_from_this<AsyncSession>
{
    tcp::resolver resolver_;
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    http::request<http::empty_body> empty_req_;
    http::request<http::string_body> loaded_req_;
    std::string req_type_;
    http::response<http::string_body> res_;
    std::function<void(std::string)> callback_;
    
    public:
    // Objects are constructed with a strand to
    // ensure that handlers do not execute concurrently.
    explicit
    AsyncSession(net::io_context& ioc)
        : resolver_(net::make_strand(ioc))
        , stream_(net::make_strand(ioc))
    {
    }

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
        resolver_.async_resolve(
            host,
            port,
            beast::bind_front_handler(
                &AsyncSession::on_resolve,
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
        resolver_.async_resolve(
            host,
            port,
            beast::bind_front_handler(
                &AsyncSession::on_resolve,
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
        stream_.expires_after(std::chrono::seconds(30));

        // Make the connection on the IP address we get from a lookup
        stream_.async_connect(
            results,
            beast::bind_front_handler(
                &AsyncSession::on_connect,
                shared_from_this()
            )
        );
    }

    void
    on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type)
    {
        if(ec)
            return fail(ec, "connect");

        // Set a timeout on the operation
        stream_.expires_after(std::chrono::seconds(30));

        if(req_type_ == "empty"){
            // Send the HTTP request to the remote host
            http::async_write(stream_, empty_req_,
                beast::bind_front_handler(
                    &AsyncSession::on_write,
                    shared_from_this()
                )
            );
        }

        if(req_type_ == "loaded"){
            // Send the HTTP request to the remote host
            http::async_write(stream_, loaded_req_,
                beast::bind_front_handler(
                    &AsyncSession::on_write,
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
                &AsyncSession::on_read,
                shared_from_this()
            )
        );
    }

    void
    on_read(
        beast::error_code ec,
        std::size_t bytes_transferred)
    {
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

        // Gracefully close the socket
        stream_.socket().shutdown(tcp::socket::shutdown_both, ec);

        // not_connected happens sometimes so don't bother reporting it.
        if(ec && ec != beast::errc::not_connected)
            return fail(ec, "shutdown");
    }
};

#endif // ASYNC_SESSON_HPP