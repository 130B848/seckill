#include <h2o.h>
#include <uv.h>

#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <stdatomic.h>

#define MAX_LEN 40
#define MAX_NUM 1000
#define MSG_LEN 1024
#define MAX_ALL 128000
#define OPTIMISTIC  1


// return json string for /get***All
static char all_users[MAX_ALL];
static char all_commodities[MAX_ALL];
static char all_orders[MAX_ALL];

struct userInfo {
    char id[MAX_LEN];
    char name[MAX_LEN];
    float balance;
};
typedef struct userInfo user_t;
static user_t users[MAX_NUM];

struct commodityInfo {
    char id[MAX_LEN];
    char name[MAX_LEN];
    unsigned int number;
    float price;
};

#define ORD_CACHE_LEN 5000

struct order_cache_entry {
    unsigned long long id;
    char result[MSG_LEN];
};
typedef struct order_cache_entry order_cache_entry;
static order_cache_entry order_cache[ORD_CACHE_LEN];

typedef struct commodityInfo commodity_t;
static commodity_t commodities[MAX_NUM];

static unsigned int userNum, commodityNum;

static h2o_globalconf_t config;
static h2o_context_t ctx;
static h2o_accept_ctx_t accept_ctx;

static redisContext *user_conn, *commodity_conn, *order_conn;
static atomic_ullong _order_id = 1LLU;

static int locks[MAX_NUM] = { 0 };

static inline void lock(int *lock) 
{
    while (__sync_lock_test_and_set(lock, 1))
        ;
}

static inline void unlock(int *lock) 
{
    __sync_lock_release(lock);
}

static int id2idx_user(char *id) {
    int l = 0, r = userNum - 1;
    while (l <= r) {
        int mid = (l + r) >> 1;
        int d = strcmp(id, users[mid].id);
        if (d > 0)
            l = mid + 1;
        else if (d < 0)
            r = mid - 1;
        else return mid;
    }
    return -1;
}

static int id2idx_cmdt(char *id) {
    int l = 0, r = commodityNum - 1;
    while (l <= r) {
        int mid = (l + r) >> 1;
        int d = strcmp(id, commodities[mid].id);
        if (d > 0)
            l = mid + 1;
        else if (d < 0)
            r = mid - 1;
        else return mid;
    }
    return -1;
}

static unsigned int hash(unsigned long long i) {
    return i % ORD_CACHE_LEN;
}

