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

        ~Crypto_context();

        Crypto_context(const Crypto_context&) = delete;
        Crypto_context& operator=(const Crypto_context&) = delete;
        
        // корутина, которая сначала создает собственные private key и public key,
        // и возобновляется при set_peer_public_key
        // (после установки чужого ключа можно шифровать и расшифровывать данные)
        boost::system::error_code init();

        // шифрует передаваемые данные и возращает unique_ptr на массив с зашифрованными данными
        std::expected<std::vector<char>, boost::system::error_code>
        encrypt_data(const char* data, std::size_t size);

        // расшифровывает передаваемые данные и возращает unique_ptr на массив с расшифрованными данными
        std::expected<std::vector<char>, boost::system::error_code>
        ecrypt_data(const char* data, std::size_t size);

        // возращает собственный public key
        std::span<const char> get_own_public_key();

        // устанавливает public key peer'а
        boost::system::error_code set_peer_public_key(const char* peer_public_key, std::size_t size);
    
    private:
        bool is_ready_ = false;

        // указатель на структуру из openssl, которая хранит ключи
        EVP_PKEY* key_handle_ = nullptr;

        std::array<char, KEY_LEN> encryption_key_;
};