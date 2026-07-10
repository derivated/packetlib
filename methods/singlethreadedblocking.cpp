// compile_flags: -lcurl
#include <curl/curl.h>
#include <cstring>
#include <chrono>

extern "C" {
    struct response_timestamp {
        char* response;
        long long timestamp;
    };

    int completed = 0;

    size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) { 
        size_t real_size = size * (nmemb);
        response_timestamp *r_t = (response_timestamp*) userdata;

        long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        r_t->timestamp = ms;

        char *copied = new char[real_size + 1];

        std::memcpy(copied, ptr, real_size);

        copied[real_size] = '\0';

        r_t->response = copied;

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
        CURL *curl;
        CURLcode res;

        curl_global_init(CURL_GLOBAL_DEFAULT);

        curl = curl_easy_init();

        response_timestamp *r_ts[count] = {};

        if (curl) {
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(timeout_seconds));


            for (int i = 0; i < count; i++) {
                response_timestamp *r_t = new response_timestamp;
                r_t->timestamp = 0;
                r_t->response = nullptr;
                r_ts[i] = r_t;
                curl_easy_setopt(curl, CURLOPT_URL, endpoints[i]);
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, bodies[i]);
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, std::strlen(bodies[i]));

                // sets content type header to json
                struct curl_slist* headers = nullptr;
                headers = curl_slist_append(headers, "Content-Type: application/json");
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, r_ts[i]);

                res = curl_easy_perform(curl);
            }

            for (int i = 0; i < count; i++) {
                response_timestamp *r_t = r_ts[i];
                if (r_t->response == nullptr) {
                    timestamps[i] = -1;
                    response_lengths[i] = 11;
                    char *no_res_msg = "No Response\0";
                    std::memcpy(responses[i], no_res_msg, 11);
                    delete r_t;
                    continue;
                };

                timestamps[i] = r_t->timestamp;
                response_lengths[i] = std::strlen(r_t->response);
                std::memcpy(responses[i], r_t->response, response_lengths[i]);
                delete[] r_t->response;
                delete r_t;
            }

            //std::cout << std::endl;

            curl_easy_cleanup(curl);
        }

        curl_global_cleanup();
        return count;
    }
}
