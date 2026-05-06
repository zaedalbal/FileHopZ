#include <utils/crypto_context.hpp>

Crypto_context::Crypto_context()
{}

Crypto_context::~Crypto_context()
{
    EVP_PKEY_free(key_handle_);
}

boost::system::error_code Crypto_context::init()
{
    EVP_PKEY_CTX* generation_context = EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, nullptr);
    if(!generation_context)
    // в будущем поменять возращаемую ошибку
        return boost::system::errc::make_error_code(boost::system::errc::bad_message);

    // генерация ключей
    EVP_PKEY_keygen_init(generation_context);
    EVP_PKEY_keygen(generation_context, &key_handle_);

    EVP_PKEY_CTX_free(generation_context);

    std::size_t key_len = X25519_LEN;

    // установка own_public_key_
    EVP_PKEY_get_raw_public_key(
        key_handle_,
        reinterpret_cast<unsigned char*>(own_public_key_.data()),
        &key_len
    );
}

std::span<const std::byte, X25519_LEN> Crypto_context::get_own_public_key()
{
    return std::span(own_public_key_);
}

boost::system::error_code Crypto_context::set_peer_public_key
(std::span<const std::byte, X25519_LEN> peer_public_key)
{
    std::memcpy(peer_public_key_.data(), peer_public_key.data(), X25519_LEN);

    auto peer_pub_key = EVP_PKEY_new_raw_public_key(
        EVP_PKEY_X25519,
        nullptr,
        reinterpret_cast<const unsigned char*>(peer_public_key_.data()),
        X25519_LEN
    );

    auto derive_context = EVP_PKEY_CTX_new(key_handle_, nullptr);
    if(!derive_context)
    // в будущем поменять возращаемую ошибку
        return boost::system::errc::make_error_code(boost::system::errc::bad_message);

    EVP_PKEY_derive_init(derive_context);
    EVP_PKEY_derive_set_peer(derive_context, peer_pub_key);

    std::size_t secret_len = X25519_LEN;
    std::array<std::byte, X25519_LEN> shared_secret;
    
    EVP_PKEY_derive(
        derive_context,
        reinterpret_cast<unsigned char*>(shared_secret.data()),
        &secret_len
    );

    EVP_PKEY_CTX_free(derive_context);

    // оставшийся кусок метода: генерация ключа для ChaCha20
}