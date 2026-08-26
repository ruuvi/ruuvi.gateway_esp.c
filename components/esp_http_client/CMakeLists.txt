idf_component_register(SRCS "esp_http_client.c"
                            "lib/http_auth.c"
                            "lib/http_header.c"
                            "lib/http_stream.c"
                            "lib/http_utils.c"
                    INCLUDE_DIRS "include"
                    PRIV_INCLUDE_DIRS "lib/include"
                    REQUIRES nghttp tcp_transport
                    PRIV_REQUIRES lwip esp-tls ruuvi.esp_wrappers.c)
