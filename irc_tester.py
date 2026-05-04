#!/usr/bin/env python3

#===========> How to test it <==============
# # With FD leak detection (recommended)
# python3 irc_tester.py 127.0.0.1 6667 pass --pid $(pgrep ircserv)

# # Enable random fuzzing after normal tests
# python3 irc_tester.py 127.0.0.1 6667 pass --fuzz

# # Repeat all tests 3 times (stress/regression)
# python3 irc_tester.py 127.0.0.1 6667 pass --replay 3

# # Save full log to file
# python3 irc_tester.py 127.0.0.1 6667 pass --log results.log

# # All options combined
# python3 irc_tester.py 127.0.0.1 6667 pass --pid $(pgrep ircserv) --fuzz --replay 2 --log full.log



"""
╔══════════════════════════════════════════════════════════════════════════╗
║              ft_irc Automated Tester — 42 Evaluation Suite              ║
║         Stress tests, edge cases, multi-client, FD leak detection       ║
╚══════════════════════════════════════════════════════════════════════════╝
Usage:
    python3 irc_tester.py [HOST] [PORT] [PASSWORD]
    python3 irc_tester.py                           # defaults: 127.0.0.1 6667 pass
    python3 irc_tester.py 127.0.0.1 6667 mypass
    python3 irc_tester.py --fuzz                    # enable random fuzzing mode
    python3 irc_tester.py --replay 3                # replay all tests 3 times
    python3 irc_tester.py --log tester.log          # write log to file
"""

import socket
import time
import threading
import sys
import os
import random
import string
import subprocess
import re
import argparse
import signal
import traceback
from datetime import datetime
from collections import defaultdict

# ─── ANSI Colors ────────────────────────────────────────────────────────────
GREEN  = "\033[1;32m"
RED    = "\033[1;31m"
YELLOW = "\033[1;33m"
BLUE   = "\033[1;34m"
CYAN   = "\033[1;36m"
BOLD   = "\033[1m"
RESET  = "\033[0m"

# ─── Config ──────────────────────────────────────────────────────────────────
DEFAULT_HOST     = "127.0.0.1"
DEFAULT_PORT     = 6667
DEFAULT_PASSWORD = "pass"
CONNECT_TIMEOUT  = 4.0
RECV_TIMEOUT     = 2.5
SLOW_RECV        = 0.3

# ─── Global counters ─────────────────────────────────────────────────────────
stats = {
    "total":    0,
    "passed":   0,
    "failed":   0,
    "warnings": 0,
    "crashes":  0,
    "fd_leaks": 0,
}
log_lines = []
_lock = threading.Lock()


# ════════════════════════════════════════════════════════════════════════════
#  LOGGING
# ════════════════════════════════════════════════════════════════════════════

def _ts():
    return datetime.now().strftime("%H:%M:%S.%f")[:-3]

def log(level, msg, extra=""):
    colors = {"PASS": GREEN, "FAIL": RED, "WARN": YELLOW,
              "INFO": BLUE,  "SECT": CYAN, "RAW": RESET}
    col = colors.get(level, RESET)
    tag = f"[{level}]".ljust(7)
    line = f"{col}{tag}{RESET} {msg}"
    if extra:
        line += f"  {YELLOW}({extra}){RESET}"
    with _lock:
        print(line)
        log_lines.append(f"[{_ts()}] {tag} {msg}" + (f"  ({extra})" if extra else ""))
    if level == "PASS":
        stats["passed"] += 1; stats["total"] += 1
    elif level == "FAIL":
        stats["failed"] += 1; stats["total"] += 1
    elif level == "WARN":
        stats["warnings"] += 1


def section(title):
    bar = "─" * 60
    print(f"\n{CYAN}{BOLD}{bar}")
    print(f"  {title}")
    print(f"{bar}{RESET}")
    log_lines.append(f"\n{'─'*60}\n  {title}\n{'─'*60}")


# ════════════════════════════════════════════════════════════════════════════
#  IRC CLIENT HELPER
# ════════════════════════════════════════════════════════════════════════════

