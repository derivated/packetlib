#include <vector>
#include <thread>
#include <curl/curl.h>
#include <cstring>
#include <string>


extern "C" {
    struct response_timestamp {
        std::string response;
        long long timestamp;
    };

    volatile bool start = false;

    void launch_thread(CURL *handles[], int i) {
        CURL *handle;
        handle = handles[i];

        bool end = false;
        while (!end) {
            if (start) {
                CURLcode res = curl_easy_perform(handle);
                end = true;
            }
        }
    }

    size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {

        size_t real_size = size * (nmemb);
        response_timestamp *r_t = (response_timestamp*) userdata;

        long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        r_t->timestamp = ms;

        r_t->response.append(ptr,real_size);

        return real_size;
    }

    int execute_requests(
        const char** endpoints,
        const char** bodies,
        int count,
        int timeout_seconds,
        char** responses,
        int* response_lengths,
        long long* timestamps
    ) {
        CURLcode res;
        CURLSH *shared_tls;
        CURL *handles[count];

        std::vector<std::thread> threads;

        curl_global_init(CURL_GLOBAL_DEFAULT);
        shared_tls = curl_share_init();

        for (int i = 0; i < count; i++) {
            handles[i] = curl_easy_init();
        }

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        response_timestamp *r_ts[count] = {};

        if (handles[0]) {


            for (int i = 0; i < count; i++) {
                response_timestamp *r_t = new response_timestamp;
                r_t->timestamp = -1;
                r_ts[i] = r_t;
                curl_easy_setopt(handles[i], CURLOPT_WRITEDATA, r_ts[i]);

                curl_easy_setopt(handles[i], CURLOPT_URL, endpoints[i]);
                curl_easy_setopt(handles[i], CURLOPT_SSL_VERIFYPEER, 0L);
                curl_easy_setopt(handles[i], CURLOPT_SSL_VERIFYHOST, 0L);
                curl_easy_setopt(handles[i], CURLOPT_WRITEFUNCTION, write_callback);
                curl_easy_setopt(handles[i], CURLOPT_TIMEOUT, static_cast<long>(timeout_seconds));
                curl_easy_setopt(handles[i], CURLOPT_HTTPHEADER, headers);

                curl_easy_setopt(handles[i], CURLOPT_POSTFIELDS, bodies[i]);
                curl_easy_setopt(handles[i], CURLOPT_POSTFIELDSIZE, std::strlen(bodies[i]));

                // adds shared TLS cache
                curl_easy_setopt(handles[i], CURLOPT_SHARE, shared_tls);
            }

            for (int i = 0; i < count; i++) {
                threads.push_back(std::thread(launch_thread, &handles[0], i));
            }

            start = true;

            for (int i = 0; i < count; i++) {
                if (threads[i].joinable()) threads[i].join();
            }


            // test requests done

            for (int i = 0; i < count; i++) {
                response_timestamp *r_t = r_ts[i];
                if (r_t->timestamp == -1) {
                    timestamps[i] = -1;
                    response_lengths[i] = 11;
                    char *no_res_msg = "No Response\0";
                    std::memcpy(responses[i], no_res_msg, 11);
                    delete r_t;
                    continue;
                };
                
                timestamps[i] = r_t->timestamp;

                response_lengths[i] = r_t->response.size();

                std::memcpy(responses[i], r_t->response.c_str(), response_lengths[i]);

                delete r_t;

            }

            for (int i = 0; i < count; i++) {
                curl_easy_cleanup(handles[i]);
            }
        }
        
        curl_slist_free_all(headers);

        curl_share_cleanup(shared_tls);

        curl_global_cleanup();
        return count;
    }
}