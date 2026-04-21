#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#if __BIG_ENDIAN__
#define htonll(x) (x)
#define ntohll(x) (x)
#else
#define htonll(x) (((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
#define ntohll(x) (((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))
#endif

// #define DEBUG
#ifdef DEBUG
#define DEBUG_PRINTF(fmt, args...) \
    do { \
        fprintf(stderr, "[DEBUG] %s:%d: " fmt, __FILE__, __LINE__, ##args); \
    } while (0)
#else
#define DEBUG_PRINTF(fmt, args...) \
    do { \
    } while (0)
#endif

#define NUM_TRAIL 5

#define PORT 12345
#define STRINGFY(x) #x
#define TOSTRING(x) STRINGFY(x)

#define RESOLVE_TIMEOUT 1000

#define MAX_BUF_SIZE (1024*1024*1024) // 1024MB
#define START_SIZE 64 // 64B

struct exchange_data {
    uint64_t buf_va;
    uint32_t buf_rkey;
    unsigned char iv[16];
};

#define MAX_THREADS 32
#define AES_BLOCK_SIZE 16
#define DEF_AES_KEY "0123456789abcdef"

#define ECB_MODE 0
#define CTR_MODE 1

/*
 * use AES-128-ECB to encrypt the buffer
 * 
 * @buf: pointer to the buffer
 * @size: size of the buffer (must be a multiple of 16)
 * @key: pointer to the key (16 bytes)
 * @iv: pointer to the initialization vector (16 bytes, used in CTR mode)
 * @mode: AES mode (ECB_MODE or CTR_MODE)
 */
void encrypt_buf_AES_128(uint8_t *buf, uint32_t size, const char *key, const char *iv, int mode);

/*
 * use AES-128-ECB to encrypt the buffer
 * 
 * @buf: pointer to the buffer
 * @size: size of the buffer (must be a multiple of 16)
 * @key: pointer to the key (16 bytes)
 * @iv: pointer to the initialization vector (16 bytes, used in CTR mode)
 * @mode: AES mode (ECB_MODE or CTR_MODE)
 */
void decrypt_buf_AES_128(uint8_t *buf, uint32_t size, const char *key, const char *iv, int mode);

#endif