static int get_user_by_id(h2o_handler_t *self, h2o_req_t *req)
{
    static h2o_generator_t generator = {NULL, NULL};

    if (!h2o_memis(req->method.base, req->method.len, H2O_STRLIT("GET")))
        return -1;

    // parse user_id from url
    char user_id[MAX_LEN] = { 0 };
    h2o_iovec_t status_list = { NULL, 0 };
    size_t para_len = sizeof("?user_id=") - 1;
    if ((req->query_at != SIZE_MAX) && ((req->path.len - req->query_at) > para_len)) {
        if (h2o_memis(&req->path.base[req->query_at], para_len, "?user_id=", para_len)) {
            status_list = h2o_iovec_init(&req->path.base[req->query_at + para_len], req->path.len - req->query_at - para_len);
            strncpy(user_id, status_list.base, status_list.len);
            //printf("user_id = %s\n", user_id);
        }
    }

    // get info from redis
    char result[MSG_LEN] = { 0 };
    int uid = id2idx_user(user_id);
    redisReply *reply;
    reply = (redisReply *)redisCommand(user_conn, "GET _u_%s", user_id);
    sprintf(result, "{\"user_id\":\"%s\",\"user_name\":\"%s\",\"account_balance\":%s}",
            users[uid].id, users[uid].name, reply->str);
    freeReplyObject(reply);
    
    // generate response
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
    redisReply *reply;
    for (; uid < userNum; uid++) {
        reply = (redisReply *)redisCommand(user_conn, "GET _u_%s", users[uid].id);
        sprintf(iterator, "{\"user_id\":\"%s\",\"user_name\":\"%s\",\"account_balance\":%s},", 
                    users[uid].id, users[uid].name, reply->str);
        freeReplyObject(reply);
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

static int get_commodity_by_id(h2o_handler_t *self, h2o_req_t *req)
{
    static h2o_generator_t generator = {NULL, NULL};

    if (!h2o_memis(req->method.base, req->method.len, H2O_STRLIT("GET")))
        return -1;

    char commodity_id[MAX_LEN] = { 0 };
    h2o_iovec_t status_list = { NULL, 0 };
    size_t para_len = sizeof("?commodity_id=") - 1;
    if ((req->query_at != SIZE_MAX) && ((req->path.len - req->query_at) > para_len)) {
        if (h2o_memis(&req->path.base[req->query_at], para_len, "?commodity_id=", para_len)) {
            status_list = h2o_iovec_init(&req->path.base[req->query_at + para_len], req->path.len - req->query_at - para_len);
            strncpy(commodity_id, status_list.base, status_list.len);
            //printf("commodity_id = %s\n", commodity_id);
        }
    }

    char result[MSG_LEN] = { 0 };
    int cid = id2idx_cmdt(commodity_id), quantity;
    redisReply *reply;
    reply = (redisReply *)redisCommand(commodity_conn, "GET _c_%s", commodity_id);
    quantity = atoi(reply->str);
    quantity = quantity < 0 ? 0 : quantity;
    sprintf(result, "{\"commodity_id\":\"%s\",\"commodity_name\":\"%s\",\"quantity\":%d,\"unit_price\":%f}",
            commodity_id, commodities[cid].name, quantity, commodities[cid].price);
    
    h2o_iovec_t body = h2o_strdup(&req->pool, result, SIZE_MAX);
    req->res.status = 200;
    req->res.reason = "OK";
    h2o_add_header(&req->pool, &req->res.headers, H2O_TOKEN_CONTENT_TYPE, NULL, H2O_STRLIT("application/json"));
    h2o_start_response(req, &generator);
    h2o_send(req, &body, 1, 1);

    return 0;
}

static int get_commodity_all(h2o_handler_t *self, h2o_req_t *req)
{
    static h2o_generator_t generator = {NULL, NULL};

    if (!h2o_memis(req->method.base, req->method.len, H2O_STRLIT("GET")))
        return -1;

    int cid = 0, quantity;
    memset(all_commodities, 0, MAX_ALL);
    strcpy(all_commodities, "[");
    char iterator[MSG_LEN];
    redisReply *reply;
    for (; cid < commodityNum; cid++) {
        reply = (redisReply *)redisCommand(commodity_conn, "GET _c_%s", commodities[cid].id);
        quantity = atoi(reply->str);
        quantity = quantity < 0 ? 0 : quantity;
        sprintf(iterator, "{\"commodity_id\":\"%s\",\"commodity_name\":\"%s\",\"quantity\":%d,\"unit_price\":%f},",
                commodities[cid].id, commodities[cid].name, quantity, commodities[cid].price);
        strcat(all_commodities, iterator);
    }
    all_commodities[strlen(all_commodities) - 1] = ']';
    //printf("all_commodities: %s\n", all_commodities);
    
    h2o_iovec_t body = h2o_strdup(&req->pool, all_commodities, SIZE_MAX);
    req->res.status = 200;
    req->res.reason = "OK";
    h2o_add_header(&req->pool, &req->res.headers, H2O_TOKEN_CONTENT_TYPE, NULL, H2O_STRLIT("application/json"));
    h2o_start_response(req, &generator);
    h2o_send(req, &body, 1, 1);

    return 0;
}

static int seckill(h2o_handler_t *self, h2o_req_t *req) 
{
    static h2o_generator_t generator = {NULL, NULL};

    if (!h2o_memis(req->method.base, req->method.len, H2O_STRLIT("GET")))
        return -1;

    // parse user_id and commodity_id from url
    char user_id[MAX_LEN] = { 0 }, commodity_id[MAX_LEN] = { 0 };
    h2o_iovec_t status_list = { NULL, 0 };
    size_t para_len = sizeof("?user_id=") - 1;
    if ((req->query_at != SIZE_MAX) && ((req->path.len - req->query_at) > para_len)) {
        if (h2o_memis(&req->path.base[req->query_at], para_len, "?user_id=", para_len)) {
            status_list = h2o_iovec_init(&req->path.base[req->query_at + para_len], req->path.len - req->query_at - para_len);
            size_t i = 0;
            for (; i < status_list.len; i++) {
                if (status_list.base[i] == '&') {
                    strncpy(user_id, status_list.base, i);
                    // 14 is the length of "&commodity_id="
                    strncpy(commodity_id, status_list.base + 14 + i, status_list.len - 14 - i);
                    //printf("user_id = %s, commodity_id = %s\n", user_id, commodity_id);
                    break;
                }
            }
        }
    }

    char result[MSG_LEN] = { 0 };
    int cid = id2idx_cmdt(commodity_id), uid = id2idx_user(user_id) - 1, quantity;
    float price = commodities[cid].price, balance;

    lock(&locks[uid]);

    redisReply *reply;

    reply = (redisReply *)redisCommand(user_conn, "INCRBYFLOAT _u_%s -%f", user_id, price);
    balance = atof(reply->str);
    // printf("new balance%f\n", balance);
    freeReplyObject(reply);
    if (balance < 0) {
        reply = (redisReply *)redisCommand(user_conn, "INCRBYFLOAT _u_%s %f", user_id, price);
        freeReplyObject(reply);
        sprintf(result, "{\"result\":0,\"order_id\":\"Insufficient Balance\",\"user_id\":\"%s\"}", user_id);
        goto END;
    }


    reply = (redisReply *)redisCommand(commodity_conn, "DECR _c_%s", commodity_id);
    //printf("quantity: %lld\n", reply->integer);
    quantity = reply->integer;
    freeReplyObject(reply);
    if (quantity < 0) {
        sprintf(result, "{\"result\":0,\"order_id\":\"Failed\",\"user_id\":\"%s\"}", user_id);
    } else {
        time_t ts;
        time(&ts);
	    struct tm *tmp_time = localtime(&ts);
	    char tmp[100];
	    strftime(tmp, sizeof(tmp), "%04Y-%02m-%02d %H:%M:%S", tmp_time);
        //printf("timestamp: %ld\n", ts);
        atomic_ullong oid = atomic_fetch_add(&_order_id, 1); 
        reply = (redisReply *)redisCommand(order_conn, "SET _o_%llu \"user_id\":\"%s\",\"commodity_id\":\"%s\",\"timestamp\":\"%s\"}", oid, user_id, commodity_id, tmp);
        freeReplyObject(reply);

        sprintf(result, "{\"result\":1,\"order_id\":%llu,\"user_id\":\"%s\",\"commodity_id\":\"%s\"}", oid, user_id, commodity_id);
        order_cache_entry *order_cacheline = &order_cache[hash(oid)];
        order_cacheline->id = 0;
        strncpy(order_cacheline->result, result, MSG_LEN);
        order_cacheline->id = oid;
    }

END:
    unlock(&locks[uid]);
   
    //printf("%s\n", result);
    h2o_iovec_t body = h2o_strdup(&req->pool, result, SIZE_MAX);
    req->res.status = 200;
    req->res.reason = "OK";
    h2o_add_header(&req->pool, &req->res.headers, H2O_TOKEN_CONTENT_TYPE, NULL, H2O_STRLIT("application/json"));
    h2o_start_response(req, &generator);
    h2o_send(req, &body, 1, 1);

    return 0;
}

static int get_order_by_id(h2o_handler_t *self, h2o_req_t *req)
{
    static h2o_generator_t generator = {NULL, NULL};

    if (!h2o_memis(req->method.base, req->method.len, H2O_STRLIT("GET")))
        return -1;

    char order_id[MAX_LEN] = { 0 };
    h2o_iovec_t status_list = { NULL, 0 };
    size_t para_len = sizeof("?order_id=") - 1;
    if ((req->query_at != SIZE_MAX) && ((req->path.len - req->query_at) > para_len)) {
        if (h2o_memis(&req->path.base[req->query_at], para_len, "?order_id=", para_len)) {
            status_list = h2o_iovec_init(&req->path.base[req->query_at + para_len], req->path.len - req->query_at - para_len);
            strncpy(order_id, status_list.base, status_list.len);
        }
    }

    char result[MSG_LEN] = { 0 };

    unsigned long long id = atoi(order_id);
    order_cache_entry *order_cacheline = &order_cache[hash(id)];
    if (order_cacheline->id == id) {
        strncpy(result, order_cacheline->result, MSG_LEN);
    }

    if (strstr(result, order_id) != result + strlen("{\"result\":1,\"order_id\":")) {
        redisReply *reply;
        reply = (redisReply *)redisCommand(order_conn, "GET _o_%s", order_id);
        sprintf(result, "{\"order_id\":\"%s\",%s", order_id, reply->str);
        freeReplyObject(reply);
    }
    
    h2o_iovec_t body = h2o_strdup(&req->pool, result, SIZE_MAX);
    req->res.status = 200;
    req->res.reason = "OK";
    h2o_add_header(&req->pool, &req->res.headers, H2O_TOKEN_CONTENT_TYPE, NULL, H2O_STRLIT("application/json"));
    h2o_start_response(req, &generator);
    h2o_send(req, &body, 1, 1);

    return 0;
}

static int get_order_all(h2o_handler_t *self, h2o_req_t *req)
{
    static h2o_generator_t generator = {NULL, NULL};

    if (!h2o_memis(req->method.base, req->method.len, H2O_STRLIT("GET")))
        return -1;

    unsigned long long oid = 1;
    memset(all_orders, 0, MAX_ALL);
    strcpy(all_orders, "[");
    char iterator[MSG_LEN];
    redisReply *reply;
    for (; oid < _order_id; oid++) {
        reply = (redisReply *)redisCommand(order_conn, "GET _o_%llu", oid);
        sprintf(iterator, "{\"order_id\":%llu,%s,", oid, reply->str);
        freeReplyObject(reply);
        strcat(all_orders, iterator);
    }
    all_orders[strlen(all_orders) - 1] = ']';
    //printf("all_users: %s\n", all_users);
    
    h2o_iovec_t body = h2o_strdup(&req->pool, all_orders, SIZE_MAX);
    req->res.status = 200;
    req->res.reason = "OK";
    h2o_add_header(&req->pool, &req->res.headers, H2O_TOKEN_CONTENT_TYPE, NULL, H2O_STRLIT("application/json"));
    h2o_start_response(req, &generator);
    h2o_send(req, &body, 1, 1);

    return 0;
}

static int verify(h2o_handler_t *self, h2o_req_t *req)
{
    static h2o_generator_t generator = {NULL, NULL};

    if (!h2o_memis(req->method.base, req->method.len, H2O_STRLIT("GET")))
        return -1;
    
    h2o_iovec_t body = h2o_strdup(&req->pool, "a829251e27c642582110677150b1f2ad", SIZE_MAX);
    req->res.status = 200;
    req->res.reason = "OK";
    h2o_add_header(&req->pool, &req->res.headers, H2O_TOKEN_CONTENT_TYPE, NULL, H2O_STRLIT("plain/text"));
    h2o_start_response(req, &generator);
    h2o_send(req, &body, 1, 1);

    return 0;
}

void swap(char *x, char *y) {
    char t[MAX_LEN];
    strcpy(t, x);
    strcpy(x, y);
    strcpy(y, t);
}

void sort() {
    int i, j;
    for (i = 0; i < userNum; i++)
        for (j = i + 1; j < userNum; j++)
            if (strcmp(users[i].id, users[j].id) > 0) {
                swap(users[i].id, users[j].id);
                swap(users[i].name, users[j].name);
            }
    for (i = 0; i < commodityNum; i++)
        for (j = i + 1; j < commodityNum; j++)
            if (strcmp(commodities[i].id, commodities[j].id) > 0) {
                swap(commodities[i].id, commodities[j].id);
                swap(commodities[i].name, commodities[j].name);

                float tmp = commodities[i].price;
                commodities[i].price = commodities[j].price;
                commodities[j].price = tmp;
            }
}

int data_init() {
    FILE *fp;
    if ((fp = fopen("./SecKill/input.txt", "r")) == NULL) {
        return -1;
    }

    size_t i;
    redisReply *reply;
    
    fscanf(fp, "%u\n", &userNum);
    memset(users, 0, sizeof(user_t) * MAX_NUM);
    for (i = 0; i < userNum; i++) {
        fscanf(fp, "%36[^,],%36[^,],%f\n", users[i].id, users[i].name, &users[i].balance);
        // prefix "_u_" means user
        reply = redisCommand(user_conn,"SET _u_%s %f", users[i].id, users[i].balance);
        freeReplyObject(reply);
    }
    
    fscanf(fp, "%u\n", &commodityNum);
    memset(commodities, 0, sizeof(commodity_t) * MAX_NUM);
    for (i = 0; i < commodityNum; i++) {
        fscanf(fp, "%36[^,],%36[^,],%d,%f\n", commodities[i].id, commodities[i].name, 
                &commodities[i].number, &commodities[i].price);
        // prefix "_c_" means user
        reply = redisCommand(commodity_conn,"SET _c_%s %u", commodities[i].id, commodities[i].number);
        freeReplyObject(reply);
    }

    memset(order_cache, 0, sizeof(order_cache));

    fclose(fp);
    sort();
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
    uv_ip4_addr("localhost", 80, &addr);
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
    h2o_hostconf_t *hostconf;

    signal(SIGPIPE, SIG_IGN);

    h2o_config_init(&config);
    hostconf = h2o_config_register_host(&config, h2o_iovec_init(H2O_STRLIT("default")), 65535);
    register_handler(hostconf, "/seckill/getUserById", get_user_by_id);
    register_handler(hostconf, "/seckill/getUserAll", get_user_all);
    register_handler(hostconf, "/seckill/getCommodityById", get_commodity_by_id);
    register_handler(hostconf, "/seckill/getCommodityAll", get_commodity_all);
    register_handler(hostconf, "/seckill/seckill", seckill);
    register_handler(hostconf, "/seckill/getOrderById", get_order_by_id);
    register_handler(hostconf, "/seckill/getOrderAll", get_order_all);
    register_handler(hostconf, "/a829251e27c642582110677150b1f2ad.txt", verify);

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

    struct timeval timeout = { 1, 500000 }; // 1.5 seconds
    user_conn = redisConnectWithTimeout("127.0.0.1", 6379, timeout);
    if (user_conn == NULL || user_conn->err) {
        if (user_conn) {
            printf("Connection error: %s\n", user_conn->errstr);
            redisFree(user_conn);
        } else {
            printf("Connection error: can't allocate redis context\n");
        }
        return 1;
    }
    commodity_conn = redisConnectWithTimeout("127.0.0.1", 6380, timeout);
    if (commodity_conn == NULL || commodity_conn->err) {
        if (commodity_conn) {
            printf("Connection error: %s\n", commodity_conn->errstr);
            redisFree(commodity_conn);
        } else {
            printf("Connection error: can't allocate redis context\n");
        }
        return 1;
    }
    order_conn = redisConnectWithTimeout("127.0.0.1", 6381, timeout);
    if (order_conn == NULL || order_conn->err) {
        if (order_conn) {
            printf("Connection error: %s\n", order_conn->errstr);
            redisFree(order_conn);
        } else {
            printf("Connection error: can't allocate redis context\n");
        }
        return 1;
    }
    if (data_init() < 0) {
        return 1;
    }

    uv_run(ctx.loop, UV_RUN_DEFAULT);

Error:
    /* Disconnects and frees the context */
    redisFree(user_conn);
    redisFree(commodity_conn);
    redisFree(order_conn);

    return 1;
}
