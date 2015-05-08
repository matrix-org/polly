#include <curl/curl.h>

#include <pthread.h>

struct matrix_session {
    char *hs_url;
    pthread_t event_stream_thread;
    char *access_token;
    int run_event_stream;
    matrix_event_cb *event_cb;
    void *event_cb_userdata;
};

typedef struct curl_context {
    json_tokener *tokener;
    json_object *obj;
} curl_context;
