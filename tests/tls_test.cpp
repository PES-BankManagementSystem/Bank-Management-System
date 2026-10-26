// S-28 (#59): the server can run with TLS, using a certificate and key whose
// paths come from configuration (SRS 3.4, SEC-3, NFR-4).

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include <gtest/gtest.h>
#include <httplib.h>

#include "api/server.hpp"
#include "common/config.hpp"

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#endif

namespace {

namespace fs = std::filesystem;

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT

// A throwaway self-signed certificate and key, written to a temp folder.
class TestCertificate {
public:
    TestCertificate() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        dir_ = fs::temp_directory_path() / ("bms_tls_test_" + std::to_string(stamp));
        fs::create_directories(dir_);
        cert_ = (dir_ / "server.crt").string();
        key_ = (dir_ / "server.key").string();

        EVP_PKEY* pkey = EVP_PKEY_Q_keygen(nullptr, nullptr, "RSA", static_cast<size_t>(2048));
        if (pkey == nullptr) throw std::runtime_error("Could not generate a test key.");
        X509* x509 = X509_new();
        X509_set_version(x509, 2);
        ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);
        X509_gmtime_adj(X509_getm_notBefore(x509), 0);
        X509_gmtime_adj(X509_getm_notAfter(x509), 60 * 60);
        X509_set_pubkey(x509, pkey);
        X509_NAME* name = X509_get_subject_name(x509);
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                   reinterpret_cast<const unsigned char*>("localhost"), -1, -1, 0);
        X509_set_issuer_name(x509, name);
        X509_sign(x509, pkey, EVP_sha256());

        write(cert_, [&](FILE* f) { return PEM_write_X509(f, x509); });
        write(key_, [&](FILE* f) { return PEM_write_PrivateKey(f, pkey, nullptr, nullptr, 0, nullptr, nullptr); });
        X509_free(x509);
        EVP_PKEY_free(pkey);
    }

    ~TestCertificate() {
        std::error_code error;
        fs::remove_all(dir_, error);
    }

    const std::string& cert() const { return cert_; }
    const std::string& key() const { return key_; }

private:
    template <typename Writer>
    static void write(const std::string& path, Writer&& writer) {
        FILE* file = std::fopen(path.c_str(), "wb");
        if (file == nullptr) throw std::runtime_error("Could not write " + path);
        const int ok = writer(file);
        std::fclose(file);
        if (ok != 1) throw std::runtime_error("Could not write PEM data to " + path);
    }

    fs::path dir_;
    std::string cert_;
    std::string key_;
};

bms::Config tls_config(const TestCertificate& certificate) {
    bms::Config config;
    config.host = "127.0.0.1";
    config.port = 0;
    config.tls_cert_path = certificate.cert();
    config.tls_key_path = certificate.key();
    return config;
}

TEST(Tls, ServesHttpsWhenACertificateAndKeyAreConfigured) {
    ASSERT_TRUE(bms::api::Server::tls_supported());
    TestCertificate certificate;
    bms::api::Server server(tls_config(certificate));
    ASSERT_TRUE(server.tls_enabled());
    const int port = server.bind();
    std::thread thread([&server] { server.run(); });
    server.wait_until_ready();

    httplib::SSLClient https("127.0.0.1", port);
    https.enable_server_certificate_verification(false);  // the test certificate is self-signed
    https.set_connection_timeout(5);
    const auto response = https.Get("/health");
    ASSERT_TRUE(response) << httplib::to_string(response.error());
    EXPECT_EQ(response->status, 200);
    EXPECT_NE(response->body.find("\"ok\""), std::string::npos);

    // Plain HTTP to the TLS port is not answered with application data.
    httplib::Client plain("127.0.0.1", port);
    plain.set_connection_timeout(2);
    plain.set_read_timeout(2);
    const auto refused = plain.Get("/health");
    EXPECT_TRUE(!refused || refused->status != 200);

    server.stop();
    thread.join();
}

TEST(Tls, RefusesToStartWithAnUnreadableCertificateOrKey) {
    TestCertificate certificate;
    bms::Config config = tls_config(certificate);
    config.tls_cert_path = (fs::path(certificate.cert()).parent_path() / "missing.crt").string();
    EXPECT_THROW(bms::api::Server{config}, std::runtime_error);

    config = tls_config(certificate);
    config.tls_key_path = certificate.cert();  // a certificate is not a private key
    EXPECT_THROW(bms::api::Server{config}, std::runtime_error);
}

#else

TEST(Tls, ASeverBuiltWithoutOpenSslRefusesATlsConfiguration) {
    EXPECT_FALSE(bms::api::Server::tls_supported());
    bms::Config config;
    config.port = 0;
    config.tls_cert_path = "server.crt";
    config.tls_key_path = "server.key";
    EXPECT_THROW(bms::api::Server{config}, std::runtime_error);
}

#endif

TEST(Tls, WithoutACertificateTheServerSpeaksPlainHttp) {
    bms::Config config;
    config.host = "127.0.0.1";
    config.port = 0;
    bms::api::Server server(config);
    EXPECT_FALSE(server.tls_enabled());
    const int port = server.bind();
    std::thread thread([&server] { server.run(); });
    server.wait_until_ready();

    httplib::Client client("127.0.0.1", port);
    const auto response = client.Get("/health");
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 200);

    server.stop();
    thread.join();
}

}  // namespace
