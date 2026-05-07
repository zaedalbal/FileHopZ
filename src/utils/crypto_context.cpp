#include <utils/crypto_context.hpp>

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
    if(EVP_PKEY_keygen_init(generation_context) != 1)
    {
        EVP_PKEY_CTX_free(generation_context);
    // в будущем поменять возращаемую ошибку
        return boost::system::errc::make_error_code(boost::system::errc::bad_message);
    }
    if(EVP_PKEY_keygen(generation_context, &key_handle_) != 1)
    {
        EVP_PKEY_CTX_free(generation_context);
    // в будущем поменять возращаемую ошибку
        return boost::system::errc::make_error_code(boost::system::errc::bad_message);
    }

    EVP_PKEY_CTX_free(generation_context);

    std::size_t key_len = X25519_LEN;

    // установка own_public_key_
    if(EVP_PKEY_get_raw_public_key(
        key_handle_,
        reinterpret_cast<unsigned char*>(own_public_key_.data()),
        &key_len
    ) != 1)
        return boost::system::errc::make_error_code(boost::system::errc::bad_message);

    return {};
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
    if(!peer_pub_key)
    // в будущем поменять возращаемую ошибку
        return boost::system::errc::make_error_code(boost::system::errc::bad_message);

    auto derive_context = EVP_PKEY_CTX_new(key_handle_, nullptr);
    if(!derive_context)
    {
        EVP_PKEY_free(peer_pub_key);
    // в будущем поменять возращаемую ошибку
        return boost::system::errc::make_error_code(boost::system::errc::bad_message);
    }

    if(EVP_PKEY_derive_init(derive_context) != 1)
    {
        EVP_PKEY_CTX_free(derive_context);
        EVP_PKEY_free(peer_pub_key);
    // в будущем поменять возращаемую ошибку
        return boost::system::errc::make_error_code(boost::system::errc::bad_message);
    }
    if(EVP_PKEY_derive_set_peer(derive_context, peer_pub_key) != 1)
    {
        EVP_PKEY_CTX_free(derive_context);
        EVP_PKEY_free(peer_pub_key);
    // в будущем поменять возращаемую ошибку
        return boost::system::errc::make_error_code(boost::system::errc::bad_message);
    }

    EVP_PKEY_free(peer_pub_key);

    std::size_t secret_len = X25519_LEN;
    std::array<std::byte, X25519_LEN> shared_secret;
    
    if(EVP_PKEY_derive(
        derive_context,
        reinterpret_cast<unsigned char*>(shared_secret.data()),
        &secret_len
    ) != 1)
    {
        EVP_PKEY_CTX_free(derive_context);
        std::ranges::fill(shared_secret, std::byte{0});
    // в будущем поменять возращаемую ошибку
        return boost::system::errc::make_error_code(boost::system::errc::bad_message);
    }

    EVP_PKEY_CTX_free(derive_context);

    // оставшийся кусок метода: генерация ключа для ChaCha20

    auto hkdf_context = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);
    if(!hkdf_context)
    {
        std::ranges::fill(shared_secret, std::byte{0});
    // в будущем поменять возращаемую ошибку
        return boost::system::errc::make_error_code(boost::system::errc::bad_message);
    }

    if(EVP_PKEY_derive_init(hkdf_context) != 1)
    {
        EVP_PKEY_CTX_free(hkdf_context);
        std::ranges::fill(shared_secret, std::byte{0});
    // в будущем поменять возращаемую ошибку
        return boost::system::errc::make_error_code(boost::system::errc::bad_message);
    }

    if(EVP_PKEY_CTX_set_hkdf_md(hkdf_context, EVP_sha256()) != 1)
    {
        EVP_PKEY_CTX_free(hkdf_context);
        std::ranges::fill(shared_secret, std::byte{0});
    // в будущем поменять возращаемую ошибку
        return boost::system::errc::make_error_code(boost::system::errc::bad_message);
    }

    // соль = соеденение двух публичных ключей
    std::array<std::byte, X25519_LEN * 2> salt;
    auto own_is_less = std::lexicographical_compare(
        own_public_key_.begin(),
        own_public_key_.end(),
        peer_public_key_.begin(),
        peer_public_key_.end()
    );

    if(own_is_less)
    {
        std::memcpy(salt.data(), own_public_key_.data(), X25519_LEN);
        std::memcpy(salt.data() + X25519_LEN, peer_public_key_.data(), X25519_LEN);
    }
    else
    {
        std::memcpy(salt.data(), peer_public_key_.data(), X25519_LEN);
        std::memcpy(salt.data() + X25519_LEN, own_public_key_.data(), X25519_LEN);
    }

    if(EVP_PKEY_CTX_set1_hkdf_salt(
        hkdf_context,
        reinterpret_cast<const unsigned char*>(salt.data()),
        static_cast<int>(salt.size())
    ) != 1)
    {
        EVP_PKEY_CTX_free(hkdf_context);
        std::ranges::fill(shared_secret, std::byte{0});
    // в будущем поменять возращаемую ошибку
        return boost::system::errc::make_error_code(boost::system::errc::bad_message);
    }

    if(EVP_PKEY_CTX_set1_hkdf_key(
        hkdf_context,
        reinterpret_cast<const unsigned char*>(shared_secret.data()),
        static_cast<int>(shared_secret.size())
    ) != 1)
    {
        EVP_PKEY_CTX_free(hkdf_context);
        std::ranges::fill(shared_secret, std::byte{0});
    // в будущем поменять возращаемую ошибку
        return boost::system::errc::make_error_code(boost::system::errc::bad_message);
    }

    const unsigned char key_purpose_label[] = "x25519-chacha20poly1305";
    if(EVP_PKEY_CTX_add1_hkdf_info(
        hkdf_context,
        key_purpose_label,
        sizeof(key_purpose_label) - 1
    ) != 1)
    {
        EVP_PKEY_CTX_free(hkdf_context);
        std::ranges::fill(shared_secret, std::byte{0});
    // в будущем поменять возращаемую ошибку
        return boost::system::errc::make_error_code(boost::system::errc::bad_message);
    }

    std::size_t key_length = KEY_LEN;

    if(EVP_PKEY_derive(
        hkdf_context,
        reinterpret_cast<unsigned char*>(encryption_key_.data()),
        &key_length
    ) != 1)
    {
        EVP_PKEY_CTX_free(hkdf_context);
        std::ranges::fill(shared_secret, std::byte{0});
    // в будущем поменять возращаемую ошибку
        return boost::system::errc::make_error_code(boost::system::errc::bad_message);
    }

    EVP_PKEY_CTX_free(hkdf_context);
    std::ranges::fill(shared_secret, std::byte{0});

    return {};
}