class IRCClient:
    def __init__(self, host, port, timeout=RECV_TIMEOUT):
        self.host    = host
        self.port    = port
        self.timeout = timeout
        self.sock    = None
        self.buf     = ""
        self.fd      = -1

    def connect(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(CONNECT_TIMEOUT)
        self.sock.connect((self.host, self.port))
        self.sock.settimeout(self.timeout)
        self.fd = self.sock.fileno()
        return self

    def send(self, data):
        if not data.endswith("\r\n"):
            data += "\r\n"
        self.sock.sendall(data.encode())

    def recv_all(self, wait=SLOW_RECV):
        """Collect everything the server sends in `wait` seconds."""
        self.sock.settimeout(wait)
        lines = []
        try:
            while True:
                chunk = self.sock.recv(4096).decode(errors="replace")
                if not chunk:
                    break
                self.buf += chunk
        except socket.timeout:
            pass
        while "\n" in self.buf:
            idx = self.buf.index("\n")
            line = self.buf[:idx].rstrip("\r")
            self.buf = self.buf[idx+1:]
            if line:
                lines.append(line)
        self.sock.settimeout(self.timeout)
        return lines

    def expect(self, patterns, wait=SLOW_RECV):
        """Return (matched_pattern, lines). patterns can be str/list/regex."""
        lines = self.recv_all(wait)
        if isinstance(patterns, str):
            patterns = [patterns]
        for line in lines:
            for p in patterns:
                if re.search(p, line, re.IGNORECASE):
                    return p, lines
        return None, lines

    def auth(self, password, nick, user="testuser", realname="Test User"):
        self.send(f"PASS {password}")
        self.send(f"NICK {nick}")
        self.send(f"USER {user} 0 * :{realname}")
        time.sleep(SLOW_RECV)
        self.recv_all()

    def close(self):
        try:
            self.sock.close()
        except Exception:
            pass


def make_client(host, port, password, nick, user="testuser"):
    """Create and authenticate a client, return it."""
    c = IRCClient(host, port)
    c.connect()
    c.auth(password, nick, user)
    return c


# ════════════════════════════════════════════════════════════════════════════
#  CHECK helpers
# ════════════════════════════════════════════════════════════════════════════

def check(test_name, condition, got="", expected=""):
    extra = ""
    if got:
        extra = f"got: {repr(got[:120])}"
    if expected:
        extra += f"  expected: {repr(expected)}"
    if condition:
        log("PASS", test_name, extra)
    else:
        log("FAIL", test_name, extra)


def server_alive(host, port):
    try:
        s = socket.socket()
        s.settimeout(2)
        s.connect((host, port))
        s.close()
        return True
    except Exception:
        return False


# ════════════════════════════════════════════════════════════════════════════
#  TEST SUITES
# ════════════════════════════════════════════════════════════════════════════

# ── 1. Authentication ────────────────────────────────────────────────────────

def test_auth(host, port, password):
    section("1. AUTHENTICATION")

    # 1a. Correct password
    c = IRCClient(host, port).connect()
    c.send(f"PASS {password}")
    c.send("NICK authtest1")
    c.send("USER authtest1 0 * :Test")
    _, lines = c.expect("001")
    check("PASS correct → 001 welcome", any("001" in l for l in lines), str(lines))
    c.close()

    # 1b. Wrong password
    c = IRCClient(host, port).connect()
    c.send("PASS wrongpassword123")
    c.send("NICK authtest2")
    c.send("USER authtest2 0 * :Test")
    _, lines = c.expect("464")
    check("PASS wrong → 464 error", any("464" in l for l in lines), str(lines))
    c.close()

    # 1c. No password
    c = IRCClient(host, port).connect()
    c.send("NICK authtest3")
    c.send("USER authtest3 0 * :Test")
    _, lines = c.expect(["451", "461", "464"])
    check("No PASS → auth error", len(lines) > 0, str(lines))
    c.close()

    # 1d. Duplicate nick
    c1 = make_client(host, port, password, "dupnick")
    c2 = IRCClient(host, port).connect()
    c2.send(f"PASS {password}")
    c2.send("NICK dupnick")
    c2.send("USER dupnick2 0 * :Dup")
    _, lines = c2.expect("433")
    check("NICK duplicate → 433", any("433" in l for l in lines), str(lines))
    c1.close(); c2.close()

    # 1e. Empty NICK
    c = IRCClient(host, port).connect()
    c.send(f"PASS {password}")
    c.send("NICK")
    _, lines = c.expect(["431", "461"])
    check("NICK empty → 431/461", any(("431" in l or "461" in l) for l in lines), str(lines))
    c.close()

    # 1f. NICK too long (>9 chars)
    c = IRCClient(host, port).connect()
    c.send(f"PASS {password}")
    c.send("NICK " + "a" * 15)
    _, lines = c.expect("432")
    check("NICK too long (15 chars) → 432", any("432" in l for l in lines), str(lines))
    c.close()

    # 1g. NICK with invalid chars
    c = IRCClient(host, port).connect()
    c.send(f"PASS {password}")
    c.send("NICK bad!nick")
    _, lines = c.expect("432")
    check("NICK invalid chars → 432", any("432" in l for l in lines), str(lines))
    c.close()

    # 1h. NICK with spaces
    c = IRCClient(host, port).connect()
    c.send(f"PASS {password}")
    c.send("NICK bad nick")
    _, lines = c.expect(["431", "432", "461"])
    check("NICK with spaces → error", len(lines) > 0, str(lines))
    c.close()

    # 1i. USER missing params
    c = IRCClient(host, port).connect()
    c.send(f"PASS {password}")
    c.send("NICK usertest9")
    c.send("USER short")
    _, lines = c.expect("461")
    check("USER missing params → 461", any("461" in l for l in lines), str(lines))
    c.close()

    # 1j. Re-register (PASS after auth)
    c = make_client(host, port, password, "reregtst")
    c.send(f"PASS {password}")
    _, lines = c.expect("462")
    check("PASS after auth → 462", any("462" in l for l in lines), str(lines))
    c.close()


# ── 2. Channel Operations ─────────────────────────────────────────────────────

def test_channels(host, port, password):
    section("2. CHANNEL OPERATIONS")

    # 2a. JOIN new channel
    c1 = make_client(host, port, password, "chantest1")
    c1.send("JOIN #testchan1")
    _, lines = c1.expect(["JOIN", "332", "353", "366"])
    check("JOIN new channel → success", any("JOIN" in l or "353" in l for l in lines), str(lines))

    # 2b. JOIN existing channel
    c2 = make_client(host, port, password, "chantest2")
    c2.send("JOIN #testchan1")
    _, lines = c2.expect(["JOIN", "353"])
    check("JOIN existing channel → success", any("JOIN" in l or "353" in l for l in lines), str(lines))

    # 2c. JOIN channel without # prefix (should fail or succeed depending on impl)
    c3 = make_client(host, port, password, "chantest3")
    c3.send("JOIN testchan_nohash")
    lines = c3.recv_all(SLOW_RECV)
    # warn only — behavior is implementation-specific
    log("WARN", "JOIN without # prefix — server-defined behavior", str(lines[:2]))

    # 2d. JOIN multiple channels
    c4 = make_client(host, port, password, "chantest4")
    c4.send("JOIN #multi1,#multi2")
    _, lines = c4.expect(["JOIN", "353"])
    check("JOIN multiple channels comma-sep", any("JOIN" in l or "353" in l for l in lines), str(lines))

    # 2e. JOIN with password (+k)
    c5 = make_client(host, port, password, "chantest5")
    c5.send("JOIN #keychan")
    c5.recv_all()
    c5.send("MODE #keychan +k secret123")
    c5.recv_all()

    c6 = make_client(host, port, password, "chantest6")
    c6.send("JOIN #keychan wrongkey")
    _, lines = c6.expect("475")
    check("JOIN +k channel wrong key → 475", any("475" in l for l in lines), str(lines))

    c7 = make_client(host, port, password, "chantest7")
    c7.send("JOIN #keychan secret123")
    _, lines = c7.expect(["JOIN", "353"])
    check("JOIN +k channel correct key → success", any("JOIN" in l or "353" in l for l in lines), str(lines))

    # 2f. JOIN invite-only without invite
    c8 = make_client(host, port, password, "chantest8")
    c8.send("JOIN #invchan")
    c8.recv_all()
    c8.send("MODE #invchan +i")
    c8.recv_all()

    c9 = make_client(host, port, password, "chantest9")
    c9.send("JOIN #invchan")
    _, lines = c9.expect("473")
    check("JOIN +i channel without invite → 473", any("473" in l for l in lines), str(lines))

    # 2g. JOIN with limit (+l)
    c10 = make_client(host, port, password, "chantest10")
    c10.send("JOIN #limitchan")
    c10.recv_all()
    c10.send("MODE #limitchan +l 1")
    c10.recv_all()

    c11 = make_client(host, port, password, "chantest11")
    c11.send("JOIN #limitchan")
    _, lines = c11.expect("471")
    check("JOIN over +l limit → 471", any("471" in l for l in lines), str(lines))

    # Cleanup
    for cx in [c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11]:
        cx.close()


# ── 3. KICK ───────────────────────────────────────────────────────────────────

def test_kick(host, port, password):
    section("3. KICK COMMAND")

    op  = make_client(host, port, password, "kickop")
    usr = make_client(host, port, password, "kickusr")

    op.send("JOIN #kickchan")
    op.recv_all()
    usr.send("JOIN #kickchan")
    usr.recv_all()
    op.recv_all()  # consume join notification

    # Kick as operator
    op.send("KICK #kickchan kickusr :go away")
    _, lines = op.expect(["KICK"])
    check("KICK user from channel (operator)", any("KICK" in l for l in lines), str(lines))

    # Kick from non-member channel
    other = make_client(host, port, password, "kickoth")
    other.send("KICK #kickchan kickop :nope")
    _, lines = other.expect(["403", "442", "482"])
    check("KICK from channel not member → error", len(lines) > 0, str(lines))

    # Kick as non-operator
    usr2 = make_client(host, port, password, "kickusr2")
    usr2.send("JOIN #kickchan")
    usr2.recv_all()
    usr2.send("KICK #kickchan kickop :revolt")
    _, lines = usr2.expect(["482", "481"])
    check("KICK as non-operator → 482", any(("482" in l or "481" in l) for l in lines), str(lines))

    for c in [op, usr, other, usr2]:
        c.close()


# ── 4. INVITE ─────────────────────────────────────────────────────────────────

def test_invite(host, port, password):
    section("4. INVITE COMMAND")

    op   = make_client(host, port, password, "invop")
    usr  = make_client(host, port, password, "invusr")
    out  = make_client(host, port, password, "invout")

    op.send("JOIN #invchan2")
    op.recv_all()
    op.send("MODE #invchan2 +i")
    op.recv_all()

    # Invite user
    op.send("INVITE invusr #invchan2")
    _, lines = op.expect(["341", "INVITE"])
    check("INVITE user (operator) → 341/INVITE", len(lines) > 0, str(lines))

    # Invited user can join
    usr.send("JOIN #invchan2")
    _, lines = usr.expect(["JOIN", "353"])
    check("Invited user can JOIN +i channel", any("JOIN" in l or "353" in l for l in lines), str(lines))

    # Non-invited still blocked
    out.send("JOIN #invchan2")
    _, lines = out.expect("473")
    check("Non-invited still blocked from +i", any("473" in l for l in lines), str(lines))

    # Invite non-existent user
    op.send("INVITE nobody #invchan2")
    _, lines = op.expect("401")
    check("INVITE non-existent user → 401", any("401" in l for l in lines), str(lines))

    for c in [op, usr, out]:
        c.close()


# ── 5. TOPIC ──────────────────────────────────────────────────────────────────

def test_topic(host, port, password):
    section("5. TOPIC COMMAND")

    op  = make_client(host, port, password, "topicop")
    usr = make_client(host, port, password, "topicusr")

    op.send("JOIN #topicchan")
    op.recv_all()
    usr.send("JOIN #topicchan")
    usr.recv_all()

    # Operator sets topic
    op.send("TOPIC #topicchan :Hello World Topic")
    _, lines = op.expect(["332", "TOPIC"])
    check("TOPIC set by operator", len(lines) > 0, str(lines))

    # Enable +t (only ops can set)
    op.send("MODE #topicchan +t")
    op.recv_all()

    # Non-op tries to set topic with +t
    usr.send("TOPIC #topicchan :hacking the topic")
    _, lines = usr.expect(["482", "TOPIC"])
    check("TOPIC with +t by non-op → 482", any("482" in l for l in lines), str(lines))

    # Query topic
    usr.send("TOPIC #topicchan")
    _, lines = usr.expect(["332", "331"])
    check("TOPIC query → 332/331", any(("332" in l or "331" in l) for l in lines), str(lines))

    for c in [op, usr]:
        c.close()


# ── 6. MODE ───────────────────────────────────────────────────────────────────

def test_mode(host, port, password):
    section("6. MODE COMMAND")

    op  = make_client(host, port, password, "modeop")
    usr = make_client(host, port, password, "modeusr")
    usr2 = make_client(host, port, password, "modeusr2")

    op.send("JOIN #modechan")
    op.recv_all()
    usr.send("JOIN #modechan")
    usr.recv_all()

    modes_to_test = [
        ("+i", "-i", "invite-only"),
        ("+t", "-t", "topic-restricted"),
        ("+k secret", "-k", "key"),
    ]
    for set_m, unset_m, name in modes_to_test:
        op.send(f"MODE #modechan {set_m}")
        _, lines = op.expect("MODE")
        check(f"MODE {set_m} ({name}) broadcast", any("MODE" in l for l in lines), str(lines))

        op.send(f"MODE #modechan {unset_m}")
        op.recv_all()

    # +o (give operator)
    op.send("MODE #modechan +o modeusr")
    _, lines = op.expect("MODE")
    check("MODE +o grant operator", any("MODE" in l for l in lines), str(lines))

    # +l user limit
    op.send("MODE #modechan +l 5")
    _, lines = op.expect("MODE")
    check("MODE +l set user limit", any("MODE" in l for l in lines), str(lines))

    # Non-op tries MODE
    usr2.send("JOIN #modechan")
    usr2.recv_all()
    usr2.send("MODE #modechan +i")
    _, lines = usr2.expect("482")
    check("MODE by non-op → 482", any("482" in l for l in lines), str(lines))

    # Invalid MODE flag
    op.send("MODE #modechan +z")
    lines = op.recv_all()
    log("WARN", "MODE +z (invalid) server behavior", str(lines[:2]))

    for c in [op, usr, usr2]:
        c.close()


# ── 7. PRIVMSG ────────────────────────────────────────────────────────────────

def test_privmsg(host, port, password):
    section("7. PRIVMSG COMMAND")

    sender   = make_client(host, port, password, "msgsend")
    receiver = make_client(host, port, password, "msgrecv")

    # Direct user message
    sender.send("PRIVMSG msgrecv :Hello there!")
    _, lines = receiver.expect("PRIVMSG", wait=1.0)
    check("PRIVMSG user → received", any("PRIVMSG" in l for l in lines), str(lines))

    # Channel message
    sender.send("JOIN #msgchan")
    sender.recv_all()
    receiver.send("JOIN #msgchan")
    receiver.recv_all()

    sender.send("PRIVMSG #msgchan :channel message here")
    _, lines = receiver.expect("PRIVMSG", wait=1.0)
    check("PRIVMSG channel → broadcast received", any("PRIVMSG" in l for l in lines), str(lines))

    # Message to self (should work)
    sender.send("PRIVMSG msgsend :talking to myself")
    lines = sender.recv_all(SLOW_RECV)
    log("WARN", "PRIVMSG self — behavior is server-defined", str(lines[:2]))

    # Message to non-existent user
    sender.send("PRIVMSG ghost_user_xyz :hello?")
    _, lines = sender.expect("401")
    check("PRIVMSG unknown user → 401", any("401" in l for l in lines), str(lines))

    # Message to non-existent channel
    sender.send("PRIVMSG #nonexistchan :hello")
    _, lines = sender.expect("401")
    check("PRIVMSG unknown channel → 401", any("401" in l for l in lines), str(lines))

    # PRIVMSG empty body
    sender.send("PRIVMSG msgrecv")
    _, lines = sender.expect(["412", "461"])
    check("PRIVMSG no text → 412/461", any(("412" in l or "461" in l) for l in lines), str(lines))

    sender.close(); receiver.close()


# ── 8. QUIT ───────────────────────────────────────────────────────────────────

def test_quit(host, port, password):
    section("8. QUIT COMMAND")

    observer = make_client(host, port, password, "quitobs")
    quitter  = make_client(host, port, password, "quitter")

    observer.send("JOIN #quitchan")
    observer.recv_all()
    quitter.send("JOIN #quitchan")
    quitter.recv_all()
    observer.recv_all()  # drain join notification

    quitter.send("QUIT :bye everyone")
    time.sleep(0.5)
    lines = observer.recv_all(0.5)
    check("QUIT notifies channel members", any("QUIT" in l for l in lines), str(lines))
    check("Server still alive after QUIT", server_alive(host, port))

    quitter.close(); observer.close()


# ── 9. Multi-client interaction ───────────────────────────────────────────────

def test_multi_client(host, port, password):
    section("9. MULTI-CLIENT INTERACTION")

    N = 8
    clients = []
    nicks = [f"mc{i:02d}" for i in range(N)]
    try:
        for i, nick in enumerate(nicks):
            c = make_client(host, port, password, nick)
            clients.append(c)
        log("INFO", f"Created {N} simultaneous clients")

        # All join same channel
        for c in clients:
            c.send("JOIN #bigchan")
            c.recv_all()

        # Client 0 broadcasts
        clients[0].send("PRIVMSG #bigchan :hello everyone!")
        time.sleep(0.6)
        received = sum(1 for c in clients[1:] if any("PRIVMSG" in l for l in c.recv_all(0.2)))
        check(f"Broadcast to {N-1} clients", received >= N - 2, f"received by {received}/{N-1}")

        # Concurrent messaging
        def blast(c, n):
            for _ in range(n):
                c.send(f"PRIVMSG #bigchan :msg from {c}")
        threads = [threading.Thread(target=blast, args=(c, 5)) for c in clients[:4]]
        for t in threads: t.start()
        for t in threads: t.join()
        time.sleep(0.5)
        check("Concurrent messaging — no crash", server_alive(host, port))

        # Sequential join/part
        c_extra = make_client(host, port, password, "joinpart")
        for _ in range(5):
            c_extra.send("JOIN #bigchan")
            time.sleep(0.1)
            c_extra.send("PART #bigchan")
            time.sleep(0.1)
        c_extra.recv_all()
        check("Rapid JOIN/PART spam — no crash", server_alive(host, port))
        c_extra.close()

    finally:
        for c in clients:
            c.close()


# ── 10. Edge cases & stress ────────────────────────────────────────────────────

def test_edge_cases(host, port, password):
    section("10. EDGE CASES & STRESS TESTS")

    c = make_client(host, port, password, "edgecase")

    # Empty command
    c.send("")
    c.send("\r\n")
    check("Empty command — no crash", server_alive(host, port))

    # Only whitespace
    c.send("   \r\n")
    check("Whitespace-only command — no crash", server_alive(host, port))

    # Very long command (buffer overflow test)
    long_msg = "A" * 600
    c.send(f"PRIVMSG edgecase :{long_msg}")
    check("600-char message — no crash", server_alive(host, port))

    # 512+ byte line (should be dropped by server)
    huge = "X" * 520
    c.send(huge)
    time.sleep(0.3)
    check("520-byte line — server still alive", server_alive(host, port))

    # Unicode / non-ASCII
    c.send("PRIVMSG edgecase :héllo wörld 日本語 🔥")
    check("Unicode/UTF-8 input — no crash", server_alive(host, port))

    # Null bytes
    try:
        c.sock.sendall(b"PRIVMSG edgecase :hello\x00world\r\n")
    except Exception:
        pass
    check("Null byte in message — no crash", server_alive(host, port))

    # Rapid commands
    for i in range(50):
        c.send(f"PRIVMSG edgecase :spam{i}")
    time.sleep(0.5)
    check("50 rapid messages — no crash", server_alive(host, port))

    # Unknown command
    c.send("FOOBAR test")
    _, lines = c.expect("421")
    check("Unknown command → 421", any("421" in l for l in lines), str(lines))

    # Partial commands (no \r\n)
    try:
        c.sock.sendall(b"PRIVMSG")
        time.sleep(0.2)
        c.sock.sendall(b" edgecase :partial\r\n")
    except Exception:
        pass
    check("Partial command split across sends — no crash", server_alive(host, port))

    # PRIVMSG without being registered
    raw = IRCClient(host, port).connect()
    raw.send("PRIVMSG someone :hi")
    _, lines = raw.expect("451")
    check("Unregistered PRIVMSG → 451", any("451" in l for l in lines), str(lines))
    raw.close()

    c.close()


# ── 11. MODE edge cases ────────────────────────────────────────────────────────

def test_mode_edge(host, port, password):
    section("11. MODE EDGE CASES")

    op = make_client(host, port, password, "moded_op")
    op.send("JOIN #modeedge")
    op.recv_all()

    # +o self
    op.send("MODE #modeedge +o moded_op")
    lines = op.recv_all(SLOW_RECV)
    log("WARN", "MODE +o self — server-defined", str(lines[:1]))

    # MODE on non-existent channel
    op.send("MODE #ghostchan +i")
    _, lines = op.expect(["403", "442", "482"])
    check("MODE on non-existent channel → error", len(lines) > 0, str(lines))

    # MODE with no args
    op.send("MODE")
    lines = op.recv_all(SLOW_RECV)
    log("WARN", "MODE with no args — server-defined", str(lines[:1]))

    # MODE +k empty key
    op.send("MODE #modeedge +k")
    lines = op.recv_all(SLOW_RECV)
    log("WARN", "MODE +k with no key arg — server-defined", str(lines[:1]))

    # MODE +l negative limit
    op.send("MODE #modeedge +l -1")
    lines = op.recv_all(SLOW_RECV)
    log("WARN", "MODE +l negative — server-defined", str(lines[:1]))

    op.close()


# ── 12. Crash & stability ─────────────────────────────────────────────────────

def test_stability(host, port, password):
    section("12. CRASH & STABILITY DETECTION")

    # Connect/disconnect storm
    log("INFO", "Running connect/disconnect storm (20 clients)...")
    for _ in range(20):
        try:
            c = IRCClient(host, port).connect()
            c.close()
        except Exception:
            pass
    check("Connect/disconnect storm — server alive", server_alive(host, port))

    # Partial auth flood
    log("INFO", "Partial auth flood (15 clients)...")
    partial = []
    for i in range(15):
        try:
            c = IRCClient(host, port).connect()
            c.send(f"PASS {password}")
            partial.append(c)
        except Exception:
            pass
    time.sleep(0.5)
    check("Partial-auth flood — server alive", server_alive(host, port))
    for c in partial:
        c.close()

    # Message flood
    log("INFO", "Message flood from single client...")
    c = make_client(host, port, password, "floodcl")
    c.send("JOIN #floodtest")
    c.recv_all()
    for i in range(200):
        try:
            c.send(f"PRIVMSG #floodtest :flood{i}")
        except Exception:
            break
    time.sleep(0.5)
    check("Message flood — server alive", server_alive(host, port))
    c.close()

    # SIGPIPE (abrupt disconnect)
    log("INFO", "Testing abrupt disconnect (SIGPIPE handling)...")
    for _ in range(5):
        c = make_client(host, port, password, f"abrt{random.randint(0,9999)}")
        try:
            c.sock.close()  # abrupt close
        except Exception:
            pass
    time.sleep(0.4)
    check("Abrupt disconnects — server alive", server_alive(host, port))


# ── 13. INVITE flow ────────────────────────────────────────────────────────────

def test_full_flow(host, port, password):
    section("13. FULL REAL-WORLD CHAT SCENARIO")

    alice = make_client(host, port, password, "alice")
    bob   = make_client(host, port, password, "bob")
    carol = make_client(host, port, password, "carol")

    alice.send("JOIN #office")
    alice.recv_all()
    bob.send("JOIN #office")
    bob.recv_all()
    alice.recv_all()  # drain bob join notification

    alice.send("PRIVMSG #office :hi bob!")
    _, lines = bob.expect("PRIVMSG", wait=1.0)
    check("Scenario: alice→bob in channel", any("PRIVMSG" in l for l in lines), str(lines))

    alice.send("MODE #office +i")
    alice.recv_all()
    carol.send("JOIN #office")
    _, lines = carol.expect("473")
    check("Scenario: carol blocked from +i channel", any("473" in l for l in lines), str(lines))

    alice.send("INVITE carol #office")
    alice.recv_all()
    carol.send("JOIN #office")
    _, lines = carol.expect(["JOIN", "353"])
    check("Scenario: carol joins after invite", any("JOIN" in l or "353" in l for l in lines), str(lines))

    bob.send("TOPIC #office :standup time")
    alice.recv_all()
    check("Scenario: bob sets topic", server_alive(host, port))

    alice.send("KICK #office carol :too many people")
    _, lines = alice.expect("KICK")
    check("Scenario: alice kicks carol", any("KICK" in l for l in lines), str(lines))

    alice.send("MODE #office -i")
    alice.recv_all()
    carol.send("JOIN #office")
    _, lines = carol.expect(["JOIN", "353"])
    check("Scenario: carol rejoins after -i", any("JOIN" in l or "353" in l for l in lines), str(lines))

    for c in [alice, bob, carol]:
        c.close()


# ── 14. FD leak detection ─────────────────────────────────────────────────────

def test_fd_leaks(host, port, server_pid=None):
    section("14. FILE DESCRIPTOR LEAK DETECTION")

    if server_pid:
        fd_path = f"/proc/{server_pid}/fd"
        if os.path.exists(fd_path):
            before = len(os.listdir(fd_path))
            log("INFO", f"FD count before leak test: {before}")

            # Connect and immediately disconnect 20 times
            for _ in range(20):
                try:
                    s = socket.socket()
                    s.connect((host, port))
                    s.close()
                except Exception:
                    pass
            time.sleep(1.0)

            after = len(os.listdir(fd_path))
            log("INFO", f"FD count after leak test: {after}")
            leaked = max(0, after - before)
            if leaked > 3:
                log("FAIL", f"FD leak detected: {leaked} extra FDs")
                stats["fd_leaks"] += leaked
            else:
                log("PASS", f"No significant FD leaks ({leaked} delta)")
        else:
            log("WARN", f"/proc/{server_pid}/fd not accessible — skipping FD check")
    else:
        # Try lsof as fallback
        try:
            result = subprocess.run(
                ["lsof", "-i", f":{port}"],
                capture_output=True, text=True, timeout=5
            )
            lines = result.stdout.strip().splitlines()
            log("INFO", f"lsof shows {len(lines)-1} socket entries for port {port}")
        except Exception as e:
            log("WARN", f"lsof not available: {e}")


# ── 15. Fuzzing mode ──────────────────────────────────────────────────────────

def test_fuzz(host, port, password, rounds=30):
    section("15. RANDOM FUZZING MODE")

    cmds = ["PASS", "NICK", "USER", "JOIN", "PART", "PRIVMSG",
            "MODE", "KICK", "INVITE", "TOPIC", "QUIT", "PING",
            "PONG", "WHOIS", "LIST", "NAMES", "FOOBAR"]

    def rand_str(n=None):
        n = n or random.randint(1, 30)
        chars = string.printable.replace("\r", "").replace("\n", "")
        return "".join(random.choice(chars) for _ in range(n))

    c = IRCClient(host, port).connect()
    c.send(f"PASS {password}")
    c.send("NICK fuzzbot")
    c.send("USER fuzzbot 0 * :Fuzzer")
    c.recv_all()

    crashes = 0
    for i in range(rounds):
        cmd = random.choice(cmds)
        arg = rand_str()
        try:
            c.send(f"{cmd} {arg}")
            if not server_alive(host, port):
                crashes += 1
                log("FAIL", f"CRASH detected after fuzz round {i}: {cmd} {arg[:40]!r}")
                stats["crashes"] += 1
                break
        except Exception as e:
            log("WARN", f"Fuzz round {i} socket error: {e}")
            break

    if crashes == 0:
        log("PASS", f"Fuzzing ({rounds} rounds) — server stable")
    c.close()


# ── 16. BOT commands ──────────────────────────────────────────────────────────

def test_bot(host, port, password):
    section("16. BOT (Ka3kou3) — OPTIONAL")

    bot    = make_client(host, port, password, "BOT")
    caller = make_client(host, port, password, "botcall")

    # ka3kou3 /logtime
    caller.send("KA3KOU3 /logtime botcall")
    _, lines = caller.expect(["Ka3kou3", "logtime", "401"], wait=1.0)
    log("INFO", "BOT /logtime response", str(lines[:2]))

    # ka3kou3 /privmsg
    caller.send("KA3KOU3 /privmsg botcall")
    _, lines = caller.expect(["Ka3kou3", "PRIVMSG", "401"], wait=1.0)
    log("INFO", "BOT /privmsg response", str(lines[:2]))

    # unknown mode
    caller.send("KA3KOU3 /unknown botcall")
    _, lines = caller.expect(["Ka3kou3", "accept"], wait=1.0)
    check("BOT invalid mode → error response", len(lines) > 0, str(lines))

    bot.close(); caller.close()


# ════════════════════════════════════════════════════════════════════════════
#  FINAL REPORT
# ════════════════════════════════════════════════════════════════════════════

def print_report(host, port):
    bar = "═" * 62
    print(f"\n{BOLD}{CYAN}{bar}")
    print(f"  {'ft_irc TESTER — FINAL REPORT':^58}")
    print(f"{bar}{RESET}")

    alive = server_alive(host, port)
    status = f"{GREEN}ALIVE{RESET}" if alive else f"{RED}DEAD{RESET}"
    print(f"  Server status  : {status}")
    print(f"  Total tests    : {BOLD}{stats['total']}{RESET}")
    print(f"  {GREEN}Passed         : {stats['passed']}{RESET}")
    print(f"  {RED}Failed         : {stats['failed']}{RESET}")
    print(f"  {YELLOW}Warnings       : {stats['warnings']}{RESET}")
    print(f"  {RED}Crashes        : {stats['crashes']}{RESET}")
    print(f"  {RED}FD leaks       : {stats['fd_leaks']}{RESET}")

    pct = (stats["passed"] / max(stats["total"], 1)) * 100
    bar_fill = int(pct / 2)
    bar_empty = 50 - bar_fill
    pbar = f"{GREEN}{'█' * bar_fill}{RESET}{'░' * bar_empty}"
    print(f"\n  Score: [{pbar}] {pct:.1f}%\n")
    print(f"{BOLD}{CYAN}{'═' * 62}{RESET}\n")


def write_log(path):
    with open(path, "w") as f:
        f.write(f"ft_irc Test Log — {datetime.now()}\n")
        f.write("=" * 70 + "\n\n")
        f.write("\n".join(log_lines))
        f.write("\n\n" + "=" * 70 + "\n")
        f.write(f"Total: {stats['total']}  Passed: {stats['passed']}  "
                f"Failed: {stats['failed']}  Warnings: {stats['warnings']}\n")
    print(f"{BLUE}[INFO]{RESET}  Log written to {path}")


# ════════════════════════════════════════════════════════════════════════════
#  ENTRY POINT
# ════════════════════════════════════════════════════════════════════════════

def main():
    parser = argparse.ArgumentParser(description="ft_irc automated tester")
    parser.add_argument("host",     nargs="?", default=DEFAULT_HOST)
    parser.add_argument("port",     nargs="?", type=int, default=DEFAULT_PORT)
    parser.add_argument("password", nargs="?", default=DEFAULT_PASSWORD)
    parser.add_argument("--pid",    type=int,  default=None, help="Server PID for FD leak detection")
    parser.add_argument("--fuzz",   action="store_true",     help="Enable fuzzing mode")
    parser.add_argument("--replay", type=int,  default=1,   help="Repeat all tests N times")
    parser.add_argument("--log",    default=None,            help="Write log to file")
    args = parser.parse_args()

    host, port, password = args.host, args.port, args.password

    print(f"\n{CYAN}{BOLD}")
    print("╔══════════════════════════════════════════════════════════════╗")
    print("║           ft_irc Automated Tester  —  42 School             ║")
    print("╚══════════════════════════════════════════════════════════════╝")
    print(f"{RESET}")
    print(f"  Target : {BOLD}{host}:{port}{RESET}   Password: {BOLD}{password}{RESET}")
    print(f"  Replay : {args.replay}x   Fuzz: {'ON' if args.fuzz else 'OFF'}\n")

    if not server_alive(host, port):
        print(f"{RED}{BOLD}  ERROR: Cannot connect to {host}:{port} — is the server running?{RESET}\n")
        sys.exit(1)

    suites = [
        test_auth,
        test_channels,
        test_kick,
        test_invite,
        test_topic,
        test_mode,
        test_privmsg,
        test_quit,
        test_multi_client,
        test_edge_cases,
        test_mode_edge,
        test_stability,
        test_full_flow,
    ]

    for run in range(args.replay):
        if args.replay > 1:
            print(f"\n{YELLOW}{BOLD}── REPLAY {run+1}/{args.replay} ──{RESET}")
        for suite in suites:
            try:
                suite(host, port, password)
            except Exception as e:
                log("FAIL", f"Suite {suite.__name__} crashed: {e}")
                traceback.print_exc()
            if not server_alive(host, port):
                log("FAIL", "Server crashed mid-suite — stopping")
                stats["crashes"] += 1
                break

    # Extras
    test_fd_leaks(host, port, args.pid)
    try:
        test_bot(host, port, password)
    except Exception:
        pass
    if args.fuzz:
        test_fuzz(host, port, password)

    print_report(host, port)
    if args.log:
        write_log(args.log)


if __name__ == "__main__":
    main()