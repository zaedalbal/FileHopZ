#pragma once

#include <boost/system/error_code.hpp>
#include <string_view>
#include <type_traits>

namespace filehopz
{
    enum class Error_code
    {
        crypto_context_not_ready,
        crypto_key_generation_failed,
        crypto_peer_key_failed,
        crypto_hkdf_failed,
        crypto_random_failed,
        crypto_encrypt_failed,
        crypto_decrypt_failed,
        crypto_ciphertext_too_short,

        loops_not_running,
        packet_too_large,
        invalid_handshake_key_size,
        transport_encrypt_failed,
        transport_decrypt_failed,
        stream_payload_too_large,

        malformed_packet,
        receiver_refused_transfer,
        unknown_packet_type,
        transfer_size_exceeded,
        premature_end_transfer,

        unsupported_file_type,
        symlink_payload_too_large,
        malformed_symlink_payload,
        unknown_file_id,
        file_open_failed,
        file_write_failed
    };

    const boost::system::error_category&
    error_category() noexcept;

    // Часть контракта Boost.System: enum превращается в error_code через ADL
    boost::system::error_code
    make_error_code(Error_code code) noexcept;

    std::string_view
    error_name(Error_code code) noexcept;
}

namespace boost::system
{
    // Разрешает неявно возвращать filehopz::Error_code как boost::system::error_code
    template<>
    struct is_error_code_enum<filehopz::Error_code> : std::true_type
    {};
}
