#include <arpa/inet.h>
#include <netdb.h>
#include <rdma/rdma_cma.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <openssl/rand.h>
#include <openssl/err.h>

#include "common.h"

int main(int argc, char *argv[]) {
    struct exchange_data server_buf_info;
    struct exchange_data client_buf_info;

    int err;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_address> <AES_mode>\n", argv[0]);
        fprintf(stderr, "AES_mode options: 0 for ECB, 1 for CTR\n");
        return -1;
    }

    int aes_mode = atoi(argv[2]);
    if (aes_mode != ECB_MODE && 
        aes_mode != CTR_MODE) {
        fprintf(stderr, "Unsupported AES mode: %d\n", aes_mode);
        return -1;
    }

    // Use rdma_cm to establish connection
    struct rdma_event_channel *event_channel;
    event_channel = rdma_create_event_channel();
    if (event_channel == NULL) {
        perror("rdma_create_event_channel");
        return -1;
    }

    struct rdma_cm_id *event_cm_id;
    err = rdma_create_id(event_channel, &event_cm_id, NULL, RDMA_PS_TCP);
    if (err != 0) {
        perror("rdma_create_id");
        return -1;
    }

    struct addrinfo *res;
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM};
    int n = getaddrinfo(argv[1], TOSTRING(PORT), &hints, &res);
    if (n < 0) {
        fprintf(stderr, "getaddrinfo: %d\n", n);
        return -1;
    }

    for (struct addrinfo *t = res; t != NULL; t = t->ai_next) {
        err = rdma_resolve_addr(event_cm_id, NULL, t->ai_addr, RESOLVE_TIMEOUT);
        if (err == 0) {
            break;
        }
    }
    if (err != 0) {
        fprintf(stderr, "rdma_resolve_addr: %d\n", err);
        return -1;
    }

    printf("[Client] Resolving address %s\n", argv[1]);

    struct rdma_cm_event *cm_event;
    err = rdma_get_cm_event(event_channel, &cm_event);
    if (err != 0) {
        perror("rdma_get_cm_event");
        return -1;
    }

    if (cm_event->event != RDMA_CM_EVENT_ADDR_RESOLVED) {
        fprintf(stderr, "Unexpected event: %d\n", cm_event->event);
        rdma_ack_cm_event(cm_event);
        return -1;
    }

    rdma_ack_cm_event(cm_event);

    err = rdma_resolve_route(event_cm_id, RESOLVE_TIMEOUT);
    if (err != 0) {
        fprintf(stderr, "rdma_resolve_route: %d\n", err);
        return -1;
    }

    err = rdma_get_cm_event(event_channel, &cm_event);
    if (err != 0) {
        perror("rdma_get_cm_event");
        return -1;
    }

    if (cm_event->event != RDMA_CM_EVENT_ROUTE_RESOLVED) {
        fprintf(stderr, "Unexpected event: %d\n", cm_event->event);
        rdma_ack_cm_event(cm_event);
        return -1;
    }

    rdma_ack_cm_event(cm_event);

    struct ibv_pd *pd;
    pd = ibv_alloc_pd(event_cm_id->verbs);
    if (pd == NULL) {
        perror("ibv_alloc_pd");
        return -1;
    }

    struct ibv_comp_channel *comp_channel;
    comp_channel = ibv_create_comp_channel(event_cm_id->verbs);
    if (comp_channel == NULL) {
        perror("ibv_create_comp_channel");
        return -1;
    }

    struct ibv_cq *cq;
    cq = ibv_create_cq(event_cm_id->verbs, 2, NULL, comp_channel, 0);
    if (cq == NULL) {
        perror("ibv_create_cq");
        return -1;
    }

    if (ibv_req_notify_cq(cq, 0)) {
        perror("ibv_req_notify_cq");
        return -1;
    }

    uint8_t *buf = malloc(MAX_BUF_SIZE);
    if (buf == NULL) {
        perror("malloc");
        return -1;
    }

    // printf("[Client] Buffer content:\n");
    // for (int i = 0; i < START_SIZE; i++) {
    //     buf[i] = (uint8_t)i;
    //     printf("%02x ", buf[i]);
    //     if (i % 16 == 15) {
    //         printf("\n");
    //     }
    // }

    struct ibv_mr *mr;
    mr = ibv_reg_mr(pd, buf, MAX_BUF_SIZE, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE);
    if (mr == NULL) {
        perror("ibv_reg_mr");
        return -1;
    }

    uint32_t notify_flag = 0;
    struct ibv_mr *notify_flag_mr;
    notify_flag_mr = ibv_reg_mr(pd, &notify_flag, sizeof(notify_flag), IBV_ACCESS_LOCAL_WRITE);
    if (notify_flag_mr == NULL) {
        perror("ibv_reg_mr");
        return -1;
    }

    struct ibv_qp_init_attr qp_attr = {};
    qp_attr.cap.max_send_wr = 2;
    qp_attr.cap.max_send_sge = 1;
    qp_attr.cap.max_recv_wr = 1;
    qp_attr.cap.max_recv_sge = 1;
    qp_attr.send_cq = cq;
    qp_attr.recv_cq = cq;
    qp_attr.qp_type = IBV_QPT_RC;

    err = rdma_create_qp(event_cm_id, pd, &qp_attr);
    if (err != 0) {
        perror("rdma_create_qp");
        return -1;
    }

    struct ibv_sge sge;
    sge.addr = (uintptr_t)&notify_flag;
    sge.length = sizeof(uint32_t);
    sge.lkey = notify_flag_mr->lkey;

    struct ibv_recv_wr recv_wr = {};
    struct ibv_recv_wr *bad_recv_wr;
    recv_wr.wr_id = 0;
    recv_wr.sg_list = &sge;
    recv_wr.num_sge = 1;
    if (ibv_post_recv(event_cm_id->qp, &recv_wr, &bad_recv_wr)) {
        perror("ibv_post_recv");
        return -1;
    }

    client_buf_info.buf_va = htonll((uint64_t)buf);
    client_buf_info.buf_rkey = htonl(mr->rkey);
    if (RAND_bytes(client_buf_info.iv, 16) != 1) {
        ERR_print_errors_fp(stderr);
        return -1;
    }

    struct rdma_conn_param conn_param = {};
    conn_param.initiator_depth = 1;
    conn_param.retry_count = 7;
    conn_param.private_data = &client_buf_info;
    conn_param.private_data_len = sizeof(client_buf_info);

    printf("[Client] Sending (host) buf rkey: 0x%x, buf addr: 0x%lx\n", mr->rkey, (uint64_t)buf);
    printf("[Client] Sending (net)  buf rkey: 0x%x, buf addr: 0x%lx\n", client_buf_info.buf_rkey, client_buf_info.buf_va);
    printf("[Client] Sending client AES IV:   ");
    for (int i = 0; i < 16; ++i) {
        printf("%02x ", client_buf_info.iv[i]);
    }
    printf("\n");

    err = rdma_connect(event_cm_id, &conn_param);
    if (err != 0) {
        perror("rdma_connect");
        return -1;
    }

    err = rdma_get_cm_event(event_channel, &cm_event);
    if (err != 0) {
        perror("rdma_get_cm_event");
        return -1;
    }

    if (cm_event->event != RDMA_CM_EVENT_ESTABLISHED) {
        fprintf(stderr, "Unexpected event: %d\n", cm_event->event);
        rdma_ack_cm_event(cm_event);
        return -1;
    }
    printf("[Client] Connected to server\n");

    if (cm_event->param.conn.private_data == NULL) {
        fprintf(stderr, "no server buffer information exchanged!\n");
        return -1;
    }
    memcpy(&server_buf_info, cm_event->param.conn.private_data, sizeof(server_buf_info));
    printf("[Client] Received (net)  buf rkey: 0x%x, buf addr: 0x%lx\n", server_buf_info.buf_rkey, server_buf_info.buf_va);
    printf("[Client] Received (host) buf rkey: 0x%x, buf addr: 0x%lx\n", ntohl(server_buf_info.buf_rkey), ntohll(server_buf_info.buf_va));
    printf("[Client] Received server AES IV:   ");
    for (int i = 0; i < 16; ++i) {
        printf("%02x ", server_buf_info.iv[i]);
    }
    printf("\n");

    rdma_ack_cm_event(cm_event);

    // Connection established
    // rdma_post_recv for notify_flag was issued

    struct timespec start, end;
    double lat[NUM_TRAIL], bw[NUM_TRAIL];
    for (uint32_t sz = START_SIZE; sz != 0 && sz <= MAX_BUF_SIZE; sz <<= 1) {
        printf("[Client] TEST SIZE # %u\n", sz);

        for (int trial = 0; trial < NUM_TRAIL; ++trial) {
            clock_gettime(CLOCK_MONOTONIC, &start);

            // Encrypt buf
            encrypt_buf_AES_128(buf, sz, DEF_AES_KEY, client_buf_info.iv, aes_mode);

            // RDMA WRITE buf to server
            struct ibv_send_wr send_wr = {};
            struct ibv_send_wr *bad_send_wr;

            sge.addr = (uintptr_t)buf;
            sge.length = sz;
            notify_flag = htonl(sge.length);
            sge.lkey = mr->lkey;

            send_wr.wr_id = 1;
            send_wr.opcode = IBV_WR_RDMA_WRITE;
            send_wr.send_flags = IBV_SEND_SIGNALED;
            send_wr.sg_list = &sge;
            send_wr.num_sge = 1;
            send_wr.wr.rdma.rkey = ntohl(server_buf_info.buf_rkey);
            send_wr.wr.rdma.remote_addr = ntohll(server_buf_info.buf_va);

            if (ibv_post_send(event_cm_id->qp, &send_wr, &bad_send_wr)) {
                perror("ibv_post_send");
                return -1;
            }

            // Wait for RDMA WRITE completion
            struct ibv_cq *event_cq;
            void *cq_context;
            if (ibv_get_cq_event(comp_channel, &event_cq, &cq_context) != 0) {
                perror("ibv_get_cq_event");
                return -1;
            }

            ibv_ack_cq_events(event_cq, 1);

            if (ibv_req_notify_cq(cq, 0) != 0) {
                perror("ibv_req_notify_cq");
                return -1;
            }

            struct ibv_wc wc;
            if (ibv_poll_cq(cq, 1, &wc) < 1) {
                perror("ibv_poll_cq");
                return -1;
            }

            if (wc.status != IBV_WC_SUCCESS) {
                fprintf(stderr, "wc.status: %s\n", ibv_wc_status_str(wc.status));
                return -1;
            }

            // Send notify_flag
            sge.addr = (uintptr_t)&notify_flag;
            sge.length = sizeof(uint32_t);
            sge.lkey = notify_flag_mr->lkey;

            send_wr.wr_id = 2;
            send_wr.opcode = IBV_WR_SEND;
            send_wr.send_flags = IBV_SEND_SIGNALED;
            send_wr.sg_list = &sge;
            send_wr.num_sge = 1;

            if (ibv_post_send(event_cm_id->qp, &send_wr, &bad_send_wr)) {
                perror("ibv_post_send");
                return -1;
            }

            // Wait for SEND completion
            if (ibv_get_cq_event(comp_channel, &event_cq, &cq_context) != 0) {
                perror("ibv_get_cq_event");
                return -1;
            }

            ibv_ack_cq_events(event_cq, 1);

            if (ibv_req_notify_cq(cq, 0) != 0) {
                perror("ibv_req_notify_cq");
                return -1;
            }

            if (ibv_poll_cq(cq, 1, &wc) < 1) {
                perror("ibv_poll_cq");
                return -1;
            }

            if (wc.status != IBV_WC_SUCCESS) {
                fprintf(stderr, "wc.status: %s\n", ibv_wc_status_str(wc.status));
                return -1;
            }

            // printf("[Client] Sent notify_flag: %u\n", ntohl(notify_flag));

            // Wait server to process buf
            if (ibv_get_cq_event(comp_channel, &event_cq, &cq_context) != 0) {
                perror("ibv_get_cq_event");
                return -1;
            }

            if (!(sz == MAX_BUF_SIZE && trial == NUM_TRAIL - 1) && ibv_req_notify_cq(cq, 0) != 0) {
                perror("ibv_req_notify_cq");
                return -1;
            }

            ibv_ack_cq_events(event_cq, 1);

            if (ibv_poll_cq(cq, 1, &wc) < 1) {
                perror("ibv_poll_cq");
                return -1;
            }

            if (wc.status != IBV_WC_SUCCESS) {
                fprintf(stderr, "wc.status: %s\n", ibv_wc_status_str(wc.status));
                return -1;
            }

            // Decrypt buf
            decrypt_buf_AES_128(buf, sz, DEF_AES_KEY, server_buf_info.iv, aes_mode);

            clock_gettime(CLOCK_MONOTONIC, &end);
            uint64_t elapsed_ns = (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
            lat[trial] = elapsed_ns / 1e6;                              // ms
            bw[trial] = ((double)sz * 2) / (lat[trial] / 1000) / 1024 / 1024;  // MBps
            printf("---- [%03d] Latency: %.10f ms, Bandwidth: %.10f MBps\n", trial, lat[trial], bw[trial]);

            // printf("[Client] Received notify_flag: %u\n", ntohl(notify_flag));

            // Rearm by ibv_post_recv
            if (!(sz == MAX_BUF_SIZE && trial == NUM_TRAIL - 1)) {
                sge.addr = (uintptr_t)&notify_flag;
                sge.length = sizeof(notify_flag);
                sge.lkey = notify_flag_mr->lkey;
                recv_wr.wr_id = 0;
                recv_wr.sg_list = &sge;
                recv_wr.num_sge = 1;
                if (ibv_post_recv(event_cm_id->qp, &recv_wr, &bad_recv_wr) != 0) {
                    perror("ibv_post_recv");
                    return -1;
                }
            }

            // printf("[Client] Buffer content after processing:\n");
            // for (int i = 0; i < START_SIZE; i++) {
            //     printf("%02x ", buf[i]);
            //     if (i % 16 == 15) {
            //         printf("\n");
            //     }
            // }
        }

        // STATISTICS
        double min_lat = lat[0];
        double max_lat = lat[0];
        double sum_lat = lat[0];
        for (int trial = 1; trial < NUM_TRAIL; ++trial) {
            min_lat = lat[trial] < min_lat ? lat[trial] : min_lat;
            max_lat = lat[trial] > max_lat ? lat[trial] : max_lat;
            sum_lat += lat[trial];
        }
        double avg_lat = sum_lat / NUM_TRAIL;
        double min_bw = ((double)sz * 2) / (max_lat / 1000) / 1024 / 1024;
        double max_bw = ((double)sz * 2) / (min_lat / 1000) / 1024 / 1024;
        double avg_bw = ((double)sz * 2) / (avg_lat / 1000) / 1024 / 1024;
        printf("---- [AVG] Latency: %.10f ms, Bandwidth: %.10f MBps\n", avg_lat, avg_bw);
        printf("---- [MIN] Latency: %.10f ms, Bandwidth: %.10f MBps\n", min_lat, min_bw);
        printf("---- [MAX] Latency: %.10f ms, Bandwidth: %.10f MBps\n", max_lat, max_bw);

    }

    return 0;
}