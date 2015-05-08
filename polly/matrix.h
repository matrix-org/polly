#include <json/json.h>

struct matrix_session;

typedef struct matrix_session matrix_session;

typedef struct matrix_event {
    const char *type;
    const char *room_id;
    const char *state_key;
    json_object *content;
} matrix_event;

typedef void (matrix_event_cb)(matrix_event *, matrix_session *, void *);

matrix_session *matrix_session_new(char *hs_url);
void matrix_session_destroy(matrix_session *sess);

void matrix_session_set_access_token(matrix_session *sess, char *access_token);

void matrix_event_stream_start(matrix_session *sess, matrix_event_cb cb, void *userdata);

int matrix_join_room(matrix_session *sess, const char *room_id);

int matrix_send_event(matrix_session *sess, const char *room_id, const char *type, json_object *event);