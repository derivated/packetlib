# Overview

Your task is to construct a Python library for a program which evaluates the amount of time between HTTPS requests received on a server. The library will contain a method that, when called, will ultimately send up to a few hundred HTTPS requests to a server, and returns a JSON array of the times for each request. The server, as well as the actual code for sending the requests, is outside of the scope of this task as the server is already written and the code for sending requests will be written in multiple implementations by the user in C++. The library should allow the user to select which implementation they would like to run, execute it, then return the received JSON array as a Python list of integers.

# Requirements

- Must be written primarily in Python, such that it can be ran as a Python function/method and imported as a library even though the user implementations will be written in C++ for perfomance reasons.
- Runs locally on the user's device.
- Allow the user to add C++ files to a "methods" subdirectory to add methods for sending requests from the server.
    - These methods should be called and given the following arguments:
        - A list of HTTPS endpoints, one for each request
        - A list of request bodies in JSON stored as a string, one for each request
        - The length of time the method should wait before timing out
    - These methods should return the following:
        - A list of the responses from the server
        - A list of the timestamps each response was received
    - Some examples of these different implementations include sending the HTTPS requests with one thread, or with multithreading. Note that you are not being asked to write these example implementations or any other implementations, you are simply providing  the interface for these implementations to be added.
- Give the user a method that allows them to select which request method they would like to use as an argument, execute it, and return the result as a Python list of integers.
- While the code for sending the HTTPS requests is under the purview of the extensible C++ part of the code and is thus out-of-scope, it may be important to know that the server does not have an SSL certificate and thus all HTTPS responses will need to be trusted without verification.
- Generate a README.md file outlining the project, as well as the contract for constructing the C++ methods