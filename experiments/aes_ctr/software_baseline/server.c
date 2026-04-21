#include <arpa/inet.h>
#include <rdma/rdma_cma.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/rand.h>
#include <openssl/err.h>

#include "common.h"

int main(int argc, char *argv[]) {
    struct exchange_data server_buf_info;
    struct exchange_data client_buf_info;

    int err;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <AES_mode>\n", argv[0]);
        fprintf(stderr, "AES_mode options: 0 for ECB, 1 for CTR\n");
        return -1;
    }

    int aes_mode = atoi(argv[1]);
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

    struct rdma_cm_id *listener_cm_id;
    err = rdma_create_id(event_channel, &listener_cm_id, NULL, RDMA_PS_TCP);
    if (err) {
        perror("rdma_create_id");
        return -1;
    }

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(PORT);
    sin.sin_addr.s_addr = INADDR_ANY;

    err = rdma_bind_addr(listener_cm_id, (struct sockaddr *)&sin);
    if (err) {
        perror("rdma_bind_addr");
        return -1;
    }

    err = rdma_listen(listener_cm_id, 1);
    if (err) {
        perror("rdma_listen");
        return -1;
    }

    printf("[Server] Listening on port %d\n", ntohs(sin.sin_port));

    struct rdma_cm_event *cm_event;
    struct rdma_cm_id *event_cm_id;
    err = rdma_get_cm_event(event_channel, &cm_event);
    if (err) {
        perror("rdma_get_cm_event");
        return -1;
    }

    if (cm_event->event != RDMA_CM_EVENT_CONNECT_REQUEST) {
        fprintf(stderr, "Unexpected event: %d\n", cm_event->event);
        rdma_ack_cm_event(cm_event);
        return -1;
    }

    if (cm_event->param.conn.private_data == NULL) {
        fprintf(stderr, "no client buffer information exchanged!\n");
        return -1;
    }
    memcpy(&client_buf_info, cm_event->param.conn.private_data, sizeof(client_buf_info));
    printf("[Server] Received (net)  buf rkey: 0x%x, buf addr: 0x%lx\n", client_buf_info.buf_rkey, client_buf_info.buf_va);
    printf("[Server] Received (host) buf rkey: 0x%x, buf addr: 0x%lx\n", ntohl(client_buf_info.buf_rkey), ntohll(client_buf_info.buf_va));
    printf("[Server] Received client AES IV:   ");
    for (int i = 0; i < 16; ++i) {
        printf("%02x ", client_buf_info.iv[i]);
    }
    printf("\n");

    event_cm_id = cm_event->id;
    rdma_ack_cm_event(cm_event);

    printf("[Server] Received connection request from client\n");

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

    err = ibv_req_notify_cq(cq, 0);
    if (err != 0) {
        perror("ibv_req_notify_cq");
        return -1;
    }

    uint8_t *buf = NULL;
    buf = malloc(MAX_BUF_SIZE);
    if (buf == NULL) {
        perror("malloc");
        return -1;
    }

    struct ibv_mr *mr;
    mr = ibv_reg_mr(pd, buf, MAX_BUF_SIZE, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE);
    if (mr == NULL) {
        perror("ibv_reg_mr");
        return -1;
    }

    uint32_t notify_flag = 0;
    struct ibv_mr *notify_flag_mr;
    notify_flag_mr = ibv_reg_mr(pd, &notify_flag, sizeof(notify_flag), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE);
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
    sge.length = sizeof(notify_flag);
    sge.lkey = notify_flag_mr->lkey;

    struct ibv_recv_wr recv_wr = {};
    struct ibv_recv_wr *bad_recv_wr;
    recv_wr.wr_id = 0;
    recv_wr.sg_list = &sge;
    recv_wr.num_sge = 1;
    if (ibv_post_recv(event_cm_id->qp, &recv_wr, &bad_recv_wr) != 0) {
        perror("ibv_post_recv");
        return -1;
    }

    server_buf_info.buf_va = htonll((uintptr_t)buf);
    server_buf_info.buf_rkey = htonl(mr->rkey);
    if (RAND_bytes(server_buf_info.iv, 16) != 1) {
        ERR_print_errors_fp(stderr);
        return -1;
    }

    printf("[Server] Sending (host) buf rkey: 0x%x, buf addr: 0x%lx\n", mr->rkey, (uintptr_t)buf);
    printf("[Server] Sending (net)  buf rkey: 0x%x, buf addr: 0x%lx\n", server_buf_info.buf_rkey, server_buf_info.buf_va);
    printf("[Server] Sending server AES IV:   ");
    for (int i = 0; i < 16; ++i) {
        printf("%02x ", server_buf_info.iv[i]);
    }
    printf("\n");

    struct rdma_conn_param conn_param = {};
    conn_param.responder_resources = 1;
    conn_param.private_data = &server_buf_info;
    conn_param.private_data_len = sizeof(server_buf_info);

    err = rdma_accept(event_cm_id, &conn_param);
    if (err != 0) {
        perror("rdma_accept");
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
    printf("[Server] Accepted connection from client\n");
    rdma_ack_cm_event(cm_event);

    // Connection established
    // rdma_post_recv for notify_flag was issued

    for (uint32_t sz = START_SIZE; sz != 0 && sz <= MAX_BUF_SIZE; sz <<= 1) {
        printf("[Server] TEST SIZE # %u\n", sz);

        for (int trial = 0; trial < NUM_TRAIL; ++trial) {
            // Wait for notify_flag to be written by client
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

            notify_flag = ntohl(notify_flag);
            // printf("[Server] Received notify_flag: %u\n", notify_flag);

            // Process the buffer
            decrypt_buf_AES_128(buf, notify_flag, DEF_AES_KEY, client_buf_info.iv, aes_mode);
            encrypt_buf_AES_128(buf, notify_flag, DEF_AES_KEY, server_buf_info.iv, aes_mode);
            uint32_t processed_size = notify_flag;
            notify_flag = htonl(processed_size);

            // RDMA WRITE back received buffer
            struct ibv_send_wr send_wr = {};
            struct ibv_send_wr *bad_send_wr;

            sge.addr = (uintptr_t)buf;
            sge.length = processed_size;
            sge.lkey = mr->lkey;

            send_wr.wr_id = 1;
            send_wr.opcode = IBV_WR_RDMA_WRITE;
            send_wr.send_flags = IBV_SEND_SIGNALED;
            send_wr.sg_list = &sge;
            send_wr.num_sge = 1;
            send_wr.wr.rdma.rkey = ntohl(client_buf_info.buf_rkey);
            send_wr.wr.rdma.remote_addr = ntohll(client_buf_info.buf_va);

            if (ibv_post_send(event_cm_id->qp, &send_wr, &bad_send_wr)) {
                perror("ibv_post_send");
                return -1;
            }

            // Wait for RDMA WRITE completion
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

            // Send back notify_flag
            sge.addr = (uintptr_t)&notify_flag;
            sge.length = sizeof(notify_flag);
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

            // Wait for SEND completion
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

            // printf("[Server] Sent notify_flag: %u\n", ntohl(notify_flag));
        }
    }

    return 0;
}