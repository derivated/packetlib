# packetlib

`packetlib` is a Python library designed to send bulk HTTPS requests using various methods. It provides a simple Python interface to orchestrate and execute highly optimized request-sending routines implemented in C++.

## LLM Use
This repository contains LLM-generated code. All C++ code except for mock.cpp is human-written.

## Features

- **High Performance**: Outsources the core networking logic to compiled C++ methods.
- **Extensible**: Easily add new C++ request-sending implementations (e.g., single-threaded, multi-threaded) by placing `.cpp` files in a `methods/` directory.
- **Automatic Compilation**: The library automatically compiles C++ source files into platform-specific shared libraries on-the-fly and loads them using Python's `ctypes`.
- **Flexible Compile Configuration**: Declare dependency link flags (e.g., `-lcurl`, `-lpthread`) directly inside C++ files as inline comments.

---

## Installation

Install the library locally by running:

```bash
pip install -e .
```

---

## Python API Usage

The library exposes two main functions: `execute` and `execute_full`.

### `execute(...)`
Executes the specified C++ request method and returns the received JSON array of times (parsed from the responses) as a list of integers.

```python
from packetlib import execute

# Define endpoints and request bodies
endpoints = [
    "https://127.0.0.1:8080/endpoint1",
    "https://127.0.0.1:8080/endpoint2",
    "https://127.0.0.1:8080/endpoint3"
]
bodies = [
    '{"request_id": 1}',
    '{"request_id": 2}',
    '{"request_id": 3}'
]

# Run the 'single_thread' C++ method (which corresponds to methods/single_thread.cpp)
timestamps = execute(
    method_name="single_thread",
    endpoints=endpoints,
    bodies=bodies,
    timeout_seconds=5
)

print("Received timestamps:", timestamps)
```

### `execute_full(...)`
If you need access to the raw response content as well as timestamps, use `execute_full` to return both lists:

```python
from packetlib import execute_full

responses, timestamps = execute_full(
    method_name="single_thread",
    endpoints=endpoints,
    bodies=bodies,
    timeout_seconds=5
)

for resp, ts in zip(responses, timestamps):
    print(f"Timestamp: {ts} -> Response: {resp}")
```

---

## C++ Implementation Contract

To add a new request-sending method, create a C++ source file in the `methods/` subdirectory of your working directory (e.g., `methods/my_custom_method.cpp`).

### 1. Function Signature
Your C++ file must export a single function named `execute_requests` with `extern "C"` linkage to avoid C++ name mangling:

```cpp
extern "C" {
    int execute_requests(
        const char** endpoints,
        const char** bodies,
        int count,
        int timeout_seconds,
        char** responses,
        int* response_lengths,
        long long* timestamps
    );
}
```

### 2. Argument Details
- `endpoints`: An array of `count` null-terminated C-strings representing the target HTTPS URLs.
- `bodies`: An array of `count` null-terminated C-strings representing the JSON request body payload for each endpoint.
- `count`: An integer specifying the number of requests to make.
- `timeout_seconds`: An integer specifying the request timeout limit.
- `responses`: An array of pre-allocated character buffers. The C++ implementation must write each response body into `responses[i]`. By default, buffers are sized to `1MB` (1,048,576 bytes) each.
- `response_lengths`: An array where the C++ implementation must store the actual length of each written response.
- `timestamps`: An array of `long long` where the C++ implementation must write the epoch timestamp (in milliseconds) when each response was received.
- **Return Value**: Returns the number of successfully completed requests (e.g., equal to `count` if all requests succeeded), or a negative integer representing a custom error code if the run failed.

### 3. Compilation Flags
By default, the library compiles all C++ files with `-O3 -shared -fPIC -std=c++17 -lcurl`. If your implementation requires additional compilation/link flags (such as `-lpthread`), you can declare them on a line starting with `// compile_flags:` at the top of your C++ file. The library will parse and append these flags to the compiler command:

```cpp
// compile_flags: -lpthread
```

---

## Example C++ Implementation

Below is a complete implementation example using `libcurl` (`methods/single_thread.cpp`):

```cpp
// compile_flags: -lcurl

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <cstring>
#include <curl/curl.h>

// Helper to write HTTP response data to a string buffer
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    std::string* s = static_cast<std::string*>(userp);
    s->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

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
        curl_global_init(CURL_GLOBAL_DEFAULT);
        int success_count = 0;

        for (int i = 0; i < count; ++i) {
            CURL* curl = curl_easy_init();
            if (!curl) {
                continue;
            }

            std::string read_buffer;

            // Set request options
            curl_easy_setopt(curl, CURLOPT_URL, endpoints[i]);
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, bodies[i]);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(timeout_seconds));
            
            // CRITICAL: Disable SSL verification for untrusted certificates
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

            // Set up write callbacks
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &read_buffer);

            // Set JSON headers
            struct curl_slist* headers = NULL;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

            // Execute the request
            CURLcode res = curl_easy_perform(curl);
            
            // Record timestamp upon response receipt
            auto now = std::chrono::system_clock::now();
            long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()
            ).count();
            timestamps[i] = ms;

            if (res == CURLE_OK) {
                // Copy response back to Python allocated buffer
                size_t to_copy = std::min(read_buffer.size(), size_t(1024 * 1024 - 1));
                std::memcpy(responses[i], read_buffer.c_str(), to_copy);
                responses[i][to_copy] = '\0';
                response_lengths[i] = static_cast<int>(to_copy);
                success_count++;
            } else {
                std::string err_msg = "Error: " + std::string(curl_easy_strerror(res));
                size_t to_copy = std::min(err_msg.size(), size_t(1024 * 1024 - 1));
                std::memcpy(responses[i], err_msg.c_str(), to_copy);
                responses[i][to_copy] = '\0';
                response_lengths[i] = static_cast<int>(to_copy);
            }

            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
        }

        curl_global_cleanup();
        return success_count;
    }
}
```
