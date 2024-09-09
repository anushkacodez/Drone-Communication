// encryption.cpp
#include "encryption.h"

std::string xor_encrypt_decrypt(const std::string &data, const std::string &key) {
    std::string result = data;
    for (size_t i = 0; i < data.size(); ++i) {
        result[i] = data[i] ^ key[i % key.size()];
    }
    return result;
}