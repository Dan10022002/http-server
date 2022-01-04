// Copyright 2021 Dan10022002 dan10022002@mail.ru 
#include "storage_json.h"
#include "suggestions_collection.h"
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <thread>
#include <chrono>
#include <utility>
#include <iostream>

// 1 - address, 2 - port, 3 - storage

std::string make_json(const nlohmann::json& data) {
  std::stringstream ss;
  if (data.is_null())
    ss << "No suggestions";
  else
    ss << data;
  return ss.str();
}

template <typename T>
struct send_lambda
{
  T& _stream;
  bool& _close;
  boost::beast::error_code& _error;

  explicit send_lambda(T& stream, bool& close, boost::beast::error_code& error):
   _stream(stream), _close(close), _error(error) {};

  template <bool isRequest, class Body, class Fields>
  void operator()(boost::beast::http::message<isRequest, Body, Fields>&& _message) const {
    _close = _message.need_eof();
    boost::beast::http::serializer<isRequest, Body, Fields> sr{_message};
    boost::beast::http::write(_stream, sr, _error);
  }
};


void send_response(boost::beast::http::request<boost::beast::http::string_body>&& request,
                   send_lambda<boost::asio::ip::tcp::socket>& lambda,
                   const std::shared_ptr<suggestions_collection>& suggestions,
                   const std::shared_ptr<std::timed_mutex>& mutex) {
  auto const bad_request = [&request](boost::beast::string_view why) {
    boost::beast::http::response<boost::beast::http::string_body> res{
        boost::beast::http::status::bad_request, request.version()};
    res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(boost::beast::http::field::content_type, "text/html");
    res.keep_alive(request.keep_alive());
    res.body() = std::string(why);
    res.prepare_payload();
    return res;
  };

  auto const not_found = [&request](boost::beast::string_view target) {
    boost::beast::http::response<boost::beast::http::string_body> res{
        boost::beast::http::status::not_found, request.version()};
    res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(boost::beast::http::field::content_type, "text/html");
    res.keep_alive(request.keep_alive());
    res.body() = "The resource '" + std::string(target) + "' was not found.";
    res.prepare_payload();
    return res;
  };

  if (request.method() == boost::beast::http::verb::get) {
    return lambda(bad_request("Hello!"));
  }

  if ((request.method() != boost::beast::http::verb::post) &&
      (request.method() != boost::beast::http::verb::head)) {
    return lambda(bad_request("Unknown HTTP-method"));
  }

  if (request.target() != "/v1/api/suggest") {
    return lambda(not_found(request.target()));
  }

  nlohmann::json input_body;
  try {
    input_body = nlohmann::json::parse(request.body());
  } catch (std::exception& e) {
    return lambda(bad_request(e.what()));
  }

  boost::optional<std::string> input;
  try {
    input = input_body.at("input").get<std::string>();
  } catch (std::exception& e) {
    return lambda(bad_request(R"(Uncorrected JSON format)"));
  }

  if (!input.has_value()) {
    return lambda(bad_request(R"(Value is clear)"));
  }

  mutex->lock();
  auto result = suggestions->suggest(input.value());
  mutex->unlock();
  boost::beast::http::string_body::value_type body = make_json(result);
  auto const size = body.size();

  if (request.method() == boost::beast::http::verb::head) {
    boost::beast::http::response<boost::beast::http::empty_body> res{
        boost::beast::http::status::ok, request.version()};
    res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(boost::beast::http::field::content_type, "application/json");
    res.content_length(size);
    res.keep_alive(request.keep_alive());
    return lambda(std::move(res));
  }

  boost::beast::http::response<boost::beast::http::string_body> res{
      std::piecewise_construct, std::make_tuple(std::move(body)),
      std::make_tuple(boost::beast::http::status::ok, request.version())};
  res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
  res.set(boost::beast::http::field::content_type, "application/json");
  res.content_length(size);
  res.keep_alive(request.keep_alive());
  return lambda(std::move(res));
}

void update_suggestions(const std::shared_ptr<storage_json>& storage,
                        const std::shared_ptr<suggestions_collection>& suggestions,
                        const std::shared_ptr<std::timed_mutex>& mutex){
  for (;;)
  {
    mutex->lock();
    storage->load();
    suggestions->update(storage->get_storage());
    mutex->unlock();
    std::this_thread::sleep_for(std::chrono::minutes(15));
  }
}

void session(boost::asio::ip::tcp::socket& socket,
             const std::shared_ptr<suggestions_collection>& suggestions,
             const std::shared_ptr<std::timed_mutex>& mutex) {
  boost::beast::error_code error;
  boost::beast::flat_buffer _buffer;
  bool close = false;
  send_lambda<boost::asio::ip::tcp::socket> _lambda {socket, close, error};
  for (;;)
  {
    boost::beast::http::request<boost::beast::http::string_body> _request;
    boost::beast::http::read(socket, _buffer, _request, error);
    if (error == boost::beast::http::error::end_of_stream) break;
    if (error)
    {
      std::cout << "Read: " << error.message() << "\n";
    }
    send_response(std::move(_request), _lambda, suggestions, mutex);
    if (error)
    {
      std::cout << "Write: " << error.message() << "\n";
    }
  }
  socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send, error);
}

int Run_server(int args, char* argv[]) {
  if (args != 4)
 {
    std::cout << "Help: ..." << std::endl;
    return EXIT_FAILURE;
  } else {
    std::shared_ptr<std::timed_mutex> mutex =
        std::make_shared<std::timed_mutex>();
    std::shared_ptr<suggestions_collection> suggestions =
        std::make_shared<suggestions_collection>();
    std::shared_ptr<storage_json> storage = std::make_shared<storage_json>(argv[3]);
    auto const address = boost::asio::ip::make_address(argv[1]);
    uint16_t const port = static_cast<uint16_t>(std::stoi(argv[2]));

    boost::asio::io_service _service;
    boost::asio::ip::tcp::endpoint _endpoint(address, port);
    boost::asio::ip::tcp::acceptor _acceptor(_service, _endpoint);

    std::thread(update_suggestions, storage, suggestions, mutex).detach();

    for (;;)
    {
      boost::asio::ip::tcp::socket _socket(_service);
      _acceptor.accept(_socket);
      std::thread{std::bind(&session, std::move(_socket), suggestions, mutex)}.detach();
    }
    return EXIT_FAILURE;
  }
}

int main(int args, char* argv[])
{
  return Run_server(args, argv);
}
