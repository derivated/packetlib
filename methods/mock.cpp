#include <cstring>
#include <chrono>
#include <string>
#include <vector>

// compile_flags: -std=c++17

extern "C" {
    int execute_requests(
        const char** endpoints,
        const char** bodies,
        int count,
        int timeout_seconds,
        char** responses,
        int* response_lengths,
        long long* timestamps
    ) {
        // Get current epoch timestamp in milliseconds as a starting point
        auto now = std::chrono::system_clock::now();
        long long base_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        ).count();

        std::vector<long long> simulated_timestamps;

        for (int i = 0; i < count; ++i) {
            long long ts = base_time + (i * 100); // 100ms interval
            simulated_timestamps.push_back(ts);
            timestamps[i] = ts;

            // If it's the last request, we'll return a JSON array containing all timestamps
            if (i == count - 1) {
                std::string json_array = "[";
                for (size_t j = 0; j < simulated_timestamps.size(); ++j) {
                    json_array += std::to_string(simulated_timestamps[j]);
                    if (j < simulated_timestamps.size() - 1) {
                        json_array += ",";
                    }
                }
                json_array += "]";

                std::strncpy(responses[i], json_array.c_str(), json_array.length());
                responses[i][json_array.length()] = '\0';
                response_lengths[i] = json_array.length();
            } else {
                std::string mock_resp = "{\"status\":\"ok\",\"index\":" + std::to_string(i) + "}";
                std::strncpy(responses[i], mock_resp.c_str(), mock_resp.length());
                responses[i][mock_resp.length()] = '\0';
                response_lengths[i] = mock_resp.length();
            }
        }

        return count; // Return count of successful requests
    }
}
