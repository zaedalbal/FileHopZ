#pragma once
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/kdf.h>
#include <boost/asio.hpp>
#include <expected>
#include <span>

constexpr int KEY_LEN   = 32; // chacha20-poly1305 ключ
constexpr int NONCE_LEN = 12; // 96-bit nonce
constexpr int TAG_LEN   = 16; // poly1305 тег аутентификации
constexpr int X25519_LEN = 32; // публичный / приватный ключ

class Crypto_context
{
    public:
        Crypto_context() = default;

        ~Crypto_context() = default;

        Crypto_context(const Crypto_context&) = delete;
        Crypto_context& operator=(const Crypto_context&) = delete;
        
        // создает public key и private key
        boost::system::error_code init();

        // шифрует передаваемые данные и возращает vector с зашифрованными данными
        std::expected<std::vector<std::byte>, boost::system::error_code>
        encrypt_data(std::span<const std::byte> data);

        // расшифровывает передаваемые данные и возращает vector с расшифрованными данными
        std::expected<std::vector<std::byte>, boost::system::error_code>
        decrypt_data(std::span<const std::byte> data);

        // возращает собственный public key
        std::span<const std::byte, X25519_LEN> get_own_public_key();

        // устанавливает public key peer'а
        // только после установки можно вызывать encrypt_data и decrypt_data
        boost::system::error_code set_peer_public_key(std::span<const std::byte, X25519_LEN> peer_public_key);
    
    private:
        struct EVP_PKEY_DELETER
        {
            void operator()(EVP_PKEY* pkey) noexcept
            {
                EVP_PKEY_free(pkey);
            }
        };

        struct EVP_PKEY_CTX_DELETER
        {
            void operator()(EVP_PKEY_CTX* pkey_ctx) noexcept
            {
                EVP_PKEY_CTX_free(pkey_ctx);
            }
        };

        struct EVP_CIPHER_CTX_DELETER
        {
            void operator()(EVP_CIPHER_CTX* cipher_ctx) noexcept
            {
                EVP_CIPHER_CTX_free(cipher_ctx);
            }
        };

    private:
        bool is_ready_ = false;

        // указатель на структуру из openssl, которая хранит ключи
        std::unique_ptr<EVP_PKEY, EVP_PKEY_DELETER> key_handle_ = nullptr;

        std::array<std::byte, X25519_LEN> own_public_key_;

        std::array<std::byte, X25519_LEN> peer_public_key_;

        std::array<std::byte, KEY_LEN> encryption_key_;
};