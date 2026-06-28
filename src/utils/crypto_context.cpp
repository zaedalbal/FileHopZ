#include <utils/crypto_context.hpp>
#include "errors/filehopz_error.hpp"

// для сборки теста раскомментировать include ниже
/*#include "../../include/utils/crypto_context.hpp"
Crypto_context::~Crypto_context()
{}*/

boost::system::error_code Crypto_context::init()
{
    std::unique_ptr<EVP_PKEY_CTX, EVP_PKEY_CTX_DELETER> generation_context(
        EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, nullptr)
    );

    if(!generation_context)
        return filehopz::Error_code::crypto_key_generation_failed;

    // генерация ключей
    if(EVP_PKEY_keygen_init(generation_context.get()) != 1)
        return filehopz::Error_code::crypto_key_generation_failed;

    EVP_PKEY* raw_key{nullptr};

    if(EVP_PKEY_keygen(generation_context.get(), &raw_key) != 1)
        return filehopz::Error_code::crypto_key_generation_failed;

    key_handle_.reset(raw_key);

    std::size_t key_len = X25519_LEN;

    // установка own_public_key_
    if(EVP_PKEY_get_raw_public_key(
        key_handle_.get(),
        reinterpret_cast<unsigned char*>(own_public_key_.data()),
        &key_len
    ) != 1)
        return filehopz::Error_code::crypto_key_generation_failed;

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

    std::unique_ptr<EVP_PKEY, EVP_PKEY_DELETER> peer_pub_key(
        EVP_PKEY_new_raw_public_key(
            EVP_PKEY_X25519,
            nullptr,
            reinterpret_cast<const unsigned char*>(peer_public_key_.data()),
            X25519_LEN
        )
    );
    if(!peer_pub_key)
        return filehopz::Error_code::crypto_peer_key_failed;

    std::unique_ptr<EVP_PKEY_CTX, EVP_PKEY_CTX_DELETER> derive_context(
        EVP_PKEY_CTX_new(key_handle_.get(), nullptr)
    );
    if(!derive_context)
        return filehopz::Error_code::crypto_peer_key_failed;

    if(EVP_PKEY_derive_init(derive_context.get()) != 1)
        return filehopz::Error_code::crypto_peer_key_failed;

    if(EVP_PKEY_derive_set_peer(derive_context.get(), peer_pub_key.get()) != 1)
        return filehopz::Error_code::crypto_peer_key_failed;

    std::size_t secret_len = X25519_LEN;
    std::array<std::byte, X25519_LEN> shared_secret;
    
    if(EVP_PKEY_derive(
        derive_context.get(),
        reinterpret_cast<unsigned char*>(shared_secret.data()),
        &secret_len
    ) != 1)
    {
        std::ranges::fill(shared_secret, std::byte{0});
        return filehopz::Error_code::crypto_peer_key_failed;
    }

    // оставшийся кусок метода: генерация ключа для ChaCha20

    std::unique_ptr<EVP_PKEY_CTX, EVP_PKEY_CTX_DELETER> hkdf_context(
        EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr)
    );

    if(!hkdf_context)
    {
        std::ranges::fill(shared_secret, std::byte{0});
        return filehopz::Error_code::crypto_hkdf_failed;
    }

    if(EVP_PKEY_derive_init(hkdf_context.get()) != 1)
    {
        std::ranges::fill(shared_secret, std::byte{0});
        return filehopz::Error_code::crypto_hkdf_failed;
    }

    if(EVP_PKEY_CTX_set_hkdf_md(hkdf_context.get(), EVP_sha256()) != 1)
    {
        std::ranges::fill(shared_secret, std::byte{0});
        return filehopz::Error_code::crypto_hkdf_failed;
    }

    // соль = соединение двух публичных ключей
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
        hkdf_context.get(),
        reinterpret_cast<const unsigned char*>(salt.data()),
        static_cast<int>(salt.size())
    ) != 1)
    {
        std::ranges::fill(shared_secret, std::byte{0});
        return filehopz::Error_code::crypto_hkdf_failed;
    }

    if(EVP_PKEY_CTX_set1_hkdf_key(
        hkdf_context.get(),
        reinterpret_cast<const unsigned char*>(shared_secret.data()),
        static_cast<int>(shared_secret.size())
    ) != 1)
    {
        std::ranges::fill(shared_secret, std::byte{0});
        return filehopz::Error_code::crypto_hkdf_failed;
    }

    const unsigned char key_purpose_label[] = "x25519-chacha20poly1305";
    if(EVP_PKEY_CTX_add1_hkdf_info(
        hkdf_context.get(),
        key_purpose_label,
        sizeof(key_purpose_label) - 1
    ) != 1)
    {
        std::ranges::fill(shared_secret, std::byte{0});
        return filehopz::Error_code::crypto_hkdf_failed;
    }

    std::size_t key_length = KEY_LEN;

    if(EVP_PKEY_derive(
        hkdf_context.get(),
        reinterpret_cast<unsigned char*>(encryption_key_.data()),
        &key_length
    ) != 1)
    {
        std::ranges::fill(shared_secret, std::byte{0});
        return filehopz::Error_code::crypto_hkdf_failed;
    }

    std::ranges::fill(shared_secret, std::byte{0});

    is_ready_ = true;
    return {};
}


