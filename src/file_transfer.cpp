#include "file_transfer.hpp"
#include <iostream>

boost::system::error_code File_transfer::print_progress(std::size_t bytes_transferred)
{
    static std::size_t total_bytes_transferred = 0;
    static uint8_t last_progress = 0;
    total_bytes_transferred += bytes_transferred;
    uint8_t new_progress = (total_bytes_transferred * 100) / bytes_to_transfer_;
    if(new_progress > last_progress)
    {
        std::cout << "Progress: " << new_progress << "%\n";
        last_progress = new_progress;
    }
    return {};
}