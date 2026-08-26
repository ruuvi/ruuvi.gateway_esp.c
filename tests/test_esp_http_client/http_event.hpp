/**
 * @file http_event.hpp
 * @author TheSomeMan
 * @date 2026-04-11
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef ESP_TESTS_HTTP_EVENT_HPP
#define ESP_TESTS_HTTP_EVENT_HPP

#include <string>
#include <vector>
#include <ostream>
#include "esp_http_client.h"

using namespace std;

class HttpEvent
{
    esp_http_client_event_id_t m_event_id;

public:
    explicit HttpEvent(const esp_http_client_event_id_t event_id)
        : m_event_id(event_id)
    {
    }

    virtual ~HttpEvent() = default;

    esp_http_client_event_id_t
    getEventId() const
    {
        return m_event_id;
    }

    bool
    operator==(const HttpEvent& other) const
    {
        return m_event_id == other.m_event_id;
    }

    bool
    operator!=(const HttpEvent& other) const
    {
        return !(*this == other);
    }
};

class HttpEventError : public HttpEvent
{
public:
    static constexpr esp_http_client_event_id_t EVENT_ID = HTTP_EVENT_ERROR;
    HttpEventError()
        : HttpEvent(EVENT_ID)
    {
    }
};

inline void
PrintTo(const HttpEventError&, std::ostream* os)
{
    *os << "HttpEventError{}";
}

class HttpEventOnConnected : public HttpEvent
{
public:
    static constexpr esp_http_client_event_id_t EVENT_ID = HTTP_EVENT_ON_CONNECTED;
    HttpEventOnConnected()
        : HttpEvent(EVENT_ID)
    {
    }
};

inline void
PrintTo(const HttpEventOnConnected&, std::ostream* os)
{
    *os << "HttpEventOnConnected{}";
}

class HttpEventHeadersSent : public HttpEvent
{
public:
    static constexpr esp_http_client_event_id_t EVENT_ID = HTTP_EVENT_HEADERS_SENT;
    HttpEventHeadersSent()
        : HttpEvent(EVENT_ID)
    {
    }
};

inline void
PrintTo(const HttpEventHeadersSent&, std::ostream* os)
{
    *os << "HttpEventHeadersSent{}";
}

class HttpEventOnHeader : public HttpEvent
{
public:
    static constexpr esp_http_client_event_id_t EVENT_ID = HTTP_EVENT_ON_HEADER;
    string                                      header_key;
    string                                      header_value;

    HttpEventOnHeader(const string& header_key_, const string& header_value_)
        : HttpEvent(EVENT_ID)
        , header_key(header_key_)
        , header_value(header_value_)
    {
    }

    bool
    operator==(const HttpEventOnHeader& other) const
    {
        return HttpEvent::operator==(other) && header_key == other.header_key && header_value == other.header_value;
    }
};

inline void
PrintTo(const HttpEventOnHeader& e, std::ostream* os)
{
    *os << "HttpEventOnHeader{key=\"" << e.header_key << "\", value=\"" << e.header_value << "\"}";
}

class HttpEventOnData : public HttpEvent
{
public:
    static constexpr esp_http_client_event_id_t EVENT_ID = HTTP_EVENT_ON_DATA;
    vector<uint8_t>                             m_data;
    HttpEventOnData(const void* data, const size_t data_len)
        : HttpEvent(EVENT_ID)
    {
        this->m_data.assign(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + data_len);
    }

    bool
    operator==(const HttpEventOnData& other) const
    {
        return HttpEvent::operator==(other) && m_data == other.m_data;
    }
};

inline void
PrintTo(const HttpEventOnData& e, std::ostream* os)
{
    *os << "HttpEventOnData{data=[";
    for (size_t i = 0; i < e.m_data.size(); ++i)
    {
        if (i > 0)
            *os << ", ";
        *os << static_cast<int>(e.m_data[i]);
    }
    *os << "]}";
}

class HttpEventOnFinish : public HttpEvent
{
public:
    static constexpr esp_http_client_event_id_t EVENT_ID = HTTP_EVENT_ON_FINISH;
    HttpEventOnFinish()
        : HttpEvent(EVENT_ID)
    {
    }
};

inline void
PrintTo(const HttpEventOnFinish&, std::ostream* os)
{
    *os << "HttpEventOnFinish{}";
}

class HttpEventDisconnected : public HttpEvent
{
public:
    static constexpr esp_http_client_event_id_t EVENT_ID = HTTP_EVENT_DISCONNECTED;
    HttpEventDisconnected()
        : HttpEvent(EVENT_ID)
    {
    }
};

inline void
PrintTo(const HttpEventDisconnected&, std::ostream* os)
{
    *os << "HttpEventDisconnected{}";
}

#endif // ESP_TESTS_HTTP_EVENT_HPP
