#include <string>

#include "cryptonote_core/cryptonote_basic.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote.hpp"

namespace cryptonote
{
std::vector<uint8_t> convert_blob(const std::vector<uint8_t>& templateBlob)
{
    std::vector<uint8_t> result;

    blobdata input =
        std::string(reinterpret_cast<const char*>(templateBlob.data()), templateBlob.size());
    blobdata output = "";

    //convert
    block b = AUTO_VAL_INIT(b);
    if (parse_and_validate_block_from_blob(input, b))
    {
        if (get_block_hashing_blob(b, output))
        {
            result = std::vector<uint8_t>(output.begin(), output.end());
        }
        else
        {
            std::cout << "Failed to create mining block" << std::endl;
//            return THROW_ERROR_EXCEPTION("Failed to create mining block");
        }
    }
    else
    {
        std::cout << "Failed to parse block" << std::endl;
//    return THROW_ERROR_EXCEPTION("Failed to parse block");
    }
    return result;
}

//std::vector<uint8_t> construct_block_blob(const std::vector<uint8_t>& templateBlob, uint32_t nonce)
//{
//    std::vector<uint8_t> result;
//
//    blobdata block_template_blob =
//        std::string(reinterpret_cast<const char*>(templateBlob.data()), templateBlob.size());
//    blobdata output = "";
//
//    block b = AUTO_VAL_INIT(b);
//    if (parse_and_validate_block_from_blob(block_template_blob, b))
//    {
//        b.nonce = nonce;
//        if (block_to_blob(b, output))
//        {
//            result = std::vector<uint8_t>(output.begin(), output.end());
//        }
//        else
//        {
//            std::cout << "Failed to convert block to blob" << std::endl;
////            return THROW_ERROR_EXCEPTION("Failed to convert block to blob");
//        }
//    }
//    else
//    {
//        std::cout << "Failed to parse block" << std::endl;
////        return THROW_ERROR_EXCEPTION("Failed to parse block");
//    }
//    return result;
//}

}