#include <h2o.h>
#include <uv.h>

#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>

#define MAX_LEN 32
#define MAX_NUM 1000
#define MSG_LEN 1024
#define MAX_ALL 128000

static char all_users[MAX_ALL];

struct userInfo {
    char id[MAX_LEN];
    char name[MAX_LEN];
    float balance;
};
typedef struct userInfo user_t;
user_t users[MAX_NUM];

struct commodityInfo {
    char id[MAX_LEN];
    char name[MAX_LEN];
    unsigned int number;
    float price;
};
typedef struct commodityInfo commodity_t;
commodity_t commodities[MAX_NUM];

unsigned int userNum, commodityNum;

static h2o_globalconf_t config;
static h2o_context_t ctx;
static h2o_accept_ctx_t accept_ctx;

static int get_user_by_id(h2o_handler_t *self, h2o_req_t *req)
{
    static h2o_generator_t generator = {NULL, NULL};

    if (!h2o_memis(req->method.base, req->method.len, H2O_STRLIT("GET")))
        return -1;

    char user_id[20] = { 0 };
    h2o_iovec_t status_list = { NULL, 0 };
    size_t para_len = sizeof("?user_id=") - 1;
    if ((req->query_at != SIZE_MAX) && ((req->path.len - req->query_at) > para_len)) {
        if (h2o_memis(&req->path.base[req->query_at], para_len, "?user_id=", para_len)) {
            status_list = h2o_iovec_init(&req->path.base[req->query_at + para_len], req->path.len - req->query_at - para_len);
            strncpy(user_id, status_list.base, status_list.len);
            printf("user_id = %s\n", user_id);
            //int i = 0;
            //for (; i < status_list.len; i++) {
            //    if (status_list.base[i] == ' ') {
            //        strncpy(user_id, status_list.base, i);
            //        printf("user_id = %s\n", user_id);
            //        break;
            //    }
            //}
        }
    }

    char result[MSG_LEN] = { 0 };
    int uid = atoi(user_id) - 1;
    sprintf(result, "{\"user_id\":%s,\"user_name\":%s,\"account_balance\":%f}", 
            users[uid].id, users[uid].name, users[uid].balance);
    
    h2o_iovec_t body = h2o_strdup(&req->pool, result, SIZE_MAX);
    req->res.status = 200;
    req->res.reason = "OK";
    h2o_add_header(&req->pool, &req->res.headers, H2O_TOKEN_CONTENT_TYPE, NULL, H2O_STRLIT("application/json"));
    h2o_start_response(req, &generator);
    h2o_send(req, &body, 1, 1);

    return 0;
}

static int get_user_all(h2o_handler_t *self, h2o_req_t *req)
{
    static h2o_generator_t generator = {NULL, NULL};

    if (!h2o_memis(req->method.base, req->method.len, H2O_STRLIT("GET")))
        return -1;

    int uid = 0;
    memset(all_users, 0, MAX_ALL);
    strcpy(all_users, "[");
    char iterator[MSG_LEN];
    for (; uid < userNum; uid++) {
        sprintf(iterator, "{\"user_id\":%s,\"user_name\":%s,\"account_balance\":%f},",
                users[uid].id, users[uid].name, users[uid].balance);
        strcat(all_users, iterator);
    }
    all_users[strlen(all_users) - 1] = ']';
    //printf("all_users: %s\n", all_users);
    
    h2o_iovec_t body = h2o_strdup(&req->pool, all_users, SIZE_MAX);
    req->res.status = 200;
    req->res.reason = "OK";
    h2o_add_header(&req->pool, &req->res.headers, H2O_TOKEN_CONTENT_TYPE, NULL, H2O_STRLIT("application/json"));
    h2o_start_response(req, &generator);
    h2o_send(req, &body, 1, 1);

    return 0;
}

