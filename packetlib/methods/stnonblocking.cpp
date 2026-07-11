#include <iostream>
#include <curl/curl.h>
#include <cstring>
#include <chrono>
#include <string>


extern "C" {
    struct response_timestamp {
        std::string response;
        long long timestamp;
    };

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
        CURL *curl;
        CURLcode res;
        CURL *handles[count];

        curl_global_init(CURL_GLOBAL_DEFAULT);

        curl = curl_multi_init();
        for (int i = 0; i < count; i++) {
            handles[i] = curl_easy_init();
        }

        response_timestamp *r_ts[count] = {};

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        if (curl) {
            for (int i = 0; i < count; i++) {
                response_timestamp *r_t = new response_timestamp;
                r_t->timestamp = -1;
                r_ts[i] = r_t;
                
                curl_easy_setopt(handles[i], CURLOPT_WRITEFUNCTION, write_callback);
                curl_easy_setopt(handles[i], CURLOPT_WRITEDATA, r_ts[i]);
                curl_easy_setopt(handles[i], CURLOPT_TIMEOUT, static_cast<long>(timeout_seconds));

                curl_easy_setopt(handles[i], CURLOPT_URL, endpoints[i]);
                curl_easy_setopt(handles[i], CURLOPT_POSTFIELDS, bodies[i]);
                curl_easy_setopt(handles[i], CURLOPT_POSTFIELDSIZE, std::strlen(bodies[i]));

                // sets content type header to json
                
                curl_easy_setopt(handles[i], CURLOPT_HTTPHEADER, headers);
                curl_easy_setopt(handles[i], CURLOPT_SSL_VERIFYPEER, 0L);
                curl_easy_setopt(handles[i], CURLOPT_SSL_VERIFYHOST, 0L);
                curl_multi_add_handle(curl, handles[i]);
            }

            int still_running = true;

            while (still_running) {

                CURLMcode mc = curl_multi_perform(curl, &still_running);
                if (mc != CURLM_OK) {
                    std::cerr << "Multi perform failed code: " << mc << "\n";
                    break;
                }

                if (still_running) {
                    mc = curl_multi_poll(curl, nullptr, 0, 50, nullptr);
                    if (mc != CURLM_OK) {
                        std::cerr << "Multi poll failure code: " << mc << "\n";
                        break;
                    }
                }

            }


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

            // test requests done
            for (int i = 0; i < count; i++) {
                curl_multi_remove_handle(curl, handles[i]);
            }

            for (int i = 0; i < count; i++) {
                curl_easy_cleanup(handles[i]);
            }
            curl_multi_cleanup(curl);
        }

        curl_slist_free_all(headers);

        curl_global_cleanup();
        return count;
    }
    
}

/*
int main() {
        std::vector<std::string> vec_end = {"https://packetvm.linusreynolds.com/start"};
        for (int i = 0; i < 200; i++) {
            vec_end.push_back("https://packetvm.linusreynolds.com/test");
        }
        vec_end.push_back("https://packetvm.linusreynolds.com/end");

        std::vector<const char*> ptr_vec_end;

        for (const auto& str : vec_end) {
            ptr_vec_end.push_back(str.c_str()); 
        }
        //ptr_vec_end.push_back(nullptr);

        const char** endpoints = ptr_vec_end.data();

        std::vector<std::string> vec_bod = {"", ""};
        for (int i = 0; i < 201; i++) {
            vec_bod.push_back("");
        }
        std::vector<const char*> ptr_vec_bod;

        for (const auto& str : vec_bod) {
            ptr_vec_bod.push_back(str.c_str()); 
        }
        //ptr_vec_bod.push_back(nullptr);

        const char** bodies = ptr_vec_bod.data();

        char **responses = new char*[202];

        for (int i = 0; i < 202; i++) {
            responses[i] = new char[1000000];
        }

        int *response_lengths = new int[202];
        long long *timestamps = new long long[202];
 
        execute_requests(
            endpoints,
            bodies,
            202,
            30,
            responses,
            response_lengths,
            timestamps
        );

        for (int i = 0; i < 202; i++) {
            delete[] responses[i];
        }
        delete[] responses;
        delete[] response_lengths;
        delete[] timestamps;
    }

*/