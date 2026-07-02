// для сборки тестов сделать объектные файлы из Crypto_context.cpp и этого файла
// а потом слинковать

#include "../../include/utils/crypto_context.hpp"
#include "../../include/errors/filehopz_error.hpp"
#include <print>
#include <string_view>

// простая проверка условия
static int tests_total  = 0;
static int tests_passed = 0;

static void check(bool condition, std::string_view test_name)
{
    ++tests_total;
    if (condition)
    {
        ++tests_passed;
        std::println("[pass] {}", test_name);
    }
    else
    {
        std::println("[FAIL] {}", test_name);
    }
}

// ---------- тесты ----------

// нельзя шифровать до завершения handshake
static void test_encrypt_before_ready()
{
    Crypto_context context;
    context.init();

    std::vector<std::byte> data = { std::byte{1}, std::byte{2}, std::byte{3} };
    auto result = context.encrypt_data(data);

    check(!result.has_value(), "encrypt до set_peer_public_key возвращает ошибку");
    check(
        result.error() ==
            make_error_code(filehopz::Error_code::crypto_context_not_ready),
        "encrypt до ready: код ошибки crypto_context_not_ready");
}

// нельзя дешифровать до завершения handshake
static void test_decrypt_before_ready()
{
    Crypto_context context;
    context.init();

    std::vector<std::byte> data(NONCE_LEN + 4 + TAG_LEN, std::byte{0});
    auto result = context.decrypt_data(data);

    check(!result.has_value(), "decrypt до set_peer_public_key возвращает ошибку");
    check(
        result.error() ==
            make_error_code(filehopz::Error_code::crypto_context_not_ready),
        "decrypt до ready: код ошибки crypto_context_not_ready");
}

// decrypt отклоняет слишком короткий пакет
static void test_decrypt_too_short()
{
    Crypto_context alice;
    Crypto_context bob;
    alice.init();
    bob.init();
    alice.set_peer_public_key(bob.get_own_public_key());
    bob.set_peer_public_key(alice.get_own_public_key());

    // пакет меньше NONCE_LEN + TAG_LEN
    std::vector<std::byte> tiny(NONCE_LEN + TAG_LEN - 1, std::byte{0});
    auto result = alice.decrypt_data(tiny);

    check(!result.has_value(), "decrypt слишком короткого пакета возвращает ошибку");
    check(
        result.error() ==
            make_error_code(filehopz::Error_code::crypto_ciphertext_too_short),
        "decrypt короткого пакета: код ошибки crypto_ciphertext_too_short");
}

// базовый сценарий: зашифровал alice — расшифровал bob
static void test_basic_encrypt_decrypt()
{
    Crypto_context alice;
    Crypto_context bob;
    alice.init();
    bob.init();
    alice.set_peer_public_key(bob.get_own_public_key());
    bob.set_peer_public_key(alice.get_own_public_key());

    std::string_view message = "hello, bob!";
    std::span<const std::byte> plaintext{
        reinterpret_cast<const std::byte*>(message.data()),
        message.size()
    };

    auto encrypted = alice.encrypt_data(plaintext);
    check(encrypted.has_value(), "encrypt вернул результат без ошибки");

    auto decrypted = bob.decrypt_data(*encrypted);
    check(decrypted.has_value(), "decrypt вернул результат без ошибки");

    check(
        std::ranges::equal(*decrypted, plaintext),
        "расшифрованные данные совпадают с исходными");
}

// каждый вызов encrypt даёт разный шифртекст и следующий nonce
static void test_different_nonce_each_call()
{
    Crypto_context alice;
    Crypto_context bob;
    alice.init();
    bob.init();
    alice.set_peer_public_key(bob.get_own_public_key());
    bob.set_peer_public_key(alice.get_own_public_key());

    std::string_view message = "same message";
    std::span<const std::byte> plaintext{
        reinterpret_cast<const std::byte*>(message.data()),
        message.size()
    };

    auto encrypted1 = alice.encrypt_data(plaintext);
    auto encrypted2 = alice.encrypt_data(plaintext);

    check(encrypted1.has_value() && encrypted2.has_value(), "оба вызова encrypt успешны");
    check(
        !std::ranges::equal(*encrypted1, *encrypted2),
        "два шифртекста одного сообщения различаются");

    check(
        !std::ranges::equal(
            encrypted1->begin(),
            encrypted1->begin() + NONCE_LEN,
            encrypted2->begin(),
            encrypted2->begin() + NONCE_LEN
        ),
        "два вызова encrypt используют разные nonce");

    check((*encrypted1)[NONCE_LEN - 1] == std::byte{0}, "первый nonce заканчивается counter=0");
    check((*encrypted2)[NONCE_LEN - 1] == std::byte{1}, "второй nonce заканчивается counter=1");
}

