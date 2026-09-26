#!/usr/bin/env python3
"""Verify the curl test server can bind without reverse DNS."""

import sys
import unittest
from unittest.mock import patch


class LoopbackServerTest(unittest.TestCase):
    def test_bind_without_reverse_dns(self):
        sys.dont_write_bytecode = True
        from curl_async_dns_server import Handler, LoopbackHTTPServer

        with patch("socket.getfqdn", side_effect=RuntimeError("reverse DNS is unavailable")):
            server = LoopbackHTTPServer(("127.0.0.1", 0), Handler)
        try:
            self.assertEqual(server.server_name, "localhost")
            self.assertEqual(server.server_port, server.server_address[1])
        finally:
            server.server_close()


if __name__ == "__main__":
    unittest.main()
