set(ORIG $ENV{IDF_PATH}/components/wpa_supplicant)

set(srcs "${ORIG}/port/os_xtensa.c"
    "${ORIG}/src/ap/ap_config.c"
    "${ORIG}/src/ap/ieee802_1x.c"
    "${ORIG}/src/ap/wpa_auth.c"
    "${ORIG}/src/ap/wpa_auth_ie.c"
    "${ORIG}/src/common/sae.c"
    "${ORIG}/src/common/wpa_common.c"
    "${ORIG}/src/common/dpp.c"
    "src/crypto/aes-ctr.c"
    "src/crypto/aes-siv.c"
    "src/crypto/sha256-kdf.c"
    "src/crypto/aes-cbc.c"
    "src/crypto/aes-ccm.c"
    "src/crypto/aes-gcm.c"
    "src/crypto/aes-internal-dec.c"
    "src/crypto/aes-internal-enc.c"
    "src/crypto/aes-internal.c"
    "src/crypto/aes-omac1.c"
    "src/crypto/aes-unwrap.c"
    "src/crypto/aes-wrap.c"
    "src/crypto/aes-omac1.c"
    "src/crypto/ccmp.c"
    "esp_supplicant/src/crypto/crypto_mbedtls-bignum.c"
    "esp_supplicant/src/crypto/crypto_mbedtls-ec.c"
    "src/crypto/crypto_ops.c"
    "src/crypto/crypto_internal-cipher.c"
    "src/crypto/crypto_internal-modexp.c"
    "src/crypto/crypto_internal-rsa.c"
    "esp_supplicant/src/crypto/crypto_mbedtls-rsa.c"
    "src/crypto/crypto_internal.c"
    "src/crypto/des-internal.c"
    "src/crypto/dh_group5.c"
    "src/crypto/dh_groups.c"
    "src/crypto/md4-internal.c"
    "src/crypto/md5-internal.c"
    "src/crypto/md5.c"
    "src/crypto/ms_funcs.c"
    "src/crypto/rc4.c"
    "src/crypto/sha1-internal.c"
    "src/crypto/sha1-pbkdf2.c"
    "src/crypto/sha1.c"
    "src/crypto/sha256-internal.c"
    "src/crypto/sha256.c"
    "src/crypto/sha1-tlsprf.c"
    "src/crypto/sha256-tlsprf.c"
    "src/crypto/sha384-tlsprf.c"
    "src/crypto/sha256-prf.c"
    "src/crypto/sha1-prf.c"
    "src/crypto/sha384-prf.c"
    "src/crypto/md4-internal.c"
    "src/crypto/sha1-tprf.c"
    "src/crypto/tls_internal.c"
    "${ORIG}/src/eap_peer/chap.c"
    "${ORIG}/src/eap_peer/eap.c"
    "${ORIG}/src/eap_peer/eap_common.c"
    "${ORIG}/src/eap_peer/eap_mschapv2.c"
    "${ORIG}/src/eap_peer/eap_peap.c"
    "${ORIG}/src/eap_peer/eap_peap_common.c"
    "${ORIG}/src/eap_peer/eap_tls.c"
    "${ORIG}/src/eap_peer/eap_tls_common.c"
    "${ORIG}/src/eap_peer/eap_ttls.c"
    "${ORIG}/src/eap_peer/mschapv2.c"
    "${ORIG}/src/esp_supplicant/esp_hostap.c"
    "${ORIG}/src/esp_supplicant/esp_wpa2.c"
    "${ORIG}/src/esp_supplicant/esp_wpa_main.c"
    "${ORIG}/src/esp_supplicant/esp_wpas_glue.c"
    "${ORIG}/src/esp_supplicant/esp_wps.c"
    "${ORIG}/src/esp_supplicant/esp_wpa3.c"
    "${ORIG}/src/eap_peer/eap_fast.c"
    "${ORIG}/src/eap_peer/eap_fast_common.c"
    "${ORIG}/src/eap_peer/eap_fast_pac.c"
    "${ORIG}/src/rsn_supp/pmksa_cache.c"
    "${ORIG}/src/rsn_supp/wpa.c"
    "${ORIG}/src/rsn_supp/wpa_ie.c"
    "${ORIG}/src/utils/base64.c"
    "src/utils/common.c"
    "${ORIG}/src/utils/ext_password.c"
    "${ORIG}/src/utils/uuid.c"
    "${ORIG}/src/utils/wpabuf.c"
    "${ORIG}/src/utils/wpa_debug.c"
    "${ORIG}/src/utils/json.c"
    "${ORIG}/src/wps/wps.c"
    "${ORIG}/src/wps/wps_attr_build.c"
    "${ORIG}/src/wps/wps_attr_parse.c"
    "${ORIG}/src/wps/wps_attr_process.c"
    "${ORIG}/src/wps/wps_common.c"
    "${ORIG}/src/wps/wps_dev_attr.c"
    "${ORIG}/src/wps/wps_enrollee.c"
    "${ORIG}/src/wps/wps_registrar.c"
    "${ORIG}/src/wps/wps_validate.c")

#if(CONFIG_WPA_MBEDTLS_CRYPTO)
#    set(tls_src "esp_supplicant/src/crypto/tls_mbedtls.c")
#else()
    set(tls_src
    "src/tls/asn1.c"
    "src/tls/bignum.c"
    "src/tls/pkcs1.c"
    "src/tls/pkcs5.c"
    "src/tls/pkcs8.c"
    "src/tls/rsa.c"
    "src/tls/tlsv1_client.c"
    "src/tls/tlsv1_client_ocsp.c"
    "src/tls/tlsv1_client_read.c"
    "src/tls/tlsv1_client_write.c"
    "src/tls/tlsv1_common.c"
    "src/tls/tlsv1_cred.c"
    "src/tls/tlsv1_record.c"
    "src/tls/tlsv1_server.c"
    "src/tls/tlsv1_server_read.c"
    "src/tls/tlsv1_server_write.c"
    "src/tls/x509v3.c"
    )
#endif()

idf_component_register(SRCS "${srcs}" "${tls_src}"
                    INCLUDE_DIRS ${ORIG}/include ${ORIG}/port/include ${ORIG}/include/esp_supplicant
                    PRIV_INCLUDE_DIRS src ${ORIG}/src src/crypto src/utils ${ORIG}/src/utils
                    PRIV_REQUIRES mbedtls esp_timer)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-strict-aliasing -Wno-write-strings)
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    __ets__
    ESP_SUPPLICANT
    IEEE8021X_EAPOL
    EAP_PEER_METHOD
    EAP_MSCHAPv2
    EAP_TTLS
    EAP_TLS
    EAP_PEAP
    EAP_FAST
    USE_WPA2_TASK
    CONFIG_WPS2
    CONFIG_WPS_PIN
    USE_WPS_TASK
    ESPRESSIF_USE
    ESP32_WORKAROUND
    CONFIG_ECC
    CONFIG_IEEE80211W
    CONFIG_SHA256
    CONFIG_DPP
    )

if(CONFIG_ESP32_WIFI_ENABLE_WPA3_SAE)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE CONFIG_WPA3_SAE)
endif()
if(CONFIG_WPA_WPS_STRICT)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE CONFIG_WPS_STRICT)
endif()
if(CONFIG_ESP_WIFI_MBEDTLS_CRYPTO)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE CONFIG_CRYPTO_MBEDTLS)
else()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE CONFIG_CRYPTO_INTERNAL)
endif()

set_property(TARGET ${COMPONENT_LIB} APPEND PROPERTY LINK_INTERFACE_MULTIPLICITY 3)
