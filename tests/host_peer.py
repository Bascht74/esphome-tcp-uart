"""Accept one TCP connection. ESPHome's host platform is the peer."""

import socket
import sys

server = socket.socket()
server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
server.bind(("127.0.0.1", 44502))
server.listen(1)
server.settimeout(12)
try:
    client, _ = server.accept()
except socket.timeout:
    sys.exit(1)
client.close()
server.close()