int data_init() {
    FILE *fp;
    if ((fp = fopen("./SecKill/input.txt", "r")) == NULL) {
        return -1;
    }

    size_t i;
    
    fscanf(fp, "%u\n", &userNum);
    memset(users, 0, sizeof(user_t) * MAX_NUM);
    for (i = 0; i < userNum; i++) {
        fscanf(fp, "%20[^,],%20[^,],%f\n", users[i].id, users[i].name, &users[i].balance);
    }
    
    fscanf(fp, "%u\n", &commodityNum);
    memset(commodities, 0, sizeof(commodity_t) * MAX_NUM);
    for (i = 0; i < commodityNum; i++) {
        fscanf(fp, "%20[^,],%20[^,],%u,%f\n", commodities[i].id, commodities[i].name, 
                &commodities[i].number, &commodities[i].price);
    }
    
    fclose(fp);
    return 0;
}

static h2o_pathconf_t *register_handler(h2o_hostconf_t *hostconf, const char *path, int (*on_req)(h2o_handler_t *, h2o_req_t *))
{
    h2o_pathconf_t *pathconf = h2o_config_register_path(hostconf, path, 0);
    h2o_handler_t *handler = h2o_create_handler(pathconf, sizeof(*handler));
    handler->on_req = on_req;
    return pathconf;
}

static void on_accept(uv_stream_t *listener, int status)
{
    uv_tcp_t *conn;
    h2o_socket_t *sock;

    if (status != 0)
        return;

    conn = h2o_mem_alloc(sizeof(*conn));
    uv_tcp_init(listener->loop, conn);

    if (uv_accept(listener, (uv_stream_t *)conn) != 0) {
        uv_close((uv_handle_t *)conn, (uv_close_cb)free);
        return;
    }

    sock = h2o_uv_socket_create((uv_handle_t *)conn, (uv_close_cb)free);
    h2o_accept(&accept_ctx, sock);
}

static int create_listener(void)
{
    static uv_tcp_t listener;
    struct sockaddr_in addr;
    int r;

    uv_tcp_init(ctx.loop, &listener);
    uv_ip4_addr("127.0.0.1", 7890, &addr);
    if ((r = uv_tcp_bind(&listener, (struct sockaddr *)&addr, 0)) != 0) {
        fprintf(stderr, "uv_tcp_bind:%s\n", uv_strerror(r));
        goto Error;
    }
    if ((r = uv_listen((uv_stream_t *)&listener, 128, on_accept)) != 0) {
        fprintf(stderr, "uv_listen:%s\n", uv_strerror(r));
        goto Error;
    }

    return 0;
Error:
    uv_close((uv_handle_t *)&listener, NULL);
    return r;
}

int main(int argc, char **argv)
{
    if (data_init() < 0) {
        return -1;
    }
    
    h2o_hostconf_t *hostconf;

    signal(SIGPIPE, SIG_IGN);

    h2o_config_init(&config);
    hostconf = h2o_config_register_host(&config, h2o_iovec_init(H2O_STRLIT("default")), 65535);
    register_handler(hostconf, "/getUserById", get_user_by_id);
    register_handler(hostconf, "/getUserAll", get_user_all);
    //register_handler(hostconf, "/getUserById", get_user_by_id);
    //register_handler(hostconf, "/getUserById", get_user_by_id);
    //register_handler(hostconf, "/getUserById", get_user_by_id);
    //register_handler(hostconf, "/getUserById", get_user_by_id);
    //register_handler(hostconf, "/getUserById", get_user_by_id);

    uv_loop_t loop;
    uv_loop_init(&loop);
    h2o_context_init(&ctx, &loop, &config);

    /* disabled by default: uncomment the line below to enable access logging */
    /* h2o_access_log_register(&config.default_host, "/dev/stdout", NULL); */

    accept_ctx.ctx = &ctx;
    accept_ctx.hosts = config.hosts;

    if (create_listener() != 0) {
        fprintf(stderr, "failed to listen to 127.0.0.1:7890:%s\n", strerror(errno));
        goto Error;
    }

    uv_run(ctx.loop, UV_RUN_DEFAULT);

Error:
    return 1;
}
