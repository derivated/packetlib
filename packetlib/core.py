import os
import sys
import ctypes
import ctypes.util
import json
import subprocess
from typing import List, Tuple, Optional

# Load libcurl globally into the address space to ensure its symbols are resolved
# for dynamically loaded C++ methods compiled with -lcurl.
def _load_libcurl_globally() -> None:
    mode = ctypes.DEFAULT_MODE
    if hasattr(ctypes, 'RTLD_GLOBAL'):
        mode = ctypes.RTLD_GLOBAL

    try:
        curl_path = ctypes.util.find_library('curl')
        if curl_path:
            ctypes.CDLL(curl_path, mode=mode)
            return
    except Exception:
        pass

    # Try common library names
    for name in ['libcurl.so', 'libcurl.so.4', 'libcurl.so.3', 'libcurl.dylib', 'libcurl.dll']:
        try:
            ctypes.CDLL(name, mode=mode)
            return
        except Exception:
            pass

_load_libcurl_globally()

def get_shared_lib_extension() -> str:
    """Return the platform-specific shared library file extension."""
    if sys.platform == 'win32':
        return '.dll'
    elif sys.platform == 'darwin':
        return '.dylib'
    else:
        return '.so'

def get_methods_dir(custom_dir: Optional[str] = None) -> str:
    """
    Resolve the directory containing the C++ method files.
    Searches:
    1. The custom_dir if provided.
    2. A directory named 'methods' in the current working directory.
    3. A directory named 'methods' inside the packetlib package.
    """
    if custom_dir:
        return os.path.abspath(custom_dir)
    
    # Search in current working directory
    cwd_methods = os.path.join(os.getcwd(), 'methods')
    if os.path.isdir(cwd_methods):
        return cwd_methods
        
    # Search inside the package
    package_methods = os.path.join(os.path.dirname(__file__), 'methods')
    if os.path.isdir(package_methods):
        return package_methods
        
    return cwd_methods

