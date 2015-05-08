#include "matrix.h"
#include "matrix_private.h"

#include <stdlib.h>
#include <string.h>

size_t curl_data_cb(void *contents, size_t size, size_t nmemb, void *userp) {
    curl_context *cctx = (curl_context *)userp;

    cctx->obj = json_tokener_parse_ex(cctx->tokener, contents, (int)size * (int)nmemb);
    return size * nmemb;
}

void *event_stream_loop(void *arg) {
    matrix_session *sess = (matrix_session *)arg;

    curl_global_init(CURL_GLOBAL_ALL);
    CURL *curl = curl_easy_init();

    curl_context cctx;

    int have_fromtok = 0;
    char fromtok[512];
    cctx.tokener = json_tokener_new();

    matrix_event ev;

    while (sess->run_event_stream) {
        char fullurl[256];
        char *escaped_token = curl_easy_escape(curl, sess->access_token, 0);
        if (!have_fromtok) {
            snprintf(
                fullurl, 256,
                "%s/_matrix/client/api/v1/events?access_token=%s&timeout=0",
                sess->hs_url, escaped_token
            );
        } else {
            snprintf(
                fullurl, 256,
                "%s/_matrix/client/api/v1/events?access_token=%s&timeout=60000&from=%s",
                sess->hs_url, escaped_token, fromtok
            );
        }
        curl_free(escaped_token);

        printf("getting url %s\n", fullurl);

        curl_easy_setopt(curl, CURLOPT_URL, fullurl);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_data_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&cctx);
        CURLcode res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            if (cctx.obj) {
                json_object *chunk = json_object_object_get(cctx.obj, "chunk");
                for (int i = 0; i < json_object_array_length(chunk); ++i) {
                    json_object *event_obj = json_object_array_get_idx(chunk, i);
                    printf("got an event: %s\n\n", json_object_to_json_string(event_obj));

                    json_object *type_obj = json_object_object_get(event_obj, "type");
                    if (type_obj) {
                        ev.type = json_object_get_string(type_obj);
                    }
                    json_object *state_key_obj = json_object_object_get(event_obj, "state_key");
                    if (state_key_obj) {
                        ev.state_key = json_object_get_string(state_key_obj);
                    }
                    json_object *room_id_obj = json_object_object_get(event_obj, "room_id");
                    if (room_id_obj) {
                        ev.room_id = json_object_get_string(room_id_obj);
                    }
                    ev.content = json_object_object_get(event_obj, "content");

                    sess->event_cb(&ev, sess, sess->event_cb_userdata);
                }

                json_object *fromtok_obj = json_object_object_get(cctx.obj, "end");
                if (fromtok_obj) {
                    have_fromtok = 1;
                    strcpy(fromtok, json_object_get_string(fromtok_obj));
                }
                json_object_put(cctx.obj);
            }
        }
        json_tokener_reset(cctx.tokener);
    }
    json_tokener_free(cctx.tokener);

    curl_easy_cleanup(curl);

    return NULL;
}


matrix_session *matrix_session_new(char *hs_url) {
    matrix_session *sess = malloc(sizeof(matrix_session));
    sess->hs_url = strdup(hs_url);
    sess->access_token = NULL;
    return sess;
}

void matrix_session_destroy(matrix_session *sess) {
    free(sess->hs_url);
    free(sess->access_token);
    free(sess);
}

void matrix_session_set_access_token(matrix_session *sess, char *access_token) {
    sess->access_token = strdup(access_token);
}

void matrix_event_stream_start(matrix_session *sess, matrix_event_cb cb, void *userdata) {
    sess->event_cb = cb;
    sess->event_cb_userdata = userdata;
    sess->run_event_stream = 1;
    pthread_create(&sess->event_stream_thread, NULL, event_stream_loop, (void*)sess);
}

int matrix_join_room(matrix_session *sess, const char *room_id) {
    CURL *curl = curl_easy_init();

    curl_context cctx;
    cctx.tokener = json_tokener_new();

    char fullurl[256];
    char *escaped_token = curl_easy_escape(curl, sess->access_token, 0);
    snprintf(
        fullurl, 256,
        "%s/_matrix/client/api/v1/rooms/%s/join?access_token=%s",
        sess->hs_url, room_id, escaped_token
    );
    curl_free(escaped_token);

    curl_easy_setopt(curl, CURLOPT_URL, fullurl);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 2);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "{}");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_data_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&cctx);
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    CURLcode res = curl_easy_perform(curl);
    int success = 0;
    if (res == CURLE_OK) {
        success = 1;
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    json_tokener_free(cctx.tokener);

    return success;
}

int matrix_send_event(matrix_session *sess, const char *room_id, const char *type, json_object *content) {
    CURL *curl = curl_easy_init();
    
    curl_context cctx;
    cctx.tokener = json_tokener_new();
    
    char fullurl[256];
    char *escaped_token = curl_easy_escape(curl, sess->access_token, 0);
    snprintf(
             fullurl, 256,
             "%s/_matrix/client/api/v1/rooms/%s/send/%s?access_token=%s",
             sess->hs_url, room_id, type, escaped_token
             );
    curl_free(escaped_token);
    
    const char *datastr = json_object_to_json_string(content);
    
    curl_easy_setopt(curl, CURLOPT_URL, fullurl);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(datastr));
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, datastr);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_data_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&cctx);
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    CURLcode res = curl_easy_perform(curl);
    int success = 0;
    if (res == CURLE_OK) {
        success = 1;
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    json_tokener_free(cctx.tokener);
    
    return success;
}

