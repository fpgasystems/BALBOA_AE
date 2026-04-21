#include <openssl/evp.h>
#include <openssl/err.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

typedef struct {
    int mode;
    const unsigned char *text_in;
    unsigned char *text_out;
    uint32_t block_num;
    const unsigned char *key;
    unsigned char iv[16];
} aes_thread_data_t;

void iv_increment(unsigned char *iv, uint32_t val) {
    for (int i = 15; i >= 0; i--) {
        uint32_t sum = iv[i] + (val & 0xFF);
        iv[i] = (unsigned char)sum;
        val = (val >> 8) | (sum >> 8); // Carry overflow to next byte
        if (val == 0) break;
    }
}

void *thread_encrypt_AES_128(void *arg) {
    aes_thread_data_t *thread_data = (aes_thread_data_t *)arg;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL) {
        ERR_print_errors_fp(stderr);
        abort();
    }

    switch (thread_data->mode) {
    case ECB_MODE:
        if (EVP_EncryptInit_ex(ctx, EVP_aes_128_ecb(), NULL, thread_data->key, NULL) != 1) {
            ERR_print_errors_fp(stderr);
            abort();
        }
        break;
    case CTR_MODE:
        if (EVP_EncryptInit_ex(ctx, EVP_aes_128_ctr(), NULL, thread_data->key, thread_data->iv) != 1) {
            ERR_print_errors_fp(stderr);
            abort();
        }
        break;
    default:
        fprintf(stderr, "Unsupported AES mode: %d\n", thread_data->mode);
        abort();
    }

    EVP_CIPHER_CTX_set_padding(ctx, 0);

    for (int i = 0; i < thread_data->block_num; ++i) {
        int tmp;
        if (EVP_EncryptUpdate(ctx, thread_data->text_out + i * AES_BLOCK_SIZE, &tmp, thread_data->text_in + i * AES_BLOCK_SIZE, AES_BLOCK_SIZE) != 1) {
            ERR_print_errors_fp(stderr);
            abort();
        }
    }

    EVP_CIPHER_CTX_free(ctx);
    return NULL;
}

void *thread_decrypt_AES_128(void *arg) {
    aes_thread_data_t *thread_data = (aes_thread_data_t *)arg;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL) {
        ERR_print_errors_fp(stderr);
        abort();
    }

    switch (thread_data->mode) {
    case ECB_MODE:
        if (EVP_DecryptInit_ex(ctx, EVP_aes_128_ecb(), NULL, thread_data->key, NULL) != 1) {
            ERR_print_errors_fp(stderr);
            abort();
        }
        break;
    case CTR_MODE:
        if (EVP_DecryptInit_ex(ctx, EVP_aes_128_ctr(), NULL, thread_data->key, thread_data->iv) != 1) {
            ERR_print_errors_fp(stderr);
            abort();
        }
        break;
    default:
        fprintf(stderr, "Unsupported AES mode: %d\n", thread_data->mode);
        abort();
    }

    EVP_CIPHER_CTX_set_padding(ctx, 0);

    for (int i = 0; i < thread_data->block_num; ++i) {
        int tmp;
        if (EVP_DecryptUpdate(ctx, thread_data->text_out + i * AES_BLOCK_SIZE, &tmp, thread_data->text_in + i * AES_BLOCK_SIZE, AES_BLOCK_SIZE) != 1) {
            ERR_print_errors_fp(stderr);
            abort();
        }
    }

    EVP_CIPHER_CTX_free(ctx);
    return NULL;
}

void encrypt_buf_AES_128(uint8_t *buf, uint32_t size, const char *key, const char *iv, int mode) {
    static pthread_t threads[MAX_THREADS];
    static aes_thread_data_t thread_data[MAX_THREADS];

    if (mode != ECB_MODE && 
        mode != CTR_MODE) {
        fprintf(stderr, "Unsupported AES mode: %d\n", mode);
        return;
    }

#ifdef DEBUG
    printf("---- Encrypting ----\n");
    for (int i = 0; i < size; ++i) {
        printf("%02x ", buf[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
#endif

    int total_blocks = size / AES_BLOCK_SIZE;
    int num_threads = (total_blocks < MAX_THREADS) ? total_blocks : MAX_THREADS;
    int blocks_per_thread = total_blocks / num_threads;
    int remaining_blocks = total_blocks % num_threads;

    for (int i = 0; i < num_threads; ++i) {
        thread_data[i].mode = mode;
        thread_data[i].text_in = buf + (i * blocks_per_thread * AES_BLOCK_SIZE);
        thread_data[i].text_out = buf + (i * blocks_per_thread * AES_BLOCK_SIZE);
        thread_data[i].block_num = blocks_per_thread + (i == num_threads - 1 ? remaining_blocks : 0);
        thread_data[i].key = key;

        if (mode == CTR_MODE) {
            memcpy(thread_data[i].iv, iv, 16);
            iv_increment(thread_data[i].iv, i * blocks_per_thread);
        }

        if (pthread_create(&threads[i], NULL, thread_encrypt_AES_128, &thread_data[i]) != 0) {
            perror("pthread_create");
            return;
        }
    }

    for (int i = 0; i < num_threads; ++i) {
        pthread_join(threads[i], NULL);
    }

#ifdef DEBUG
    printf("---- Encrypted ----\n");
    for (int i = 0; i < size; ++i) {
        printf("%02x ", buf[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
#endif
}

void decrypt_buf_AES_128(uint8_t *buf, uint32_t size, const char *key, const char *iv, int mode) {
    static pthread_t threads[MAX_THREADS];
    static aes_thread_data_t thread_data[MAX_THREADS];

    if (mode != ECB_MODE && 
        mode != CTR_MODE) {
        fprintf(stderr, "Unsupported AES mode: %d\n", mode);
        return;
    }

#ifdef DEBUG
    printf("---- Decrypting ----\n");
    for (int i = 0; i < size; ++i) {
        printf("%02x ", buf[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
#endif

    int total_blocks = size / AES_BLOCK_SIZE;
    int num_threads = (total_blocks < MAX_THREADS) ? total_blocks : MAX_THREADS;
    int blocks_per_thread = total_blocks / num_threads;
    int remaining_blocks = total_blocks % num_threads;

    for (int i = 0; i < num_threads; ++i) {
        thread_data[i].mode = mode;
        thread_data[i].text_in = buf + (i * blocks_per_thread * AES_BLOCK_SIZE);
        thread_data[i].text_out = buf + (i * blocks_per_thread * AES_BLOCK_SIZE);
        thread_data[i].block_num = blocks_per_thread + (i == num_threads - 1 ? remaining_blocks : 0);
        thread_data[i].key = key;

        if (mode == CTR_MODE) {
            memcpy(thread_data[i].iv, iv, 16);
            iv_increment(thread_data[i].iv, i * blocks_per_thread);
        }

        if (pthread_create(&threads[i], NULL, thread_decrypt_AES_128, &thread_data[i]) != 0) {
            perror("pthread_create");
            return;
        }
    }

    for (int i = 0; i < num_threads; ++i) {
        pthread_join(threads[i], NULL);
    }

#ifdef DEBUG
    printf("---- Decrypted ----\n");
    for (int i = 0; i < size; ++i) {
        printf("%02x ", buf[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
#endif
}