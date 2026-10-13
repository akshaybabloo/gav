import socket
import threading
import time

ADDRESS = ("127.0.0.1", 8555)
SDP = (
    "v=0\r\n"
    "o=- 0 0 IN IP4 127.0.0.1\r\n"
    "s=silent\r\n"
    "c=IN IP4 127.0.0.1\r\n"
    "t=0 0\r\n"
    "m=video 0 RTP/AVP 96\r\n"
    "a=rtpmap:96 H264/90000\r\n"
    "a=fmtp:96 packetization-mode=1;profile-level-id=42c01e\r\n"
    "a=control:trackID=0\r\n"
)


def log(started, text):
    print(f"+{time.monotonic() - started:6.2f}s {text}", flush=True)


def reply(method, headers):
    if method == "OPTIONS":
        return "Public: OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, TEARDOWN, GET_PARAMETER\r\n", ""
    if method == "DESCRIBE":
        base = f"rtsp://{ADDRESS[0]}:{ADDRESS[1]}/s/"
        return f"Content-Type: application/sdp\r\nContent-Base: {base}\r\nContent-Length: {len(SDP)}\r\n", SDP
    if method == "SETUP":
        transport = headers.get("transport", "")
        if "TCP" in transport:
            return "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\nSession: 1;timeout=60\r\n", ""
        return f"Transport: {transport};server_port=9000-9001\r\nSession: 1;timeout=60\r\n", ""
    if method == "PLAY":
        return "Session: 1\r\nRange: npt=0.000-\r\n", ""
    return "Session: 1\r\n", ""


def serve(connection):
    started = time.monotonic()
    log(started, "connection")
    pending = b""
    while True:
        try:
            data = connection.recv(4096)
        except OSError:
            break
        if not data:
            break
        pending += data
        while b"\r\n\r\n" in pending:
            head, pending = pending.split(b"\r\n\r\n", 1)
            lines = head.decode(errors="replace").split("\r\n")
            method = lines[0].split(" ")[0]
            headers = {}
            for line in lines[1:]:
                name, separator, value = line.partition(":")
                if separator:
                    headers[name.strip().lower()] = value.strip()
            if method == "SETUP":
                log(started, f"SETUP {headers.get('transport', '')}")
            else:
                log(started, method)
            extra, body = reply(method, headers)
            connection.sendall(f"RTSP/1.0 200 OK\r\nCSeq: {headers.get('cseq', '0')}\r\n{extra}\r\n{body}".encode())
    log(started, "closed")
    connection.close()


def main():
    with socket.socket() as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(ADDRESS)
        listener.listen()
        print(f"silent RTSP server on rtsp://{ADDRESS[0]}:{ADDRESS[1]}/s", flush=True)
        while True:
            connection, _ = listener.accept()
            threading.Thread(target=serve, args=(connection,), daemon=True).start()


if __name__ == "__main__":
    main()
