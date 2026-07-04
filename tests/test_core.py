import unittest
import os
import shutil
import tempfile
from packetlib import execute, execute_full

class TestPacketLib(unittest.TestCase):
    def setUp(self):
        # We can use the mock.cpp file in methods directory
        self.methods_dir = os.path.join(os.path.dirname(os.path.dirname(__file__)), 'methods')
        self.assertTrue(os.path.isdir(self.methods_dir), "methods directory must exist")
        self.mock_cpp = os.path.join(self.methods_dir, 'mock.cpp')
        self.assertTrue(os.path.isfile(self.mock_cpp), "mock.cpp must exist")

    def test_execute_success(self):
        # Test execute with mock method
        endpoints = ["https://example.com/api/v1", "https://example.com/api/v2", "https://example.com/api/v3"]
        bodies = ["{}", "{}", "{}"]
        
        # Test basic execute (returns the JSON array of timestamps from the last response)
        result = execute("mock", endpoints, bodies, timeout_seconds=5, methods_dir=self.methods_dir)
        
        self.assertEqual(len(result), 3)
        self.assertTrue(all(isinstance(x, int) for x in result))
        # Check that the simulated timestamps are increasing by 100ms
        self.assertEqual(result[1] - result[0], 100)
        self.assertEqual(result[2] - result[1], 100)

    def test_execute_full(self):
        # Test execute_full with mock method
        endpoints = ["https://example.com/a", "https://example.com/b"]
        bodies = ["{\"x\": 1}", "{\"y\": 2}"]
        
        responses, timestamps = execute_full("mock", endpoints, bodies, timeout_seconds=5, methods_dir=self.methods_dir)
        
        self.assertEqual(len(responses), 2)
        self.assertEqual(len(timestamps), 2)
        
        # First response is a mock json object
        self.assertIn("status", responses[0])
        # Second response is the JSON array of timestamps
        self.assertTrue(responses[1].startswith("["))
        self.assertTrue(responses[1].endswith("]"))
        
        self.assertTrue(all(isinstance(x, int) for x in timestamps))
        self.assertEqual(timestamps[1] - timestamps[0], 100)

    def test_mismatched_lengths(self):
        endpoints = ["https://example.com/a"]
        bodies = []
        with self.assertRaises(ValueError):
            execute("mock", endpoints, bodies, methods_dir=self.methods_dir)

    def test_method_not_found(self):
        endpoints = ["https://example.com/a"]
        bodies = ["{}"]
        with self.assertRaises(FileNotFoundError):
            execute("nonexistent_method_xyz", endpoints, bodies, methods_dir=self.methods_dir)

if __name__ == '__main__':
    unittest.main()
