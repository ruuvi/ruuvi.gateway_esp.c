#!/usr/bin/env python
# -*- coding: utf-8 -*-

import time
import datetime
from http.server import BaseHTTPRequestHandler, HTTPServer
import argparse
import os
from socketserver import ThreadingMixIn
import socketserver
import threading


def log(msg):
    cur_time = datetime.datetime.now()
    print(f'[{cur_time}] {msg}')


class ThreadedHTTPServer(ThreadingMixIn, HTTPServer):
    allow_reuse_address = True

    def shutdown(self):
        self.socket.close()
        HTTPServer.shutdown(self)


class HTTPRequestHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        file_name = self.path[1:]
        resp = b''
        if not os.path.isfile(file_name):
            log(f'GET {self.path} - not found')
            resp += f'HTTP/1.1 404 Not Found\r\n'.encode('ascii')
            resp += f'Content-Length: {0}\r\n'.encode('ascii')
            resp += f'\r\n'.encode('ascii')
            self.wfile.write(resp)
        else:
            with open(file_name, "rb") as fd:
                file_content = fd.read()
            file_len = len(file_content)
            log(f'GET {self.path} - found, len={file_len}')
            resp += f'HTTP/1.1 200 OK\r\n'.encode('ascii')
            resp += f'Content-Length: {len(file_content)}\r\n'.encode('ascii')
            resp += f'\r\n'.encode('ascii')
            self.wfile.write(resp)
            offset = 0
            while offset < file_len:
                chunk = file_content[offset:offset+4096]
                log(f'Write chunk: offset {offset} of {file_len}, len={len(chunk)}')
                self.wfile.write(chunk)
                offset += len(chunk)
                time.sleep(0.5)


class SimpleHttpServer(object):
    def __init__(self, ip, port):
        self.server_thread = None
        self.server = socketserver.BaseRequestHandler
        self.server = ThreadedHTTPServer((ip, port), HTTPRequestHandler)

    def start(self):
        self.server_thread = threading.Thread(target=self.server.serve_forever)
        self.server_thread.daemon = True
        self.server_thread.start()

    def wait_for_thread(self):
        self.server_thread.join()

    def stop(self):
        self.server.shutdown()
        self.wait_for_thread()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Slow HTTP Server')
    parser.add_argument('--port', type=int, help='Listening port for HTTP Server', default=7000)
    parser.add_argument('--ip', help='HTTP Server IP', default='0.0.0.0')
    args = parser.parse_args()

    server = SimpleHttpServer(args.ip, args.port)
    log(f'HTTP Server Running: IP:{args.ip}, port:{args.port}')
    server.start()
    server.wait_for_thread()
