#include "errors/filehopz_error.hpp"

#include <string>

namespace filehopz
{
    namespace
    {
        class Filehopz_error_category : public boost::system::error_category
        {
            public:
                const char* name() const noexcept override
                {
                    return "filehopz";
                }

                std::string message(int value) const override
                {
                    switch(static_cast<Error_code>(value))
                    {
                        case Error_code::crypto_context_not_ready:
                            return "crypto context is not ready: handshake did not finish before encryption or decryption";
                        case Error_code::crypto_key_generation_failed:
                            return "failed to generate local crypto key material with OpenSSL";
                        case Error_code::crypto_peer_key_failed:
                            return "failed to accept peer public key or derive shared X25519 secret";
                        case Error_code::crypto_hkdf_failed:
                            return "failed to derive session encryption key with HKDF";
                        case Error_code::crypto_random_failed:
                            return "failed to generate cryptographic random nonce";
                        case Error_code::crypto_encrypt_failed:
                            return "failed to encrypt packet payload with ChaCha20-Poly1305";
                        case Error_code::crypto_decrypt_failed:
                            return "failed to decrypt or authenticate packet payload; data may be corrupted or keys do not match";
                        case Error_code::crypto_ciphertext_too_short:
                            return "encrypted packet is too short to contain nonce, payload and authentication tag";

                        case Error_code::loops_not_running:
                            return "transport loops are not running; protocol operation cannot continue";
                        case Error_code::packet_too_large:
                            return "packet size exceeds FileHopZ protocol limit";
                        case Error_code::invalid_handshake_key_size:
                            return "handshake packet contains public key with unexpected size";
                        case Error_code::transport_encrypt_failed:
                            return "transport failed to encrypt outgoing packet";
                        case Error_code::transport_decrypt_failed:
                            return "transport failed to decrypt incoming packet";
                        case Error_code::stream_payload_too_large:
                            return "stream payload is larger than one FileHopZ transport packet";
                        case Error_code::connection_idle_timeout:
                            return "connection timed out: no packets received from peer for 30 seconds";

                        case Error_code::malformed_packet:
                            return "received malformed FileHopZ packet; sender and receiver may use incompatible versions or data is corrupted";
                        case Error_code::receiver_refused_transfer:
                            return "receiver refused the file transfer";
                        case Error_code::unknown_packet_type:
                            return "received unknown FileHopZ file-transfer packet type";
                        case Error_code::transfer_size_exceeded:
                            return "sender attempted to send more file data than was announced before transfer";
                        case Error_code::premature_end_transfer:
                            return "sender ended transfer before all announced file data was received";

                        case Error_code::unsupported_file_type:
                            return "input contains unsupported filesystem entry type";
                        case Error_code::symlink_payload_too_large:
                            return "symlink path and target are too large for one FileHopZ file-transfer packet";
                        case Error_code::malformed_symlink_payload:
                            return "received malformed symlink packet payload";
                        case Error_code::unknown_file_id:
                            return "packet references unknown file id; file was not opened or was already closed";
                        case Error_code::file_open_failed:
                            return "failed to open destination or source file";
                        case Error_code::file_write_failed:
                            return "failed to write destination file data";
                    }

                    return "unknown FileHopZ error";
                }
        };
    }

    const boost::system::error_category&
    error_category() noexcept
    {
        // Category должна быть одной на процесс: error_code сравнивает category по адресу
        static const Filehopz_error_category category;
        return category;
    }

    boost::system::error_code
    make_error_code(Error_code code) noexcept
    {
        return {static_cast<int>(code), error_category()};
    }

    std::string_view
    error_name(Error_code code) noexcept
    {
        switch(code)
        {
            case Error_code::crypto_context_not_ready:
                return "crypto_context_not_ready";
            case Error_code::crypto_key_generation_failed:
                return "crypto_key_generation_failed";
            case Error_code::crypto_peer_key_failed:
                return "crypto_peer_key_failed";
            case Error_code::crypto_hkdf_failed:
                return "crypto_hkdf_failed";
            case Error_code::crypto_random_failed:
                return "crypto_random_failed";
            case Error_code::crypto_encrypt_failed:
                return "crypto_encrypt_failed";
            case Error_code::crypto_decrypt_failed:
                return "crypto_decrypt_failed";
            case Error_code::crypto_ciphertext_too_short:
                return "crypto_ciphertext_too_short";

            case Error_code::loops_not_running:
                return "loops_not_running";
            case Error_code::packet_too_large:
                return "packet_too_large";
            case Error_code::invalid_handshake_key_size:
                return "invalid_handshake_key_size";
            case Error_code::transport_encrypt_failed:
                return "transport_encrypt_failed";
            case Error_code::transport_decrypt_failed:
                return "transport_decrypt_failed";
            case Error_code::stream_payload_too_large:
                return "stream_payload_too_large";
            case Error_code::connection_idle_timeout:
                return "connection_idle_timeout";

            case Error_code::malformed_packet:
                return "malformed_packet";
            case Error_code::receiver_refused_transfer:
                return "receiver_refused_transfer";
            case Error_code::unknown_packet_type:
                return "unknown_packet_type";
            case Error_code::transfer_size_exceeded:
                return "transfer_size_exceeded";
            case Error_code::premature_end_transfer:
                return "premature_end_transfer";

            case Error_code::unsupported_file_type:
                return "unsupported_file_type";
            case Error_code::symlink_payload_too_large:
                return "symlink_payload_too_large";
            case Error_code::malformed_symlink_payload:
                return "malformed_symlink_payload";
            case Error_code::unknown_file_id:
                return "unknown_file_id";
            case Error_code::file_open_failed:
                return "file_open_failed";
            case Error_code::file_write_failed:
                return "file_write_failed";
        }

        return "unknown_filehopz_error";
    }
}
