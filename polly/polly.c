#include "polly.h"

#include "matrix.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MY_MATRIX_ID "@polly:matrix.org"
#define IP "192.168.26.241"

struct hostport {
    char *host;
    int port;
    char *sessid;
};

struct hostport *hostport_from_sdp(const char *sdp, const char* type) {
    struct hostport *hostport = malloc(sizeof(struct hostport));
    hostport->host = NULL;
    hostport->port = 0;
    
    char *sdpcopy = strdup(sdp);
    char *token;
    
    while ((token = strsep(&sdpcopy, "\n")) != NULL) {
        if (strncmp(token, "o=", 2) == 0) {
            char *linetoken;
            int i = 0;
            while ((linetoken = strsep(&token, " ")) != NULL) {
                if (i == 1) {
                    hostport->sessid = linetoken;
                    break;
                }
                ++i;
            }
        } else if (strncmp(token, "c=IN IP4 ", 9) == 0) {
            hostport->host = token + 9;
            for (int i = 0; i < strlen(hostport->host); ++i) {
                if (hostport->host[i] == '\r' || hostport->host[i] == '\n') {
                    hostport->host[i] = '\0';
                }
            }
        } else if (strncmp(token, "m=", 2) == 0) {
            if (strncmp(token+2, type, strlen(type)) != 0) continue;
            
            char *linetoken;
            int i = 0;
            while ((linetoken = strsep(&token, " ")) != NULL) {
                if (i == 1) {
                    hostport->port = atoi(linetoken);
                    break;
                }
                ++i;
            }
        }
    }
    free(sdpcopy);
    return hostport;
}

char *mksdp(char *sessid, char *ip, int port) {
    char *ret = NULL;
    asprintf(&ret, "v=0\r\n\
o=- %s 0 IN IP4 127.0.0.1\r\n\
s=-\r\n\
c=IN IP4 %s\r\n\
t=0 0\r\n\
a=sendonly\r\n\
m=video %d RTP/AVP 99\r\n\
a=rtpmap:97 H264/90000\r\n", sessid, ip, port);
    return ret;
}

void on_matrix_event(matrix_event *ev, matrix_session *sess, void *userdata) {
    if (strcmp(ev->type, "m.room.member") == 0) {
        json_object *membership_obj = json_object_object_get(ev->content, "membership");
        if (membership_obj) {
            const char *membership = json_object_get_string(membership_obj);
            if (strcmp(membership, "invite") == 0 && strcmp(ev->state_key, MY_MATRIX_ID) == 0) {
                printf("Joining room: %s", ev->room_id);
                matrix_join_room(sess, ev->room_id);
            }
        }
    } else if (strcmp(ev->type, "m.call.invite") == 0) {
        printf("Got an invite: %s\n", json_object_to_json_string(ev->content));
        json_object *offer = json_object_object_get(ev->content, "offer");
        if (!offer) return;
        json_object *sdp_obj = json_object_object_get(offer, "sdp");
        if (!sdp_obj) return;
        
        json_object *call_id_obj = json_object_object_get(ev->content, "call_id");
        const char *call_id = NULL;
        if (call_id_obj) {
            call_id = json_object_get_string(call_id_obj);
        }
        
        const char *sdp = json_object_get_string(sdp_obj);
        printf("Got SDP: %s\n", sdp);
        
        struct hostport *hp = hostport_from_sdp(sdp, "video");
        printf("Host cand: %s %d\n", hp->host, hp->port);
        
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        struct sockaddr_in name;
        name.sin_family = AF_INET;
        name.sin_port = 0;
        name.sin_addr.s_addr = INADDR_ANY;
        memset(name.sin_zero, '\0', sizeof(name.sin_zero));
        bind(sock, (struct sockaddr *)&name, sizeof(name));
        
        socklen_t len = sizeof(struct sockaddr_in);
        getsockname(sock, (struct sockaddr *)&name, &len);
        
        int myport = ntohs(name.sin_port);
        char *outsdp = mksdp(hp->sessid, IP, myport);
        free(hp);
        
        printf("outbound sdp: %s\n", outsdp);
        
        json_object *answerobj = json_object_new_object();
        json_object_object_add(answerobj, "call_id", call_id_obj);
        json_object_object_add(answerobj, "version", json_object_new_int(0));
        
        json_object *offerobj = json_object_new_object();
        json_object_object_add(offerobj, "type", json_object_new_string("answer"));
        json_object_object_add(offerobj, "sdp", json_object_new_string(outsdp));
        json_object_object_add(answerobj, "answer", offerobj);
        
        matrix_send_event(sess, ev->room_id, "m.call.answer", answerobj);
        
        json_object_put(answerobj);
    } else if (strcmp(ev->type, "m.call.candidates") == 0) {
        printf("Got candidates: %s\n", json_object_to_json_string(ev->content));
    }
}

int main(int argc, char **argv) {
    matrix_session *mxsess = matrix_session_new("https://matrix.org");
    matrix_session_set_access_token(mxsess, "QHBvbGx5Om1hdHJpeC5vcmc..LIbiJzQUCbwKylrasG");
    matrix_event_stream_start(mxsess, on_matrix_event, NULL);

    while (1) {
        sleep(10);
    }
}
