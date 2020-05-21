#include <jni.h>
#include <string>
#include <android/log.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include<errno.h>

#define TAG "native_backend"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

#define MSG_DATA_LENGTH 4096
using namespace std;
int sockfd;
struct Msg {
    int length;
    char type;
    char data[MSG_DATA_LENGTH];
};

bool ip_flag, tun_flag;
int tot_recv=0, tot_send=0;
int send_count = 0;
int recv_count = 0;

int tunfd = -1;
bool running;

pthread_mutex_t lock_time;
pthread_mutex_t lock_sockfd;


static time_t cur_time, last_beat, init_time;

static string packet101 = "";


extern "C" JNIEXPORT jstring JNICALL
Java_com_java_a4over6_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_java_a4over6_MainActivity_getflow(
        JNIEnv *env,
        jobject /* this */) {
    cur_time = time(nullptr);
    string ans = to_string(tot_send)+" "+to_string(send_count) + " "+
            to_string(tot_recv)+" "+to_string(recv_count)+ " "+to_string(cur_time-init_time);
//    LOGD("send back info: %s\n",ans.c_str());
    return env->NewStringUTF(ans.c_str());
}
extern "C" JNIEXPORT jstring JNICALL
Java_com_java_a4over6_MainActivity_getip(
        JNIEnv *env,
        jobject /* this */) {
    string result = to_string(sockfd) + " " + packet101;
    return env->NewStringUTF(result.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_java_a4over6_MainActivity_setTunfd(
        JNIEnv *env,
        jobject thiz,
        jint tun){
    tunfd = tun;
    tun_flag = true;
    LOGD("backend get tun");
    return env->NewStringUTF("finish");
}



int send_msg(int sockfd, struct Msg *msg, int length)
{
    pthread_mutex_lock(&lock_sockfd);
    long real_len = write(sockfd, (char*)msg, (size_t)length);
    pthread_mutex_unlock(&lock_sockfd);
    if (real_len < length) {
        LOGE("send_msg() error: length incompatible.\n");
        return -1;
    }
    return 0;
}

int send_ip_request_(int sockfd)
{
    struct Msg request_packet;
    request_packet.length = 5;
    request_packet.type = 100;
    return send_msg(sockfd, &request_packet, 5);
}


int sock_recv(int sockfd, char* buf, int n)
{
    int cur = 0, len;
    while (cur < n && running) {
        len = read(sockfd, buf + cur, n - cur);
        if (len <  0 ) {
            return -1;
        }
        cur += len;
    }
    if (cur > n) {
        LOGE("sock_recv error: len: %d > n: %d\n", cur, n);
    }
    return cur;
}
//extern "C"
//JNIEXPORT void JNICALL
//Java_com_java_a4over6_MainActivity_
void * readtun(void* arg)
{
    struct Msg tun_packet;
    LOGD("start read tun....");
    while (!tun_flag );
    LOGD("tun is ready");

    while (running ) {
        int  length = read(tunfd, tun_packet.data, sizeof(struct Msg)-5);

        if (length > 0) {
            tun_packet.length = length + 5;
            tun_packet.type = 102;
            send_msg(sockfd, &tun_packet, tun_packet.length);
            send_count += 1;
            tot_send += tun_packet.length;
        }else{
            if (errno==11){
                sleep(0);
            }else{
                LOGD("%s failed: %s (%d)\n", __FUNCTION__, strerror(errno), errno);
                return nullptr;
            }
        }
    }
    LOGD("read_tun() exit.\n");
    return nullptr;
}

//extern "C"
//JNIEXPORT void JNICALL
//Java_com_java_a4over6_MainActivity_
void* heartbeat(void* arg)
{

    int tmp = 0;
    struct Msg msg;
    msg.type = 104;
    msg.length = 5;
    while(!ip_flag);
    LOGD("start heartbeat......");
    while (running) {
        sleep(1);
        tmp++;
        cur_time = time(NULL);
        if (tmp <= 60) {
            if (tmp%20==0) {
//                LOGD("sending 104\n");
                send_msg(sockfd, &msg, 5);
//                LOGD("send 104 finish");
            }
        } else {

            if (cur_time-last_beat > 60){
                LOGE("connect time out, heartbeat thread exit.\n");
                running = false;
                return nullptr;
            }
        }
    }
    LOGD("heartbeat thread exit.\n");
    return nullptr;
}

//extern "C"
//JNIEXPORT void JNICALL
//Java_com_java_a4over6_MainActivity_
void* listens(void* arg){
    struct Msg r_packet;
    while (running) {  //running
        LOGD("receiving....");
        int size = sock_recv(sockfd, (char *)&r_packet, 5);
        if (size == -1){
            LOGE("receiver head failed\n");
            continue;
        }
        int data_size = sock_recv(sockfd, ((char *)&r_packet) + 5, r_packet.length - 5);
        if (data_size == -1){
            LOGE("receiver data failed\n");
            continue;
        }

        LOGD("recv packet type: %d\n", r_packet.type);
        LOGD("head_len: %d, data_len: %d\n", size, data_size);

        if (r_packet.type == 103) {
            int length = r_packet.length - sizeof(int) - sizeof(char);
            long write_len = write(tunfd, r_packet.data, (size_t)length);
            if (write_len != length) {
                LOGE("103 error. length incompatible.");
            }
            recv_count += 1;
            tot_recv += r_packet.length;
        }
        else if (r_packet.type == 104) {
            last_beat = time(nullptr);
        }
    }
    LOGD("stop listen\n");
    return  nullptr;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_java_a4over6_MainActivity_breakconnection(JNIEnv *env, jobject instance) {
    running = false;
    LOGD("destroy connection\n");
    close(sockfd);
}

// backend thread entrance
extern "C"
JNIEXPORT void JNICALL
Java_com_java_a4over6_MainActivity_buildconnection(JNIEnv *env, jobject instance, jstring addr_,
                                                 jstring port_)
{
    const char *addr = env->GetStringUTFChars(addr_, 0);
    const char *port = env->GetStringUTFChars(port_, 0);
    LOGD("backend thread start running...\n");

    ip_flag = false;
    tun_flag = false;
    running = true;
    cur_time = time(nullptr);
    last_beat = cur_time;
    init_time = cur_time;
    pthread_mutex_init(&lock_sockfd, nullptr);

    int ret;

    struct addrinfo hints;
    struct addrinfo *res;
    struct addrinfo *cur;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    LOGD("%s", addr);
    LOGD("%s", port);

    ret = getaddrinfo(addr, port, &hints, &res);
    if (ret != 0) {
        LOGE("getaddrinfo() failed.\n");
        goto out;
    }
    ret = -1;

    for (cur = res; cur != nullptr; cur = cur->ai_next) {
        sockfd = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
        LOGD("cur->family : %d, cur->socktype: %d, cur->proto : %d\n", cur->ai_family, cur->ai_socktype, cur->ai_protocol);
        LOGD("socketfd: %d\n",sockfd);
        if (sockfd < 0) {
            LOGD("socket() failed. %s\n", strerror(errno));
            continue;
        }
        LOGD("connecting....");
        ret = connect(sockfd, cur->ai_addr, cur->ai_addrlen);
        if (ret == 0) {
            // server connected
            break;
        } else {
            LOGD("connect() failed. %s\n",strerror(errno));
            close(sockfd);
        }
    }

    if (ret != 0) {
        LOGE("connect to server failed. backend thread exit.\n");
        goto out;
    }

    LOGD("connect complete!\n");

    ret = send_ip_request_(sockfd);
    if (ret != 0) {
        LOGE("send_ip_request_() failed.\n");
    } else {
        LOGD("send_ip_request() succeeded.\n");
    }
//    cur_time = time(nullptr);
//    last_beat = cur_time;
//    init_time = cur_time;

    struct Msg recv_packet;
    while (running) {  //running
        LOGD("receiving....");
        int size = sock_recv(sockfd, (char *)&recv_packet, 5);
        if (size == -1){
            LOGE("receiver head failed\n");
            continue;
        }
        int data_size = sock_recv(sockfd, ((char *)&recv_packet) + 5, recv_packet.length - 5);
        if (data_size == -1){
            LOGE("receiver data failed\n");
            continue;
        }
        LOGD("recv packet type: %d\n", recv_packet.type);
        LOGD("head_len: %d, data_len: %d\n", size, data_size);
        LOGD("data: %s\n",recv_packet.data);

        if (recv_packet.type == 101) {
            packet101 = recv_packet.data;
            ip_flag = true;
            LOGD("build connection success\n");
            break;
        }
    }
    pthread_t read_tun,heart_beat,listen_t;
    pthread_create(&read_tun, nullptr,readtun, nullptr);
    pthread_create(&heart_beat, nullptr,heartbeat, nullptr);
    pthread_create(&listen_t, nullptr,listens, nullptr);

//
    pthread_join(read_tun, nullptr);
    pthread_join(heart_beat, nullptr);
    pthread_join(listen_t, nullptr);
//
    close(sockfd);
    tot_recv = tot_send = 0;
    recv_count = send_count = 0;

    LOGD("backend thread successfully returned.\n");
    out:
    env->ReleaseStringUTFChars(addr_, addr);
    env->ReleaseStringUTFChars(port_, port);
    freeaddrinfo(res);
    close(sockfd);
//    LOGD("backend thread successfully returned.\n");
    return;
}