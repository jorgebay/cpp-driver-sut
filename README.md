# C/C++ Driver SUT

## Web server configuration

This SUT requires a web server that support FastCGI.

### Nginx configuration

```
server {
  listen <ip_address>:8080;

  location ~ /.* {
    fastcgi_pass unix: <path_to_unix_sock_file>;
    fastcgi_keep_conn on;
    include fastcgi_params;
  }
}
```

## To build

Requires the cassandra-cpp-driver-dev and libuv-dev packages.

```
make
```

## To run

```bash
./sut <contact_points>  <path_to_unix_sock_file>
```
