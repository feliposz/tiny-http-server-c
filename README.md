# Tiny HTTP Server

This project implements a small, iterative HTTP server in C, designed primarily as an educational exercise to deepen understanding of network programming concepts. It's built upon the principles discussed in chapters **11: Network Programming** and **12: Concurrent Programming** of the book [Computer Systems: A Programmer's Perspective (CS:APP)](https://csapp.cs.cmu.edu/3e/home.html) .

It's a foundational server capable of handling basic HTTP requests and serving both static and dynamic content.

## Features

  * **HTTP/1.1 Support**: Handles `GET`, `HEAD`, and `POST` HTTP methods.
  * **Static Content Serving**: Serves HTML, images (JPG, PNG, GIF, MP4), and plain text files directly from a specified document root (`wwwroot`).
  * **Dynamic Content (CGI)**: Executes CGI (Common Gateway Interface) programs for dynamic content generation. Includes example CGI scripts (`hello.c` and `adder.c`) to demonstrate functionality like query string parsing and POST data handling.
  * **Error Handling**: Provides basic error responses (e.g., 400 Bad Request, 403 Forbidden, 404 Not Found, 500 Internal Server Error) and handles common network issues.

## Experimental

While `master` branch is iterative (handling one client request at a time), this project also explored various concurrency models in dedicated branches:

  * `concurrency-with-fork`: Process-based concurrency using `fork()`.
  * `concurrency-with-threads`: Thread-based concurrency using `pthreads`.
  * `concurrency-with-select`: Concurrency using I/O multiplexing with `select()`.
  * `pre-threading`: Pre-threaded (thread pool) model with a simple auto-scaling heuristic.

## Project Structure

  * **`tiny.c`**: The main server implementation. It accepts client connections, parses HTTP requests, and dispatches to appropriate handlers for static or dynamic content.
  * **`mynetlib.c` / `mynetlib.h`**: A custom utility library providing wrappers around socket functions (`clientConnect`, `serverListen`, `sendBytes`, `receiveBytes`) and a buffered reader (`buffered_reader_t`, `bufReadLine`, `bufReadBytes`) for efficient network I/O.
  * **`cgi/hello.c`** and **`cgi/adder.c`**: Simple example CGI programs.
  * **`Makefile`**: Used to compile the server and the CGI programs.
  * **`wwwroot/`**: (Created by Makefile) The web document root directory for static files.
  * **`wwwroot/cgi-bin/`**: (Created by Makefile) The directory where compiled CGI executables are placed.

-----

## Requirements

  * Developed and tested on Linux
  * `make` utility
  * `gcc` C compiler

-----

## Building and Running

```
$ cd src
$ make clean
$ make
$ ./tiny 8080   # or another port number
```

## Usage Examples

Once the server is running, you can access it using a web browser or `curl`.

```
curl http://localhost:8000/
curl http://localhost:8000/cgi-bin/hello
curl "http://localhost:8000/cgi-bin/adder?a=100&b=200"
curl -X POST -H "Content-Type: application/x-www-form-urlencoded" -d "a=50&b=75" http://localhost:8000/cgi-bin/adder
```

## Limitations

This project is an educational effort and, as such, has several intentional simplifications and limitations!!!

  * Poor security
  * Lacks robustness
  * Inefficient large file serving
  * Poor CGI input parsing
  * No Persistent Connections
  * No HTTPS/SSL/TLS
  * No Advanced Features
  * Etc.

## Other projects

For other HTTP server implementations with a different set of features, please check:

- [Build Your Own HTTP Server in C#](https://github.com/feliposz/codecrafters-http-server-csharp)
- [Build Your Own HTTP Server in Javascript](https://github.com/feliposz/codecrafters-http-server-javascript)
- [Build Your Own HTTP Server in Go](https://github.com/feliposz/codecrafters-http-server-go)