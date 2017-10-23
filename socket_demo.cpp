#include <limits.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <utility>

#include "interprocess_future.hpp"
#include "new_process_executor.hpp"

class listening_socket
{
  public:
    listening_socket(int port)
      : file_descriptor_(socket(AF_INET, SOCK_STREAM, 0))
    {
      if(file_descriptor_ == -1)
      {
        throw std::system_error(errno, std::system_category(), "listening_socket ctor: Error after socket()");
      }

      sockaddr_in server_address{};

      server_address.sin_family = AF_INET;
      server_address.sin_addr.s_addr = INADDR_ANY;
      server_address.sin_port = port;

      // bind the socket to our selected port
      if(bind(file_descriptor_, reinterpret_cast<const sockaddr*>(&server_address), sizeof(server_address)) == -1)
      {
        throw std::system_error(errno, std::system_category(), "listening_socket ctor: Error after bind()");
      }

      // make this socket a listening socket, listen for a single connection
      if(listen(file_descriptor_, 1) == -1)
      {
        throw std::system_error(errno, std::system_category(), "listening_socket ctor: Error after listen()");
      }
    }

    listening_socket(listening_socket&& other)
      : file_descriptor_(-1)
    {
      std::swap(file_descriptor_, other.file_descriptor_);
    }

    ~listening_socket()
    {
      if(file_descriptor_ != -1)
      {
        if(close(file_descriptor_) == -1)
        {
          std::cerr << "listening_socket dtor: Error after close()" << std::endl;
        }
      }
    }

    int get()
    {
      return file_descriptor_;
    }

  private:
    int file_descriptor_;
};

class read_socket
{
  public:
    read_socket(listening_socket listener)
      : file_descriptor_(accept(listener.get(), nullptr, nullptr))
    {
      if(file_descriptor_ == -1)
      {
        throw std::system_error(errno, std::system_category(), "make_read_socket(): Error after listen()");
      }
    }

    read_socket(read_socket&& other)
      : file_descriptor_(-1)
    {
      std::swap(file_descriptor_, other.file_descriptor_);
    }

    ~read_socket()
    {
      if(file_descriptor_ != -1)
      {
        if(close(file_descriptor_) == -1)
        {
          std::cerr << "read_socket dtor: Error after close()" << std::endl;
        }
      }
    }

    read_socket(int port)
      : read_socket(listening_socket(port))
    {}

    int get() const
    {
      return file_descriptor_;
    }

    int release()
    {
      int result = -1;
      std::swap(file_descriptor_, result);
      return result;
    }

  private:
    int file_descriptor_;
};


class write_socket
{
  public:
    write_socket(const char* hostname, int port)
      : file_descriptor_(socket(AF_INET, SOCK_STREAM, 0))
    {
      if(file_descriptor_ == -1)
      {
        throw std::system_error(errno, std::system_category(), "write_socket ctor: Error after socket()");
      }

      // get the address of the server
      struct hostent* server = gethostbyname(hostname);
      if(server == nullptr)
      {
        throw std::system_error(errno, std::system_category(), "write_socket ctor: Error after gethostbyname()");
      }

      sockaddr_in server_address{};

      server_address.sin_family = AF_INET;
      std::memcpy(&server_address.sin_addr.s_addr, server->h_addr, server->h_length);
      server_address.sin_port = port;

      // keep attempting a connection while the server refuses
      int attempt = 0;
      int connect_result = 0;
      while((connect_result = connect(file_descriptor_, reinterpret_cast<sockaddr*>(&server_address), sizeof(server_address))) == -1 && attempt < 1000)
      {
        if(errno != ECONNREFUSED)
        {
          throw std::system_error(errno, std::system_category(), "write_socket ctor: Error after connect()");
        }

        ++attempt;
      }

      if(connect_result == -1)
      {
        throw std::system_error(errno, std::system_category(), "write_socket ctor: Error after connect()");
      }
    }

    int get() const
    {
      return file_descriptor_;
    }

  private:
    int file_descriptor_;
};


struct client
{
  std::string hostname;
  int port;

  void operator()() const
  {
    // create a promise corresponding to a connection with the server
    write_socket writer(hostname.c_str(), port);
    
    file_descriptor_ostream os(writer.get());
    
    interprocess_promise<int> promise(os);
    
    promise.set_value(13);
  }

  template<class InputArchive>
  friend void deserialize(InputArchive& ar, client& self)
  {
    ar(self.hostname, self.port);
  }

  template<class OutputArchive>
  friend void serialize(OutputArchive& ar, const client& self)
  {
    ar(self.hostname, self.port);
  }
};


int main()
{
  {
    // test normal case

    // get the name of this machine
    char hostname[HOST_NAME_MAX];
    if(gethostname(hostname, sizeof(hostname)) == -1)
    {
      throw std::system_error(errno, std::system_category(), "main(): Error after gethostname()");
    }

    int port = 71342;

    // start a client process
    new_process_executor exec;
    exec.execute(client{std::string(hostname), port});

    // create a future corresponding to the client
    interprocess_future<int> future(read_socket(port).release());

    int result = future.get();
    std::cout << "Received " << result << " from client" << std::endl;
    assert(result == 13);
  }
}