std::expected<std::vector<std::byte>, boost::system::error_code>
Crypto_context::encrypt_data(std::span<const std::byte> data)
{
    if(!is_ready_)
        return std::unexpected(
            filehopz::Error_code::crypto_context_not_ready
        );

    std::vector<std::byte> output_packet(NONCE_LEN + data.size_bytes() + TAG_LEN);

    // генерация NONCE
    if(RAND_bytes(
        reinterpret_cast<unsigned char*>(output_packet.data()),
        NONCE_LEN
    ) != 1)
        return std::unexpected(
            filehopz::Error_code::crypto_random_failed
        );

    std::unique_ptr<EVP_CIPHER_CTX, EVP_CIPHER_CTX_DELETER> cipher_context(EVP_CIPHER_CTX_new());
    if(!cipher_context)
        return std::unexpected(
            filehopz::Error_code::crypto_encrypt_failed
        );
    
    if(EVP_EncryptInit_ex(
        cipher_context.get(),
        EVP_chacha20_poly1305(),
        nullptr,
        nullptr,
        nullptr
    ) != 1)
        return std::unexpected(
            filehopz::Error_code::crypto_encrypt_failed
        );    

    // явное указание длины nonce
    if(EVP_CIPHER_CTX_ctrl(
        cipher_context.get(),
        EVP_CTRL_AEAD_SET_IVLEN,
        NONCE_LEN,
        nullptr
    ) != 1)
        return std::unexpected(
            filehopz::Error_code::crypto_encrypt_failed
        );
    
    if(EVP_EncryptInit_ex(
        cipher_context.get(),
        nullptr,
        nullptr,
        reinterpret_cast<const unsigned char*>(encryption_key_.data()),
        reinterpret_cast<const unsigned char*>(output_packet.data())
    ) != 1)
        return std::unexpected(
            filehopz::Error_code::crypto_encrypt_failed
        );

    int encrypted_bytes = 0;
    if(EVP_EncryptUpdate(
        cipher_context.get(),
        reinterpret_cast<unsigned char*>(output_packet.data() + NONCE_LEN),
        &encrypted_bytes,
        reinterpret_cast<const unsigned char*>(data.data()),
        static_cast<int>(data.size_bytes())
    ) != 1)
        return std::unexpected(
            filehopz::Error_code::crypto_encrypt_failed
        );
    
    int final_bytes = 0;
    if(EVP_EncryptFinal_ex(
        cipher_context.get(),
        nullptr,
        &final_bytes
    ) != 1)
        return std::unexpected(
            filehopz::Error_code::crypto_encrypt_failed
        );
    
    if(EVP_CIPHER_CTX_ctrl(
        cipher_context.get(),
        EVP_CTRL_AEAD_GET_TAG,
        TAG_LEN,
        output_packet.data() + NONCE_LEN + data.size_bytes()
    ) != 1)
        return std::unexpected(
            filehopz::Error_code::crypto_encrypt_failed
        );

    return output_packet;
}


std::expected<std::vector<std::byte>, boost::system::error_code>
Crypto_context::decrypt_data(std::span<const std::byte> data)
{
    if(!is_ready_)
        return std::unexpected(
            filehopz::Error_code::crypto_context_not_ready
        );
    
    if(data.size_bytes() <= NONCE_LEN + TAG_LEN)
        return std::unexpected(
            filehopz::Error_code::crypto_ciphertext_too_short
        );

    const auto nonce_ptr = data.data();
    const auto encrypted_data_ptr = data.data() + NONCE_LEN;
    const auto tag_ptr = data.data() + data.size_bytes() - TAG_LEN;
    const auto encrypted_data_size = data.size_bytes() - NONCE_LEN - TAG_LEN;

    std::unique_ptr<EVP_CIPHER_CTX, EVP_CIPHER_CTX_DELETER> cipher_context(
        EVP_CIPHER_CTX_new()
    );
    if(!cipher_context)
        return std::unexpected(
            filehopz::Error_code::crypto_decrypt_failed
        );
    
    if(EVP_DecryptInit_ex(
        cipher_context.get(),
        EVP_chacha20_poly1305(),
        nullptr,
        nullptr,
        nullptr
    ) != 1)
        return std::unexpected(
            filehopz::Error_code::crypto_decrypt_failed
        );
    
    if(EVP_CIPHER_CTX_ctrl(
        cipher_context.get(),
        EVP_CTRL_AEAD_SET_IVLEN,
        NONCE_LEN,
        nullptr
    ) != 1)
        return std::unexpected(
            filehopz::Error_code::crypto_decrypt_failed
        );
    
    if(EVP_DecryptInit_ex(
        cipher_context.get(),
        nullptr,
        nullptr,
        reinterpret_cast<const unsigned char*>(encryption_key_.data()),
        reinterpret_cast<const unsigned char*>(nonce_ptr)
    ) != 1)
        return std::unexpected(
            filehopz::Error_code::crypto_decrypt_failed
        );
    
    std::vector<std::byte> output_data(encrypted_data_size);
    int decrypted_bytes = 0;
    if(EVP_DecryptUpdate(
        cipher_context.get(),
        reinterpret_cast<unsigned char*>(output_data.data()),
        &decrypted_bytes,
        reinterpret_cast<const unsigned char*>(encrypted_data_ptr),
        static_cast<int>(encrypted_data_size)
    ) != 1)
        return std::unexpected(
            filehopz::Error_code::crypto_decrypt_failed
        );
    
    // установка ожидаемого тега; затем EVP_DecryptFinal_ex проверит его —
    // при несовпадении вернёт 0 (данные повреждены/подделаны либо неверный ключ)
    if(EVP_CIPHER_CTX_ctrl(
        cipher_context.get(),
        EVP_CTRL_AEAD_SET_TAG,
        TAG_LEN,
        const_cast<std::byte*>(tag_ptr)
    ) != 1)
        return std::unexpected(
            filehopz::Error_code::crypto_decrypt_failed
        );
    
    int final_bytes = 0;
    if(EVP_DecryptFinal_ex(
        cipher_context.get(),
        nullptr,
        &final_bytes
    ) != 1)
        return std::unexpected(
            filehopz::Error_code::crypto_decrypt_failed
        );
    
    return output_data;
}
