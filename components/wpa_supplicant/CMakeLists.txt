set(srcs "port/os_xtensa.c"
    "src/ap/ap_config.c"
    "src/ap/ieee802_1x.c"
    "src/ap/wpa_auth.c"
    "src/ap/wpa_auth_ie.c"
    "src/common/sae.c"
    "src/common/wpa_common.c"
    "src/common/dpp.c"
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
#    "esp_supplicant/src/crypto/crypto_mbedtls.c"
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
    "src/eap_peer/chap.c"
    "src/eap_peer/eap.c"
    "src/eap_peer/eap_common.c"
    "src/eap_peer/eap_mschapv2.c"
    "src/eap_peer/eap_peap.c"
    "src/eap_peer/eap_peap_common.c"
    "src/eap_peer/eap_tls.c"
    "src/eap_peer/eap_tls_common.c"
    "src/eap_peer/eap_ttls.c"
    "src/eap_peer/mschapv2.c"
    "src/esp_supplicant/esp_hostap.c"
    "src/esp_supplicant/esp_wpa2.c"
    "src/esp_supplicant/esp_wpa_main.c"
    "src/esp_supplicant/esp_wpas_glue.c"
    "src/esp_supplicant/esp_wps.c"
    "src/esp_supplicant/esp_wpa3.c"
    "src/rsn_supp/pmksa_cache.c"
    "src/rsn_supp/wpa.c"
    "src/rsn_supp/wpa_ie.c"
    "src/utils/base64.c"
    "src/utils/common.c"
    "src/utils/ext_password.c"
    "src/utils/uuid.c"
    "src/utils/wpabuf.c"
    "src/utils/wpa_debug.c"
    "src/utils/json.c"
    "src/wps/wps.c"
    "src/wps/wps_attr_build.c"
    "src/wps/wps_attr_parse.c"
    "src/wps/wps_attr_process.c"
    "src/wps/wps_common.c"
    "src/wps/wps_dev_attr.c"
    "src/wps/wps_enrollee.c"
    "src/wps/wps_registrar.c"
    "src/wps/wps_validate.c")

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
                    INCLUDE_DIRS include port/include include/esp_supplicant
                    PRIV_INCLUDE_DIRS src src/utils esp_supplicant/src src/crypto
                    PRIV_REQUIRES mbedtls esp_timer)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-strict-aliasing)
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    __ets__
    ESP_SUPPLICANT
    IEEE8021X_EAPOL
    EAP_PEER_METHOD
    EAP_MSCHAPv2
    EAP_TTLS
    EAP_TLS
    EAP_PEAP
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
if(CONFIG_ESP_WIFI_MBEDTLS_CRYPTO)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE CONFIG_CRYPTO_MBEDTLS)
else()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE CONFIG_CRYPTO_INTERNAL)
endif()

set_property(TARGET ${COMPONENT_LIB} APPEND PROPERTY LINK_INTERFACE_MULTIPLICITY 3)