def compile_method(cpp_path: str, so_path: str, extra_flags: Optional[List[str]] = None) -> None:
    """
    Compile a C++ file into a shared library using g++.
    Reads extra compile flags from comments in the C++ file, e.g.:
    // compile_flags: -lcurl
    """
    file_flags = []
    try:
        with open(cpp_path, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                if line.startswith('// compile_flags:') or line.startswith('// compile-flags:'):
                    parts = line.split(':', 1)[1].strip()
                    file_flags.extend(parts.split())
    except Exception as e:
        print(f"Warning: Could not read compile flags from {cpp_path}: {e}", file=sys.stderr)

    # Base compilation command for shared libraries, linking libcurl by default
    cmd = ['g++', '-O3', '-shared', '-fPIC', '-std=c++17', '-lcurl']
    cmd.extend(file_flags)
    if extra_flags:
        cmd.extend(extra_flags)
    cmd.extend([cpp_path, '-o', so_path])

    print(f"Compiling {cpp_path} to {so_path}...")
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(
            f"Failed to compile C++ method {cpp_path}.\n"
            f"Command: {' '.join(cmd)}\n"
            f"Exit code: {result.returncode}\n"
            f"Stderr: {result.stderr}"
        )

def execute_full(
    method_name: str,
    endpoints: List[str],
    bodies: List[str],
    timeout_seconds: int = 10,
    max_response_size: int = 1024 * 1024,
    methods_dir: Optional[str] = None,
    force_recompile: bool = False,
    extra_compile_flags: Optional[List[str]] = None
) -> Tuple[List[str], List[int]]:
    """
    Call the specified C++ request method and return both:
    1. A list of response bodies from the server.
    2. A list of timestamps (in milliseconds) each response was received.
    """
    if len(endpoints) != len(bodies):
        raise ValueError("endpoints and bodies lists must have the same length")
        
    count = len(endpoints)
    if count == 0:
        return [], []

    resolved_methods_dir = get_methods_dir(methods_dir)
    cpp_path = os.path.join(resolved_methods_dir, f"{method_name}.cpp")
    so_path = os.path.join(resolved_methods_dir, f"{method_name}{get_shared_lib_extension()}")

    if not os.path.exists(cpp_path):
        raise FileNotFoundError(f"C++ source file not found: {cpp_path}")

    # Compile if needed (source is newer or shared library is missing)
    needs_compile = force_recompile or not os.path.exists(so_path)
    if not needs_compile:
        cpp_mtime = os.path.getmtime(cpp_path)
        so_mtime = os.path.getmtime(so_path)
        if cpp_mtime > so_mtime:
            needs_compile = True

    if needs_compile:
        compile_method(cpp_path, so_path, extra_compile_flags)

    # Load the library using ctypes
    try:
        lib = ctypes.CDLL(so_path)
    except Exception as e:
        raise RuntimeError(f"Failed to load shared library {so_path}: {e}")

    # Define the argument and return types
    # int execute_requests(const char** endpoints, const char** bodies, int count,
    #                      int timeout_seconds, char** responses, int* response_lengths,
    #                      long long* timestamps)
    try:
        lib.execute_requests.argtypes = [
            ctypes.POINTER(ctypes.c_char_p),  # endpoints
            ctypes.POINTER(ctypes.c_char_p),  # bodies
            ctypes.c_int,                     # count
            ctypes.c_int,                     # timeout_seconds
            ctypes.POINTER(ctypes.c_char_p),  # responses
            ctypes.POINTER(ctypes.c_int),     # response_lengths
            ctypes.POINTER(ctypes.c_longlong) # timestamps
        ]
        lib.execute_requests.restype = ctypes.c_int
    except AttributeError:
        raise AttributeError(
            f"The compiled method {method_name} does not export the function 'execute_requests'. "
            "Make sure to declare it with extern \"C\" linkage."
        )

    # Convert Python lists to ctypes arrays
    endpoints_bytes = [e.encode('utf-8') for e in endpoints]
    bodies_bytes = [b.encode('utf-8') for b in bodies]

    endpoints_arr = (ctypes.c_char_p * count)(*endpoints_bytes)
    bodies_arr = (ctypes.c_char_p * count)(*bodies_bytes)

    # Allocate response buffers and output arrays
    response_buffers = [ctypes.create_string_buffer(max_response_size) for _ in range(count)]
    responses_arr = (ctypes.c_char_p * count)(*[ctypes.cast(buf, ctypes.c_char_p) for buf in response_buffers])
    response_lengths_arr = (ctypes.c_int * count)()
    timestamps_arr = (ctypes.c_longlong * count)()

    # Execute the C++ method
    ret = lib.execute_requests(
        endpoints_arr,
        bodies_arr,
        count,
        timeout_seconds,
        responses_arr,
        response_lengths_arr,
        timestamps_arr
    )

    if ret < 0:
        raise RuntimeError(f"C++ method '{method_name}' failed with return code {ret}")

    # Read and decode the response buffers
    responses = []
    for i in range(count):
        length = response_lengths_arr[i]
        if length < 0 or length > max_response_size:
            raise ValueError(f"Invalid response length {length} for request index {i}")
        resp_bytes = response_buffers[i].raw[:length]
        responses.append(resp_bytes.decode('utf-8', errors='replace'))

    timestamps = [int(timestamps_arr[i]) for i in range(count)]
    return responses, timestamps

def execute(
    method_name: str,
    endpoints: List[str],
    bodies: List[str],
    timeout_seconds: int = 10,
    max_response_size: int = 1024 * 1024,
    methods_dir: Optional[str] = None,
    force_recompile: bool = False,
    extra_compile_flags: Optional[List[str]] = None
) -> List[int]:
    """
    Execute the request method and return the result.
    It attempts to find and parse a JSON array of integers from the server responses.
    If none is found, it falls back to returning the list of timestamps each response was received.
    """
    responses, timestamps = execute_full(
        method_name=method_name,
        endpoints=endpoints,
        bodies=bodies,
        timeout_seconds=timeout_seconds,
        max_response_size=max_response_size,
        methods_dir=methods_dir,
        force_recompile=force_recompile,
        extra_compile_flags=extra_compile_flags
    )

    # Search for a JSON array of integers in responses (usually in the final/last response)
    for resp in reversed(responses):
        try:
            data = json.loads(resp)
            if isinstance(data, list) and all(isinstance(x, (int, float)) for x in data):
                return [int(x) for x in data]
        except Exception:
            continue

    # Fallback to timestamps from the C++ execution wrapper
    return timestamps
