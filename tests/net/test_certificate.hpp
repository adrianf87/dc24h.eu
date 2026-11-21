#pragma once

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <stdexcept>

namespace dc24h::net::test {

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        auto name = (std::filesystem::temp_directory_path() / "dc24h-tls-XXXXXX").string();
        if (::mkdtemp(name.data()) == nullptr) {
            throw std::runtime_error("Could not create the TLS test directory");
        }
        path = name;
    }

    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }

    TemporaryDirectory(const TemporaryDirectory &) = delete;
    TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;

    std::filesystem::path path;
};

class TestCertificate {
  public:
    TestCertificate() {
        const std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
            EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", "prime256v1"), &EVP_PKEY_free);
        const std::unique_ptr<X509, decltype(&X509_free)> certificate(X509_new(), &X509_free);
        if (!key || !certificate) {
            throw std::runtime_error("Could not generate a TLS test key pair");
        }
        auto *subject = X509_get_subject_name(certificate.get());
        constexpr unsigned char common_name[] = "localhost";
        if (X509_set_version(certificate.get(), 2) != 1 ||
            ASN1_INTEGER_set(X509_get_serialNumber(certificate.get()), 1) != 1 ||
            X509_gmtime_adj(X509_getm_notBefore(certificate.get()), -60) == nullptr ||
            X509_gmtime_adj(X509_getm_notAfter(certificate.get()), 86400) == nullptr ||
            X509_set_pubkey(certificate.get(), key.get()) != 1 ||
            X509_NAME_add_entry_by_txt(subject, "CN", MBSTRING_ASC, common_name, -1, -1, 0) != 1 ||
            X509_set_issuer_name(certificate.get(), subject) != 1) {
            throw std::runtime_error("Could not initialize the TLS test certificate");
        }

        X509V3_CTX context{};
        X509V3_set_ctx(&context, certificate.get(), certificate.get(), nullptr, nullptr, 0);
        add_extension(*certificate, context, NID_subject_alt_name, "DNS:localhost,IP:127.0.0.1");
        add_extension(*certificate, context, NID_basic_constraints, "critical,CA:TRUE");
        if (X509_sign(certificate.get(), key.get(), EVP_sha256()) <= 0) {
            throw std::runtime_error("Could not sign the TLS test certificate");
        }

        const auto close_file = [](std::FILE *file) { std::fclose(file); };
        const std::unique_ptr<std::FILE, decltype(close_file)> cert_file(
            std::fopen(cert_path.c_str(), "wb"), close_file);
        const std::unique_ptr<std::FILE, decltype(close_file)> key_file(
            std::fopen(key_path.c_str(), "wb"), close_file);
        if (!cert_file || !key_file || PEM_write_X509(cert_file.get(), certificate.get()) != 1 ||
            PEM_write_PrivateKey(key_file.get(), key.get(), nullptr, nullptr, 0, nullptr,
                                 nullptr) != 1) {
            throw std::runtime_error("Could not write TLS test files");
        }
    }

  private:
    static void add_extension(X509 &certificate, X509V3_CTX &context, int id, const char *value) {
        const std::unique_ptr<X509_EXTENSION, decltype(&X509_EXTENSION_free)> extension(
            X509V3_EXT_nconf_nid(nullptr, &context, id, value), &X509_EXTENSION_free);
        if (!extension || X509_add_ext(&certificate, extension.get(), -1) != 1) {
            throw std::runtime_error("Could not add an extension to the TLS test certificate");
        }
    }

    TemporaryDirectory directory_;

  public:
    const std::filesystem::path cert_path = directory_.path / "server.pem";
    const std::filesystem::path key_path = directory_.path / "server-key.pem";
};

} // namespace dc24h::net::test
