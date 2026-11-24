#include "dc24h/adcs/tls.hpp"

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <initializer_list>
#include <string>
#include <string_view>
#include <unistd.h>
#include <vector>

namespace {
void require(bool value, const char* reason) {
    if (!value) { throw std::runtime_error(reason); }
}

struct Workspace {
    std::string root;
    std::vector<std::string> files;
    Workspace() {
        char buffer[] = "/tmp/dc24h-095-tlsXXXXXX";
        if (mkdtemp(buffer) == nullptr) { throw std::runtime_error("Cannot create TLS test directory"); }
        root = buffer;
    }
    ~Workspace() {
        for (const auto& file : files) { std::remove(file.c_str()); }
        std::remove(root.c_str());
    }
    std::string file(const std::string& name) {
        files.push_back(root + "/" + name);
        return files.back();
    }
};

using Key = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using Certificate = std::unique_ptr<X509, decltype(&X509_free)>;

Key generate(int kind, int parameter) {
    std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> context(EVP_PKEY_CTX_new_id(kind, nullptr), &EVP_PKEY_CTX_free);
    require(context != nullptr && EVP_PKEY_keygen_init(context.get()) == 1, "Cannot prepare test key");
    if (kind == EVP_PKEY_RSA) {
        require(EVP_PKEY_CTX_set_rsa_keygen_bits(context.get(), parameter) == 1, "Cannot set RSA size");
    } else if (kind == EVP_PKEY_EC) {
        require(EVP_PKEY_CTX_set_ec_paramgen_curve_nid(context.get(), parameter) == 1, "Cannot set curve");
    }
    EVP_PKEY* raw = nullptr;
    require(EVP_PKEY_keygen(context.get(), &raw) == 1 && raw != nullptr, "Cannot generate test key");
    return {raw, &EVP_PKEY_free};
}

void add_extension(X509* certificate, int nid, const char* value) {
    X509V3_CTX context;
    X509V3_set_ctx_nodb(&context);
    X509V3_set_ctx(&context, certificate, certificate, nullptr, nullptr, 0);
    X509_EXTENSION* extension = X509V3_EXT_conf_nid(nullptr, &context, nid, value);
    const bool added = extension != nullptr && X509_add_ext(certificate, extension, -1) == 1;
    X509_EXTENSION_free(extension);
    require(added, "Cannot add test extension");
}

struct Material { std::string certificate; std::string key; };

Material issue(Workspace& workspace, const std::string& stem, EVP_PKEY* key, const EVP_MD* digest,
               long not_before, long not_after, const char* usage, const char* extended,
               const char* basic_constraints = nullptr) {
    Certificate certificate(X509_new(), &X509_free);
    require(certificate != nullptr, "Cannot allocate test certificate");
    require(X509_set_version(certificate.get(), 2L) == 1, "Cannot set certificate version");
    require(ASN1_INTEGER_set(X509_get_serialNumber(certificate.get()), 1L) == 1, "Cannot set serial");
    require(X509_gmtime_adj(X509_getm_notBefore(certificate.get()), not_before) != nullptr, "Cannot set notBefore");
    require(X509_gmtime_adj(X509_getm_notAfter(certificate.get()), not_after) != nullptr, "Cannot set notAfter");
    require(X509_set_pubkey(certificate.get(), key) == 1, "Cannot set public key");
    X509_NAME* name = X509_get_subject_name(certificate.get());
    require(X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                reinterpret_cast<const unsigned char*>("localhost"), -1, -1, 0) == 1,
            "Cannot set subject");
    require(X509_set_issuer_name(certificate.get(), name) == 1, "Cannot set issuer");
    if (usage != nullptr) { add_extension(certificate.get(), NID_key_usage, usage); }
    if (extended != nullptr) { add_extension(certificate.get(), NID_ext_key_usage, extended); }
    if (basic_constraints != nullptr) { add_extension(certificate.get(), NID_basic_constraints, basic_constraints); }
    require(X509_sign(certificate.get(), key, digest) != 0, "Cannot sign test certificate");
    Material material{workspace.file(stem + ".pem"), workspace.file(stem + ".key")};
    std::unique_ptr<BIO, decltype(&BIO_free)> certificate_file(BIO_new_file(material.certificate.c_str(), "w"), &BIO_free);
    std::unique_ptr<BIO, decltype(&BIO_free)> key_file(BIO_new_file(material.key.c_str(), "w"), &BIO_free);
    require(certificate_file != nullptr && key_file != nullptr &&
                PEM_write_bio_X509(certificate_file.get(), certificate.get()) == 1 &&
                PEM_write_bio_PrivateKey(key_file.get(), key, nullptr, nullptr, 0, nullptr, nullptr) == 1,
            "Cannot write test certificate");
    return material;
}