// подделка одного байта шифртекста — decrypt должен вернуть ошибку
static void test_tampered_ciphertext()
{
    Crypto_context alice;
    Crypto_context bob;
    alice.init();
    bob.init();
    alice.set_peer_public_key(bob.get_own_public_key());
    bob.set_peer_public_key(alice.get_own_public_key());

    std::string_view message = "tamper me";
    std::span<const std::byte> plaintext{
        reinterpret_cast<const std::byte*>(message.data()),
        message.size()
    };

    auto encrypted = alice.encrypt_data(plaintext);
    check(encrypted.has_value(), "encrypt для теста подделки успешен");

    // портим один байт в середине шифртекста (после nonce)
    (*encrypted)[NONCE_LEN] ^= std::byte{0xff};

    auto decrypted = bob.decrypt_data(*encrypted);
    check(!decrypted.has_value(), "decrypt подделанного пакета возвращает ошибку");
}

// подделка тега — decrypt должен вернуть ошибку
static void test_tampered_tag()
{
    Crypto_context alice;
    Crypto_context bob;
    alice.init();
    bob.init();
    alice.set_peer_public_key(bob.get_own_public_key());
    bob.set_peer_public_key(alice.get_own_public_key());

    std::string_view message = "tamper my tag";
    std::span<const std::byte> plaintext{
        reinterpret_cast<const std::byte*>(message.data()),
        message.size()
    };

    auto encrypted = alice.encrypt_data(plaintext);
    check(encrypted.has_value(), "encrypt для теста тега успешен");

    // портим последний байт (тег)
    encrypted->back() ^= std::byte{0xff};

    auto decrypted = bob.decrypt_data(*encrypted);
    check(!decrypted.has_value(), "decrypt с испорченным тегом возвращает ошибку");
}

// шифрование пустых данных
static void test_empty_plaintext()
{
    Crypto_context alice;
    Crypto_context bob;
    alice.init();
    bob.init();
    alice.set_peer_public_key(bob.get_own_public_key());
    bob.set_peer_public_key(alice.get_own_public_key());

    std::span<const std::byte> empty{};

    auto encrypted = alice.encrypt_data(empty);
    check(encrypted.has_value(), "encrypt пустых данных успешен");

    // размер пакета должен быть ровно nonce + tag
    check(
        encrypted->size() == NONCE_LEN + TAG_LEN,
        "пакет пустых данных: размер = nonce + tag");

    auto decrypted = bob.decrypt_data(*encrypted);
    check(decrypted.has_value(), "decrypt пустых данных успешен");
    check(decrypted->empty(), "decrypt пустых данных возвращает пустой plaintext");
}

// неверный ключ: bob пытается расшифровать пакет от alice своим ключом,
// но его encryption_key отличается (другой peer)
static void test_wrong_key()
{
    Crypto_context alice;
    Crypto_context bob;
    Crypto_context eve; // посторонняя сторона
    alice.init();
    bob.init();
    eve.init();

    // alice ↔ bob
    alice.set_peer_public_key(bob.get_own_public_key());
    bob.set_peer_public_key(alice.get_own_public_key());

    // eve ↔ alice (другая сессия, другой ключ)
    eve.set_peer_public_key(alice.get_own_public_key());

    std::string_view message = "only for bob";
    std::span<const std::byte> plaintext{
        reinterpret_cast<const std::byte*>(message.data()),
        message.size()
    };

    auto encrypted = alice.encrypt_data(plaintext);
    check(encrypted.has_value(), "encrypt для теста неверного ключа успешен");

    // eve пытается расшифровать — должна получить ошибку
    auto decrypted_by_eve = eve.decrypt_data(*encrypted);
    check(!decrypted_by_eve.has_value(), "decrypt чужим ключом возвращает ошибку");
}

// ---------- main ----------

int main()
{
    std::println("=== crypto_context tests ===\n");

    test_encrypt_before_ready();
    test_decrypt_before_ready();
    test_decrypt_too_short();
    test_basic_encrypt_decrypt();
    test_different_nonce_each_call();
    test_tampered_ciphertext();
    test_tampered_tag();
    test_empty_plaintext();
    test_wrong_key();

    std::println("\n{}/{} тестов прошли", tests_passed, tests_total);
    return tests_passed == tests_total ? 0 : 1;
}
