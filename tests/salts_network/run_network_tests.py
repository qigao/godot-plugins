"""Start loopback protocol peers and run the Godot networking tests."""

import json
import socketserver
import subprocess
import sys
import threading
from contextlib import ExitStack
from http.server import BaseHTTPRequestHandler


def rpc_reply(data):
    request = json.loads(data)
    assert request["jsonrpc"] == "2.0"
    reply = {"jsonrpc": "2.0", "id": request["id"]}
    if request["method"] == "fail":
        reply["error"] = {"code": -32001, "message": "fixture error", "data": [True, None]}
    else:
        assert request["method"] == "echo"
        reply["result"] = request.get("params")
    return json.dumps(reply, ensure_ascii=False, separators=(",", ":")).encode()


class Http1Peer(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def do_POST(self):
        body = self.rfile.read(int(self.headers["Content-Length"]))
        rpc = self.path == "/rpc"
        response = rpc_reply(body) if rpc else body
        self.send_response(200)
        self.send_header("Content-Length", str(len(response)))
        self.send_header("Content-Type", "application/json" if rpc else "application/octet-stream")
        self.send_header("X-Repeat", "first")
        self.send_header("X-Repeat", "second")
        self.end_headers()
        self.wfile.write(response)

    def log_message(self, *args):
        pass


class Http2Peer(socketserver.StreamRequestHandler):
    def frame(self, kind, flags, stream, data=b""):
        self.wfile.write(len(data).to_bytes(3, "big") + bytes((kind, flags)) +
                         stream.to_bytes(4, "big") + data)

    def handle(self):
        assert self.rfile.read(24) == b"PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"
        self.frame(4, 0, 0)
        streams = {}
        try:
            while header := self.rfile.read(9):
                size = int.from_bytes(header[:3], "big")
                kind, flags = header[3:5]
                stream = int.from_bytes(header[5:9], "big")
                data = self.rfile.read(size)
                if kind == 4 and not flags & 1:
                    self.frame(4, 1, 0)
                elif kind == 1:
                    if self.server.hold:
                        continue
                    streams[stream] = bytearray()
                elif kind == 0 and stream in streams:
                    streams[stream].extend(data)
                if kind in (0, 1) and flags & 1 and stream in streams:
                    body = streams.pop(stream)
                    reply = rpc_reply(body) if body else b"h2"
                    # 0x88 indexes :status 200. 0x0f 0x10 selects content-type
                    # as a non-indexed field with a literal value.
                    content_type = b"application/json" if body else b"text/plain"
                    self.frame(1, 4, stream, b"\x88\x0f\x10" + bytes((len(content_type),)) + content_type)
                    self.frame(0, 1, stream, reply)
        except (ConnectionResetError, BrokenPipeError):
            pass


class MqttPeer(socketserver.StreamRequestHandler):
    def read_packet(self):
        header = self.rfile.read(1)
        if not header:
            return None, b""
        remaining = 0
        scale = 1
        for _ in range(4):
            value = self.rfile.read(1)[0]
            remaining += (value & 127) * scale
            if value < 128:
                return header[0], self.rfile.read(remaining)
            scale *= 128
        raise AssertionError("Invalid MQTT packet length")

    def packet(self, header, data):
        assert len(data) < 128
        self.wfile.write(bytes((header, len(data))) + data)

    def handle(self):
        header, data = self.read_packet()
        assert header == 0x10
        protocol_len = int.from_bytes(data[:2], "big")
        mqtt5 = data[2 + protocol_len] == 5
        self.packet(0x20, b"\x00\x00\x00" if mqtt5 else b"\x00\x00")
        while True:
            header, data = self.read_packet()
            if header is None or header == 0xe0:
                return
            if header == 0x82:
                self.packet(0x90, data[:2] + (b"\x00" if mqtt5 else b"") + b"\x01")
            elif header == 0xa2:
                self.packet(0xb0, data[:2] + (b"\x00\x00" if mqtt5 else b""))
            elif header == 0x32:
                topic_end = 2 + int.from_bytes(data[:2], "big")
                self.packet(0x40, data[topic_end:topic_end + 2])
                self.packet(0x32, data[:topic_end] + b"\x00\x44" + data[topic_end + 2:])
            elif header == 0xc0:
                self.packet(0xd0, b"")
            else:
                assert header == 0x40 and data[:2] == b"\x00\x44"


class PeerServer(socketserver.ThreadingTCPServer):
    daemon_threads = True

    def handle_error(self, request, client):
        super().handle_error(request, client)
        self.failed = True


def main():
    godot, project = sys.argv[1:]
    with ExitStack() as stack:
        servers = []
        for handler in (Http1Peer, Http2Peer, Http2Peer, MqttPeer):
            server = stack.enter_context(PeerServer(("127.0.0.1", 0), handler))
            server.failed = False
            server.hold = len(servers) == 2
            threading.Thread(target=server.serve_forever, daemon=True).start()
            stack.callback(server.shutdown)
            servers.append(server)
        command = [godot, "--headless", "--path", project, "--script", "res://network_tests.gd", "--"]
        result = subprocess.run(command + [str(server.server_address[1]) for server in servers],
                                timeout=35, check=False)
        return result.returncode or int(any(server.failed for server in servers))


if __name__ == "__main__":
    sys.exit(main())