void accept(const Material& material) {
    const dc24h::adcs::Context context(material.certificate, material.key);
}

void reject(const Material& material, std::initializer_list<std::string_view> messages) {
    try {
        const dc24h::adcs::Context context(material.certificate, material.key);
        throw std::runtime_error("accepted a certificate the policy must refuse");
    } catch (const std::exception& error) {
        const std::string_view actual = error.what();
        for (const std::string_view message : messages) {
            if (actual == message) { return; }
        }
        throw std::runtime_error(std::string(actual) + " was not an expected refusal");
    }
}

void policy() {
    Workspace workspace;
    constexpr long day = 24L * 60L * 60L;
    constexpr const char* server_usage = "digitalSignature,keyEncipherment";
    const Key rsa = generate(EVP_PKEY_RSA, 2048);
    const Key small = generate(EVP_PKEY_RSA, 1024);
    const Key curve = generate(EVP_PKEY_EC, NID_X9_62_prime256v1);
    const Key bitcoin = generate(EVP_PKEY_EC, NID_secp256k1);
    const Key edwards = generate(EVP_PKEY_ED25519, 0);
    accept(issue(workspace, "server", rsa.get(), EVP_sha256(), 0, day, server_usage, "serverAuth"));
    accept(issue(workspace, "plain", rsa.get(), EVP_sha256(), 0, day, nullptr, nullptr));
    accept(issue(workspace, "p256", curve.get(), EVP_sha256(), 0, day, "digitalSignature", "serverAuth"));
    accept(issue(workspace, "ed25519", edwards.get(), nullptr, 0, day, "digitalSignature", "serverAuth"));
    accept(issue(workspace, "non-ca", rsa.get(), EVP_sha256(), 0, day, server_usage, "serverAuth", "critical,CA:FALSE"));
    reject(issue(workspace, "expired", rsa.get(), EVP_sha256(), -2L * day, -day, server_usage, "serverAuth"),
           {"TLS certificate is not valid at startup"});
    reject(issue(workspace, "future", rsa.get(), EVP_sha256(), 2L * day, 3L * day, server_usage, "serverAuth"),
           {"TLS certificate is not valid at startup"});
    reject(issue(workspace, "sha1", rsa.get(), EVP_sha1(), 0, day, server_usage, "serverAuth"),
           {"TLS certificate signature is too weak"});
    // OpenSSL security level 2 refuses this key while loading. Level 1 would load it,
    // and the hub policy still has to refuse it.
    reject(issue(workspace, "rsa1024", small.get(), EVP_sha256(), 0, day, server_usage, "serverAuth"),
           {"TLS certificate public key is too weak", "Cannot load TLS certificate chain"});
    reject(issue(workspace, "secp256k1", bitcoin.get(), EVP_sha256(), 0, day, "digitalSignature", "serverAuth"),
           {"TLS certificate public key is too weak"});
    reject(issue(workspace, "client", rsa.get(), EVP_sha256(), 0, day, server_usage, "clientAuth"),
           {"TLS certificate is not authorized for server authentication"});
    reject(issue(workspace, "ca-only", rsa.get(), EVP_sha256(), 0, day, "keyCertSign", "serverAuth"),
           {"TLS certificate key usage does not allow a hub listener"});
    reject(issue(workspace, "ca-with-server-usage", rsa.get(), EVP_sha256(), 0, day, server_usage,
                 "serverAuth", "critical,CA:TRUE"),
           {"TLS certificate is not a valid server leaf"});
    reject(issue(workspace, "encipherment-only", rsa.get(), EVP_sha256(), 0, day, "keyEncipherment", "serverAuth"),
           {"TLS certificate key usage does not allow a hub listener"});
    reject(issue(workspace, "agreement-only", curve.get(), EVP_sha256(), 0, day, "keyAgreement", "serverAuth"),
           {"TLS certificate key usage does not allow a hub listener"});
}
} // namespace

int main() {
    try {
        policy();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "PASS: hub non-CA leaf validity, ECDHE signing use, key strength and AEAD context\n";
    return 0;
}
