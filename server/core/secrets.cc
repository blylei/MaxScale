/*
 * Copyright (c) 2016 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2024-04-23
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

#include <maxscale/secrets.hh>

#include <cctype>
#include <fstream>
#include <sys/stat.h>
#include <openssl/aes.h>

#include <maxbase/format.hh>
#include <maxscale/paths.hh>
#include <maxscale/utils.hh>
#include "internal/secrets.hh"

using std::string;

const char* const SECRETS_FILENAME = ".secrets";

namespace
{

struct ThisUnit
{
    std::unique_ptr<EncryptionKeys> keys;
};
ThisUnit this_unit;

}

/**
 * Reads binary data from file and extracts the AES encryption key and init vector. The source file needs to
 * be a certain size and have expected permissions. If the source file does not exist, returns empty results.
 *
 * @param filepath Path to binary file.
 * @return Result structure. Ok if file was loaded successfully or if file did not exist. False on error.
 */

ReadKeyResult secrets_readkeys(const string& filepath)
{
    ReadKeyResult rval;
    auto filepathc = filepath.c_str();

    // Before opening the file, check its size and permissions.
    struct stat filestats { 0 };
    bool stat_error = false;
    errno = 0;
    if (stat(filepathc, &filestats) == 0)
    {
        auto filesize = filestats.st_size;
        if (filesize != EncryptionKeys::total_len)
        {
            MXS_ERROR("Size of secrets file '%s' is %li when %i was expected.",
                      filepathc, filesize, EncryptionKeys::total_len);
            stat_error = true;
        }

        auto filemode = filestats.st_mode;
        if (!S_ISREG(filemode))
        {
            MXS_ERROR("Secrets file '%s' is not a regular file.", filepathc);
            stat_error = true;
        }
        else if ((filemode & (S_IRWXU | S_IRWXG | S_IRWXO)) != S_IRUSR)
        {
            MXS_ERROR("Secrets file '%s' permissions are wrong. The only permission on the file should be "
                      "owner:read.", filepathc);
            stat_error = true;
        }
    }
    else if (errno == ENOENT)
    {
        // The file does not exist. This is ok. Return empty result.
        rval.ok = true;
        return rval;
    }
    else
    {
        MXS_ERROR("stat() for secrets file '%s' failed. Error %d, %s.",
                  filepathc, errno, mxs_strerror(errno));
        stat_error = true;
    }

    if (stat_error)
    {
        return rval;
    }

    // Open file in binary read mode.
    errno = 0;
    std::ifstream file(filepath, std::ios_base::binary);
    if (file.is_open())
    {
        // Read all data from file.
        char readbuf[EncryptionKeys::total_len];
        file.read(readbuf, sizeof(readbuf));
        if (file.good())
        {
            // Success, copy contents to key structure.
            rval.key = std::make_unique<EncryptionKeys>();
            memcpy(rval.key->enckey, readbuf, EncryptionKeys::key_len);
            memcpy(rval.key->initvector, readbuf + EncryptionKeys::key_len, EncryptionKeys::iv_len);
            rval.ok = true;
        }
        else
        {
            MXS_ERROR("Read from secrets file %s failed. Read %li, expected %i bytes. Error %d, %s.",
                      filepathc, file.gcount(), EncryptionKeys::total_len, errno, mxs_strerror(errno));
        }
    }
    else
    {
        MXS_ERROR("Could not open secrets file '%s'. Error %d, %s.", filepathc, errno, mxs_strerror(errno));
    }
    return rval;
}

namespace maxscale
{
string decrypt_password(const string& crypt)
{
    const auto* key = this_unit.keys.get();
    if (!key)
    {
        // Password encryption is not used, so return original.
        return crypt;
    }

    // If the input is not a HEX string, return the input as is. Likely it was not encrypted.
    for (auto c : crypt)
    {
        if (!isxdigit(c))
        {
            return crypt;
        }
    }

    return decrypt_password(*key, crypt);
}
}


std::string decrypt_password(const EncryptionKeys& key, const std::string& input)
{
    // Convert to binary.
    size_t hex_len = input.length();
    auto bin_len = hex_len / 2;
    unsigned char encrypted_bin[bin_len];
    mxs::hex2bin(input.c_str(), hex_len, encrypted_bin);

    AES_KEY aeskey;
    AES_set_decrypt_key(key.enckey, 8 * EncryptionKeys::key_len, &aeskey);

    auto plain_len = bin_len + 1;   // Decryption output cannot be longer than input data.
    unsigned char plain[plain_len];
    memset(plain, '\0', plain_len);

    // Need to copy the init vector as it's modified during decryption.
    unsigned char init_vector[EncryptionKeys::iv_len];
    memcpy(init_vector, key.initvector, EncryptionKeys::iv_len);
    AES_cbc_encrypt(encrypted_bin, plain, bin_len, &aeskey, init_vector, AES_DECRYPT);

    string rval((char*)plain);
    return rval;
}

/**
 * Encrypt a password that can be stored in the MaxScale configuration file.
 *
 * @param key Encryption key and init vector
 * @param input The plaintext password to encrypt.
 * @return The encrypted password, or empty on failure.
 */
string encrypt_password(const EncryptionKeys& key, const string& input)
{
    AES_KEY aeskey;
    AES_set_encrypt_key(key.enckey, 8 * EncryptionKeys::key_len, &aeskey);

    size_t padded_len = ((input.length() / AES_BLOCK_SIZE) + 1) * AES_BLOCK_SIZE;
    unsigned char encrypted[padded_len + 1];

    unsigned char init_vector[EncryptionKeys::iv_len];
    memcpy(init_vector, key.initvector, EncryptionKeys::iv_len);
    AES_cbc_encrypt((const unsigned char*) input.c_str(), encrypted, padded_len,
                    &aeskey, init_vector, AES_ENCRYPT);

    char hex_output[2 * padded_len + 1];
    mxs::bin2hex(encrypted, padded_len, hex_output);
    return hex_output;
}

bool load_encryption_keys()
{
    mxb_assert(this_unit.keys == nullptr);
    string path(mxs::datadir());
    path.append("/").append(SECRETS_FILENAME);
    auto ret = secrets_readkeys(path);
    if (ret.ok)
    {
        if (ret.key)
        {
            MXB_NOTICE("Using encrypted passwords. Encryption key read from '%s'.", path.c_str());
            this_unit.keys = move(ret.key);
        }
        else
        {
            MXB_NOTICE("Password encryption key file '%s' not found, using configured passwords as "
                       "plaintext.", path.c_str());
        }
    }
    return ret.ok;
}
