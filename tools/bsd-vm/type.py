# type.py LINE...: type each line on the video console of the machine through the QEMU monitor (sendkey), then Enter.  An OpenBSD image of the cross-platform-actions project has no login on
# its serial port and its network card comes up without a carrier: a root login on the console (the password runner) is the way to bring it up.  Run inside the container (/vm/mon.sock).
import socket, sys, time

s = socket.socket(socket.AF_UNIX)
s.connect('/vm/mon.sock')
s.settimeout(0.3)


def mon(c):
    s.sendall(c.encode() + b'\r\n')
    time.sleep(0.04)
    try:
        s.recv(65536)
    except socket.timeout:
        pass


shifted = {'~': 'shift-grave_accent', '!': 'shift-1', '@': 'shift-2', '#': 'shift-3', '$': 'shift-4', '%': 'shift-5', '^': 'shift-6', '&': 'shift-7', '*': 'shift-8', '(': 'shift-9',
           ')': 'shift-0', '_': 'shift-minus', '+': 'shift-equal', '{': 'shift-bracket_left', '}': 'shift-bracket_right', '|': 'shift-backslash', ':': 'shift-semicolon',
           '"': 'shift-apostrophe', '<': 'shift-comma', '>': 'shift-dot', '?': 'shift-slash'}
plain = {' ': 'spc', '-': 'minus', '=': 'equal', '[': 'bracket_left', ']': 'bracket_right', '\\': 'backslash', ';': 'semicolon', "'": 'apostrophe', ',': 'comma', '.': 'dot',
         '/': 'slash', '`': 'grave_accent', '\t': 'tab'}


def key(ch):
    if ch in shifted:
        return shifted[ch]
    if ch in plain:
        return plain[ch]
    if ch.isupper():
        return 'shift-' + ch.lower()
    return ch


mon('')
for line in sys.argv[1:]:
    for ch in line:
        mon('sendkey ' + key(ch))
    mon('sendkey ret')
    time.sleep(0.5)
