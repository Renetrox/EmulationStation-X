#!/usr/bin/env python3
"""Exercise the real ES-X HttpReq.cpp against a local HTTP server.

The full frontend depends on SDL/graphics libraries; this test builds HttpReq
with only its lightweight logging/file-system dependencies stubbed out.
It needs Python 3, a C++11 compiler, and libcurl development headers.
"""
import http.server
import pathlib
import shlex
import shutil
import subprocess
import tempfile
import threading
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class LocalHandler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.server.user_agents.append(self.headers.get("User-Agent", ""))
        if self.path == "/redirect":
            self.send_response(302)
            self.send_header("Location", "/200")
            self.end_headers()
            return
        code = int(self.path.lstrip("/"))
        self.send_response(code)
        self.end_headers()
        self.wfile.write(("response " + str(code)).encode("utf-8"))

    def log_message(self, *_args):
        pass


class HttpReqStatusTest(unittest.TestCase):
    @unittest.skipUnless(shutil.which("g++"), "g++ is required")
    @unittest.skipUnless(shutil.which("pkg-config"), "pkg-config is required")
    def test_http_status_and_user_agent(self):
        with tempfile.TemporaryDirectory() as directory:
            temp = pathlib.Path(directory)
            (temp / "utils").mkdir()
            shutil.copyfile(ROOT / "es-core/src/HttpReq.cpp", temp / "HttpReq.cpp")
            shutil.copyfile(ROOT / "es-core/src/HttpReq.h", temp / "HttpReq.h")
            (temp / "Log.h").write_text(
                "#pragma once\n#include <iostream>\n#define LogError 1\n"
                "#define LOG(level) std::cerr\n",
                encoding="utf-8",
            )
            (temp / "utils/FileSystemUtil.h").write_text(
                "#pragma once\n#include <string>\n"
                "namespace Utils { namespace FileSystem {\n"
                "inline bool exists(const std::string&) { return false; }\n"
                "}}\n",
                encoding="utf-8",
            )
            (temp / "test.cpp").write_text(
                r'''
#include "HttpReq.h"
#include <cassert>
#include <chrono>
#include <string>
#include <thread>

static void check(const std::string& url, bool shouldSucceed, long code) {
    HttpReq request(url);
    HttpReq::Status status = HttpReq::REQ_IN_PROGRESS;
    for (int attempt = 0; attempt < 1000; ++attempt) {
        status = request.status();
        if (status != HttpReq::REQ_IN_PROGRESS) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    assert(status != HttpReq::REQ_IN_PROGRESS);
    if (shouldSucceed) {
        assert(status == HttpReq::REQ_SUCCESS);
    } else {
        assert(status == HttpReq::REQ_BAD_STATUS_CODE);
        assert(request.getErrorMsg().find("HTTP status " + std::to_string(code))
               != std::string::npos);
    }
}

int main(int argc, char** argv) {
    assert(argc == 2);
    std::string base = argv[1];
    check(base + "/200", true, 200);
    check(base + "/204", true, 204);
    check(base + "/redirect", true, 200);
    check(base + "/401", false, 401);
    check(base + "/403", false, 403);
    check(base + "/404", false, 404);
    check(base + "/429", false, 429);
    check(base + "/500", false, 500);
}
''',
                encoding="utf-8",
            )
            flags = shlex.split(
                subprocess.check_output(
                    ["pkg-config", "--cflags", "--libs", "libcurl"], text=True
                )
            )
            subprocess.run(
                [
                    "g++", "-std=c++11", "-pthread", "-I", str(temp),
                    str(temp / "HttpReq.cpp"), str(temp / "test.cpp"),
                    "-o", str(temp / "http_req_test"), *flags,
                ],
                check=True, capture_output=True, text=True,
            )
            server = http.server.HTTPServer(("127.0.0.1", 0), LocalHandler)
            server.user_agents = []
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            try:
                subprocess.run(
                    [str(temp / "http_req_test"),
                     "http://127.0.0.1:" + str(server.server_port)],
                    check=True, capture_output=True, text=True, timeout=15,
                )
                self.assertEqual(len(server.user_agents), 9)
                self.assertTrue(
                    all("EmulationStation-X" in agent for agent in server.user_agents)
                )
            finally:
                server.shutdown()
                server.server_close()
                thread.join(timeout=2)


if __name__ == "__main__":
    unittest.main()
