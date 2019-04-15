#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <iostream>
#include "frames.hpp"

using namespace std;

static unsigned char next_proto_list[256];
static size_t next_proto_list_len;

int create_socket(int port) {
    int s;
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        perror("Unable to create socket");
        exit(EXIT_FAILURE);
    }

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Unable to bind");
        exit(EXIT_FAILURE);
    }

    if (listen(s, 1) < 0) {
        perror("Unable to listen");
        exit(EXIT_FAILURE);
    }

    return s;
}

void init_openssl() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
}

void cleanup_openssl() {
    EVP_cleanup();
}

SSL_CTX *create_context() {
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    method = SSLv23_server_method();

    ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    return ctx;
}

static int next_proto_cb(SSL *s, const unsigned char **data,
                         unsigned int *len, void *arg) {
    *data = next_proto_list;
    *len = (unsigned int)next_proto_list_len;
    return SSL_TLSEXT_ERR_OK;
}


static int select_protocol(unsigned char **out, unsigned char *outlen, const unsigned char *in, unsigned int inlen) {
    unsigned int start_index = (unsigned int) in[0];
    unsigned int end_of_next_protocol = start_index ;

    unsigned int i = 0;
    cout << endl << "Client supports following protocols, ordered according to preference: " << endl;
    while (i < inlen) {
        while (i <= end_of_next_protocol && i < inlen) {
            cout << in[i];
            ++i;
        }
        cout << endl;
        end_of_next_protocol += in[i] + 1;
        ++i;

    }
    cout << endl;

    *outlen = in[0];
    out[0] = (unsigned char*) "h2";

    return 1;
}

static int alpn_select_proto_cb(SSL *ssl, const unsigned char **out,
                                unsigned char *outlen, const unsigned char *in,
                                unsigned int inlen, void *arg) {
    int rv;

    rv = select_protocol((unsigned char **)out, outlen, in, inlen);

    /*
    for(unsigned int i = 0; i < inlen; i++) {
        std::cout << i << ": " << in[i] << endl;
    }

    std::cout << "" << std::endl;

    cout << in[1] << endl;

    std::cout << *out << " " << (unsigned int)*outlen << std::endl;

    std::cout << rv << std::endl;
    */

    if (rv != 1) {
        return SSL_TLSEXT_ERR_NOACK;
    }

    cout << "Server has chosen 'h2', meaning HTTP/2 over TLS" << endl;

    return SSL_TLSEXT_ERR_OK;
}

void configure_alpn(SSL_CTX *ctx) {
    next_proto_list[0] = 2;
    memcpy(&next_proto_list[1], "h2", 2);
    next_proto_list_len = 1 + 2;

    SSL_CTX_set_next_protos_advertised_cb(ctx, next_proto_cb, NULL);

    SSL_CTX_set_alpn_select_cb(ctx, alpn_select_proto_cb, NULL);
}

void configure_context(SSL_CTX *ctx) {
    SSL_CTX_set_ecdh_auto(ctx, 1);

    /* Set the key and cert */
    if (SSL_CTX_use_certificate_file(ctx, "../cert.pem", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, "../key.pem", SSL_FILETYPE_PEM) <= 0 ) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    configure_alpn(ctx);
}

char *sslRead (SSL *ssl) {
    const int readSize = 64;
    char *rc = NULL;
    int received, count = 0;
    int TotalReceived = 0;
    fd_set fds;
    struct timeval timeout;
    char buffer[1024];

    if (ssl) {
        while (1) {
            received = SSL_read (ssl, buffer, readSize);
            if (received > 0) {
                TotalReceived += received;
                printf("Buffsize - %i - %.*s \n", received, received, buffer);
                for (int i = 0; i < received; i++) {
                    printf("%x", buffer[i]);
                }
                cout << "\n";
            }
            else {
                count++;

                //printf(" received equal to or less than 0\n")
                int err = SSL_get_error(ssl, received);
                switch (err) {
                    case SSL_ERROR_NONE: {
                        // no real error, just try again...
                        printf("SSL_ERROR_NONE %i\n", count);
                        continue;
                    }

                    case SSL_ERROR_ZERO_RETURN: {
                        // peer disconnected...
                        printf("SSL_ERROR_ZERO_RETURN %i\n", count);
                        break;
                    }

                    case SSL_ERROR_WANT_READ: {
                        // no data available right now, wait a few seconds in case new data arrives...
                        printf("SSL_ERROR_WANT_READ %i\n", count);

                        int sock = SSL_get_rfd(ssl);
                        FD_ZERO(&fds);
                        FD_SET(sock, &fds);

                        timeout.tv_sec = 5;
                        timeout.tv_usec = 0;

                        err = select(sock+1, &fds, NULL, NULL, &timeout);
                        if (err > 0)
                            continue; // more data to read...

                        if (err == 0) {
                            // timeout...
                        } else {
                            // error...
                        }

                        break;
                    }

                    case SSL_ERROR_WANT_WRITE: {
                        // socket not writable right now, wait a few seconds and try again...
                        printf("SSL_ERROR_WANT_WRITE %i\n", count);

                        int sock = SSL_get_wfd(ssl);
                        FD_ZERO(&fds);
                        FD_SET(sock, &fds);

                        timeout.tv_sec = 5;
                        timeout.tv_usec = 0;

                        err = select(sock+1, NULL, &fds, NULL, &timeout);
                        if (err > 0)
                            continue; // can write more data now...

                        if (err == 0) {
                            // timeout...
                        } else {
                            // error...
                        }

                        break;
                    }

                    default: {
                        printf("error %i:%i\n", received, err);
                        break;
                    }
                }

                break;
            }
        }
    }

    return rc;
}

int main(int argc, char **argv) {
    bool use_default_port = false;
    int port = use_default_port ? 443 : 8443;
    int sock;
    SSL_CTX *ctx;

    init_openssl();
    ctx = create_context();

    configure_context(ctx);

    sock = create_socket(port);

    /* Handle connections */
    while(1) {
        struct sockaddr_in addr;
        uint len = sizeof(addr);
        SSL *ssl;
        //const char reply[] = "test\n";

        int client = accept(sock, (struct sockaddr*)&addr, &len);
        if (client < 0) {
            perror("Unable to accept");
            exit(EXIT_FAILURE);
        }

        ssl = SSL_new(ctx);
        SSL_set_fd(ssl, client);

        if (SSL_accept(ssl) <= 0) {
            ERR_print_errors_fp(stderr);
        }
        else {
            SSL_write(ssl, settingsframe(0x0), 9);
            char* res = sslRead(ssl);
            SSL_write(ssl, settingsframe(0x1), 9);
        }

        SSL_free(ssl);
        close(client);
    }

    close(sock);
    SSL_CTX_free(ctx);
    cleanup_openssl();
}