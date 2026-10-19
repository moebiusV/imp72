#!/usr/bin/env python3
"""oracle.py: drive TOPS-10 on the SIMH KA10 (oracle/boot.ini) over its telnet port.

    oracle.py [-p PORT] CMD...      log in, run each command, print what the system said

A command is sent as a line and the reply is read up to the next monitor prompt
("." at the start of a line) unless it begins with "!" (send the text raw, no wait).
Library use: Session().cmd("DIR") returns the text.
"""
import os, re, socket, sys, time

IAC, DONT, DO, WONT, WILL, SB, SE = 255, 254, 253, 252, 251, 250, 240


class Session:
    def __init__(self, port=10603, user=None, password=None, timeout=120):
        user = user or os.environ.get("ORACLE_USER", "100,100")           # the KA10 disk's account;
        password = password or os.environ.get("ORACLE_PASS", "demo1")    # the KI10 disk's is 1,2 / FAILSA
        self.port, self.timeout = port, timeout
        for attempt in range(8):            # a line just hung up may not be free yet
            try:
                self.s = socket.create_connection(("127.0.0.1", port), timeout=timeout)
                self._login(user, password)
                return
            except (TimeoutError, OSError):
                getattr(self, 's', socket.socket()).close()
                time.sleep(5)
        raise TimeoutError("could not log in to TOPS-10")

    def _read(self, wait=0.5):
        """read what arrives within `wait` seconds, answering telnet negotiation"""
        self.s.settimeout(wait)
        out = b""
        try:
            while True:
                d = self.s.recv(4096)
                if not d:
                    break
                out += d
        except socket.timeout:
            pass
        text = bytearray()
        i = 0
        while i < len(out):
            c = out[i]
            if c == IAC and i + 1 < len(out):
                v = out[i + 1]
                if v in (DO, DONT, WILL, WONT) and i + 2 < len(out):
                    reply = WONT if v == DO else DONT if v == WILL else None
                    if reply:
                        self.s.sendall(bytes([IAC, reply, out[i + 2]]))
                    i += 3
                    continue
                if v == SB:
                    j = out.find(bytes([IAC, SE]), i)
                    i = j + 2 if j >= 0 else len(out)
                    continue
                i += 2
                continue
            text.append(c & 0x7F)
            i += 1
        return text.decode("ascii", "replace").replace("\r", "")

    def send(self, data):
        """type slowly: the simulated terminal line drops characters sent in a burst"""
        if isinstance(data, str):
            data = data.encode()
        for i in range(len(data)):
            self.s.sendall(data[i:i + 1])
            time.sleep(0.03)

    def expect(self, pattern, timeout=None):
        end = time.time() + (timeout or self.timeout)
        got = ""
        rx = re.compile(pattern, re.M)
        while time.time() < end:
            got += self._read(0.3)
            if rx.search(got):
                return got
        raise TimeoutError("waiting for %r; got %r" % (pattern, got[-300:]))

    def _login(self, user, password):
        self._read(3.0)                     # the banner
        self.s.sendall(b"\x03\x03")         # whatever a lost session left running
        self._read(2.0)
        self.expect_after(b"\r", r"^\.", 20)
        self.send("login %s\r" % user)
        got = self.expect(r"Password:|[Aa]lready|\?", 20)
        if "Password:" in got:
            self.send(password + "\r")
            self.expect(r"^\.", 60)

    def expect_after(self, data, pattern, timeout):
        self.s.sendall(data)
        return self.expect(pattern, timeout)

    def cmd(self, line, prompt=r"^\.", timeout=None):
        self.send(line + "\r")
        return self.expect(prompt, timeout)

    def raw(self, data):
        self.s.sendall(data if isinstance(data, bytes) else data.encode())

    def close(self):
        """log out (^C out of whatever runs, KJOB with the F option), then hang up"""
        try:
            self.raw(b"\x03\x03")
            self._read(1.0)
            self.send("KJOB\r")
            self.expect(r"Confirm:", 15)
            self.send("F\r")
            self._read(3.0)
        except Exception:
            pass
        self.s.close()


def main(argv):
    port = 10603
    if argv[:1] == ["-p"]:
        port = int(argv[1])
        argv = argv[2:]
    t = Session(port)
    for c in argv:
        if c.startswith("!"):
            t.raw(c[1:] + "\r")
            print(t._read(1.0), end="")
        else:
            print(t.cmd(c), end="")
    t.close()


if __name__ == "__main__":
    main(sys.argv[1:])
