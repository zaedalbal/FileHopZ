#include <utils/crypto_context.hpp>
#include "errors/filehopz_error.hpp"
#include <algorithm>
#include <cstring>
#include <limits>

// для сборки теста раскомментировать include ниже
/*#include "../../include/utils/crypto_context.hpp"
Crypto_context::~Crypto_context()
{}*/

boost::system::error_code Crypto_context::init()
{
    // Контекст OpenSSL для генерации X25519 key pair
    std::unique_ptr<EVP_PKEY_CTX, EVP_PKEY_CTX_DELETER> generation_context(
        EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, nullptr)
    );

    if(!generation_context)
        return filehopz::Error_code::crypto_key_generation_failed;

    // Подготовка keygen перед фактической генерацией ключевой пары
    if(EVP_PKEY_keygen_init(generation_context.get()) != 1)
        return filehopz::Error_code::crypto_key_generation_failed;

    EVP_PKEY* raw_key{nullptr};

    // Передача владения от OpenSSL через raw pointer с немедленным переносом в unique_ptr
    if(EVP_PKEY_keygen(generation_context.get(), &raw_key) != 1)
        return filehopz::Error_code::crypto_key_generation_failed;

    key_handle_.reset(raw_key);

    std::size_t key_len = X25519_LEN;

    // Получение public key наружу с сохранением private key внутри OpenSSL
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
    // Сохранение public key peer'а для создания salt и направления ключей
    std::memcpy(peer_public_key_.data(), peer_public_key.data(), X25519_LEN);

    // Превращение raw X25519 public key peer'а в OpenSSL объект для EVP_PKEY_derive
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

    // Привязка derive context к private key из key_handle_
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
    
    // Получение X25519 shared secret, одинакового на обеих сторонах соединения
    if(EVP_PKEY_derive(
        derive_context.get(),
        reinterpret_cast<unsigned char*>(shared_secret.data()),
        &secret_len
    ) != 1)
    {
        std::ranges::fill(shared_secret, std::byte{0});
        return filehopz::Error_code::crypto_peer_key_failed;
    }

    // Превращение X25519 secret через HKDF в ключевой материал
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

    // Сортировка public keys для получения одинакового HKDF salt на обеих сторонах
    std::array<std::byte, X25519_LEN * 2> salt;
    auto own_is_less = std::lexicographical_compare(
        own_public_key_.begin(),
        own_public_key_.end(),
        peer_public_key_.begin(),
        peer_public_key_.end()
    );
    // Запрет одинаковых public keys из-за неоднозначного направления send/recv
    if(std::ranges::equal(own_public_key_, peer_public_key_))
    {
        std::ranges::fill(shared_secret, std::byte{0});
        return filehopz::Error_code::crypto_peer_key_failed;
    }

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

    // Привязка ключей сессии к конкретной паре публичных ключей через salt
    if(EVP_PKEY_CTX_set1_hkdf_salt(
        hkdf_context.get(),
        reinterpret_cast<const unsigned char*>(salt.data()),
        static_cast<int>(salt.size())
    ) != 1)
    {
        std::ranges::fill(shared_secret, std::byte{0});
        return filehopz::Error_code::crypto_hkdf_failed;
    }

    // Использование X25519 shared secret как input key material для HKDF
    if(EVP_PKEY_CTX_set1_hkdf_key(
        hkdf_context.get(),
        reinterpret_cast<const unsigned char*>(shared_secret.data()),
        static_cast<int>(shared_secret.size())
    ) != 1)
    {
        std::ranges::fill(shared_secret, std::byte{0});
        return filehopz::Error_code::crypto_hkdf_failed;
    }

    // Отделение этих ключей от будущих ключей с другим назначением через info
    const unsigned char key_purpose_label[] = "x25519-chacha20poly1305-directional-keys-v1";
    if(EVP_PKEY_CTX_add1_hkdf_info(
        hkdf_context.get(),
        key_purpose_label,
        sizeof(key_purpose_label) - 1
    ) != 1)
    {
        std::ranges::fill(shared_secret, std::byte{0});
        return filehopz::Error_code::crypto_hkdf_failed;
    }

    // Выведение сразу двух 256-bit ключей: low->high и high->low
    std::array<std::byte, KEY_LEN * 2> directional_keys;
    std::size_t key_length = directional_keys.size();

    if(EVP_PKEY_derive(
        hkdf_context.get(),
        reinterpret_cast<unsigned char*>(directional_keys.data()),
        &key_length
    ) != 1)
    {
        std::ranges::fill(shared_secret, std::byte{0});
        return filehopz::Error_code::crypto_hkdf_failed;
    }

    // Проверка заполнения ровно запрошенного объема ключевого материала OpenSSL
    if(key_length != directional_keys.size())
    {
        std::ranges::fill(shared_secret, std::byte{0});
        std::ranges::fill(directional_keys, std::byte{0});
        return filehopz::Error_code::crypto_hkdf_failed;
    }

    const auto* low_to_high_key = directional_keys.data();
    const auto* high_to_low_key = directional_keys.data() + KEY_LEN;
    // Выбор send/recv ключей по порядку public keys: меньший public key означает low->high
    if(own_is_less)
    {
        std::memcpy(send_key_.data(), low_to_high_key, KEY_LEN);
        std::memcpy(recv_key_.data(), high_to_low_key, KEY_LEN);
    }
    else
    {
        std::memcpy(send_key_.data(), high_to_low_key, KEY_LEN);
        std::memcpy(recv_key_.data(), low_to_high_key, KEY_LEN);
    }

    // Очистка временных секретов после раскладки send/recv ключей
    std::ranges::fill(shared_secret, std::byte{0});
    std::ranges::fill(directional_keys, std::byte{0});
    send_nonce_counter_ = 0;

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

    // Ограничение одним send_key_ на 2^64 уникальных nonce
    if(send_nonce_counter_ == std::numeric_limits<uint64_t>::max())
        return std::unexpected(
            filehopz::Error_code::crypto_encrypt_failed
        );

    // Nonce: 32 нулевых бита prefix + 64-битный счетчик в big-endian
    std::ranges::fill(output_packet.begin(), output_packet.begin() + NONCE_LEN, std::byte{0});

    auto nonce_counter = send_nonce_counter_++;
    for(std::size_t i = 0; i < sizeof(nonce_counter); ++i)
    {
        auto shift = 8 * (sizeof(nonce_counter) - 1 - i);
        output_packet[NONCE_LEN - sizeof(nonce_counter) + i] =
            static_cast<std::byte>((nonce_counter >> shift) & 0xff);
    }

    // Создание нового EVP_CIPHER_CTX на каждый пакет из-за разных nonce
    std::unique_ptr<EVP_CIPHER_CTX, EVP_CIPHER_CTX_DELETER> cipher_context(EVP_CIPHER_CTX_new());
    if(!cipher_context)
        return std::unexpected(
            filehopz::Error_code::crypto_encrypt_failed
        );
    
    // Выбор AEAD алгоритма с заданием ключа и nonce отдельным вызовом ниже
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

    // Фиксация 96-bit nonce для ChaCha20-Poly1305
    if(EVP_CIPHER_CTX_ctrl(
        cipher_context.get(),
        EVP_CTRL_AEAD_SET_IVLEN,
        NONCE_LEN,
        nullptr
    ) != 1)
        return std::unexpected(
            filehopz::Error_code::crypto_encrypt_failed
        );
    
    // Привязка encrypt operation к ключу отправки и только что созданному nonce
    if(EVP_EncryptInit_ex(
        cipher_context.get(),
        nullptr,
        nullptr,
        reinterpret_cast<const unsigned char*>(send_key_.data()),
        reinterpret_cast<const unsigned char*>(output_packet.data())
    ) != 1)
        return std::unexpected(
            filehopz::Error_code::crypto_encrypt_failed
        );

    int encrypted_bytes = 0;
    // Запись ciphertext сразу после nonce с поддержкой пустого plaintext
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
    // Завершение EVP operation без добавления bytes для ChaCha20-Poly1305
    if(EVP_EncryptFinal_ex(
        cipher_context.get(),
        nullptr,
        &final_bytes
    ) != 1)
        return std::unexpected(
            filehopz::Error_code::crypto_encrypt_failed
        );
    
    // Запись тега аутентификации в конец пакета после ciphertext
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
    
    if(data.size_bytes() < NONCE_LEN + TAG_LEN)
        return std::unexpected(
            filehopz::Error_code::crypto_ciphertext_too_short
        );

    // Формат encrypted packet: nonce | ciphertext | tag
    const auto nonce_ptr = data.data();
    const auto encrypted_data_ptr = data.data() + NONCE_LEN;
    const auto tag_ptr = data.data() + data.size_bytes() - TAG_LEN;
    const auto encrypted_data_size = data.size_bytes() - NONCE_LEN - TAG_LEN;

    // Использование отдельного EVP context для decrypt, как и для encrypt
    std::unique_ptr<EVP_CIPHER_CTX, EVP_CIPHER_CTX_DELETER> cipher_context(
        EVP_CIPHER_CTX_new()
    );
    if(!cipher_context)
        return std::unexpected(
            filehopz::Error_code::crypto_decrypt_failed
        );
    
    // Выбор того же AEAD алгоритма, что и на стороне отправки
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
    
    // Синхронизация длины nonce с encrypt_data
    if(EVP_CIPHER_CTX_ctrl(
        cipher_context.get(),
        EVP_CTRL_AEAD_SET_IVLEN,
        NONCE_LEN,
        nullptr
    ) != 1)
        return std::unexpected(
            filehopz::Error_code::crypto_decrypt_failed
        );
    
    // Использование recv_key_ для входящих пакетов, противоположного send_key_ peer'а
    if(EVP_DecryptInit_ex(
        cipher_context.get(),
        nullptr,
        nullptr,
        reinterpret_cast<const unsigned char*>(recv_key_.data()),
        reinterpret_cast<const unsigned char*>(nonce_ptr)
    ) != 1)
        return std::unexpected(
            filehopz::Error_code::crypto_decrypt_failed
        );
    
    std::vector<std::byte> output_data(encrypted_data_size);
    int decrypted_bytes = 0;
    // Недоверенный plaintext до проверки tag в Final
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
    
    // Установка ожидаемого tag для проверки целостности и ключа в Final
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
    // Ошибка Final при поврежденных данных, подделке или неверном ключе
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
