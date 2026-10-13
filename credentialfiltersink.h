#ifndef CREDENTIALFILTERSINK_H
#define CREDENTIALFILTERSINK_H

#include "streamaddress.h"

#include <spdlog/details/log_msg.h>
#include <spdlog/sinks/base_sink.h>

#include <memory>
#include <mutex>
#include <string>
#include <string_view>

class CredentialFilterSink : public spdlog::sinks::base_sink<std::mutex> {
public:
    explicit CredentialFilterSink(spdlog::sink_ptr inner) : m_inner(std::move(inner)) {}

protected:
    void sink_it_(const spdlog::details::log_msg &message) override {
        const std::string_view payload(message.payload.data(), message.payload.size());
        if (payload.find("://") == std::string_view::npos || payload.find('@') == std::string_view::npos) {
            m_inner->log(message);
            return;
        }
        const std::string text = StreamAddress::withoutCredentials(QString::fromUtf8(payload.data(), qsizetype(payload.size()))).toStdString();
        spdlog::details::log_msg filtered = message;
        filtered.payload = text;
        m_inner->log(filtered);
    }

    void flush_() override { m_inner->flush(); }
    void set_pattern_(const std::string &pattern) override { m_inner->set_pattern(pattern); }
    void set_formatter_(std::unique_ptr<spdlog::formatter> formatter) override { m_inner->set_formatter(std::move(formatter)); }

private:
    spdlog::sink_ptr m_inner;
};

#endif // CREDENTIALFILTERSINK_H
