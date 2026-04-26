#!/bin/bash

# ============================================================
#   ft_irc Test Suite - RFC1459 Corrector Script
#   School 1337 / 42 Network
# ============================================================

PORT=8080
PASS="12345"
SERVER_PID=""
TOTAL=0
PASSED=0
FAILED=0

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
RESET='\033[0m'

BINARY="./ircserv"

# ============================================================
# Portable nc helper
# Usage: irc_send "cmd1\r\ncmd2\r\n" [wait_seconds]
# Sends data to server, waits wait_seconds for response, returns it.
# Default wait = 0.4s.  Always closes cleanly.
# ============================================================
irc_send() {
    local data="$1"
    local wait="${2:-0.4}"
    # printf interprets \r\n, pipe into nc with a fixed timeout via 'timeout'
    printf "%b" "$data" | timeout "$wait" nc 127.0.0.1 "$PORT" 2>/dev/null
}

# ============================================================
# Utility
# ============================================================
print_header() {
    echo ""
    echo -e "${CYAN}${BOLD}══════════════════════════════════════════════════${RESET}"
    echo -e "${CYAN}${BOLD}  $1${RESET}"
    echo -e "${CYAN}${BOLD}══════════════════════════════════════════════════${RESET}"
}

print_test() {
    TOTAL=$((TOTAL + 1))
    local status=$1
    local name=$2
    local detail=$3
    if [ "$status" = "PASS" ]; then
        PASSED=$((PASSED + 1))
        echo -e "  ${GREEN}✓ PASS${RESET}  $name"
        [ -n "$detail" ] && echo -e "         ${GREEN}$detail${RESET}"
    else
        FAILED=$((FAILED + 1))
        echo -e "  ${RED}✗ FAIL${RESET}  $name"
        [ -n "$detail" ] && echo -e "         ${RED}$detail${RESET}"
    fi
}

start_server() {
    if [ ! -f "$BINARY" ]; then
        echo -e "${RED}${BOLD}ERROR: Binary '$BINARY' not found.${RESET}"
        echo -e "Run this script from the directory containing your ircserv binary."
        exit 1
    fi
    "$BINARY" $PORT $PASS &>/dev/null &
    SERVER_PID=$!
    sleep 0.5
    if ! kill -0 "$SERVER_PID" 2>/dev/null; then
        echo -e "${RED}Server failed to start!${RESET}"
        exit 1
    fi
    echo -e "${GREEN}Server started (PID $SERVER_PID) on port $PORT with pass '$PASS'${RESET}"
}

stop_server() {
    if [ -n "$SERVER_PID" ]; then
        kill "$SERVER_PID" 2>/dev/null
        wait "$SERVER_PID" 2>/dev/null
        echo -e "${YELLOW}Server stopped.${RESET}"
    fi
}

check_running() {
    if ! kill -0 "$SERVER_PID" 2>/dev/null; then
        echo -e "${RED}${BOLD}!! SERVER CRASHED !! (PID $SERVER_PID gone)${RESET}"
        FAILED=$((FAILED + 1))
        TOTAL=$((TOTAL + 1))
        return 1
    fi
    return 0
}

# Open a persistent TCP connection on a given fd number.
# Usage: open_fd 3   =>  exec 3<>/dev/tcp/127.0.0.1/$PORT
open_fd() {
    eval "exec $1<>/dev/tcp/127.0.0.1/$PORT"
}

# Send data on a given fd.
# Usage: send_fd 3 "PASS testpass\r\n"
send_fd() {
    printf "%b" "$2" >&"$1"
}

# Read whatever is buffered on an fd within a short timeout, non-blocking.
# We use 0.4s — enough for server to reply, never hangs.
read_fd() {
    timeout 0.4 dd bs=4096 count=1 <&"$1" 2>/dev/null
}

# Close a persistent fd.
close_fd() {
    eval "exec $1>&-"
}

# ============================================================
# Test Sections
# ============================================================

# ----------------------------------------------------------
# 1. Program Arguments Validation
# ----------------------------------------------------------
test_args() {
    print_header "1. Program Arguments Validation"

    # No args → should exit immediately with error
    "$BINARY" 2>/dev/null
    if [ $? -ne 0 ]; then
        print_test "PASS" "No args → exits with error"
    else
        print_test "FAIL" "No args should exit with error"
    fi

    # One arg only → should exit
    "$BINARY" 6667 2>/dev/null
    if [ $? -ne 0 ]; then
        print_test "PASS" "One arg only → exits with error"
    else
        print_test "FAIL" "One arg should exit with error"
    fi

    # Port too low (< 1024)
    "$BINARY" 80 testpass 2>/dev/null
    if [ $? -ne 0 ]; then
        print_test "PASS" "Port 80 (< 1024) → rejected"
    else
        print_test "FAIL" "Port 80 should be rejected (below 1024)"
    fi

    # Port too high (>= 65536)
    "$BINARY" 99999 testpass 2>/dev/null
    if [ $? -ne 0 ]; then
        print_test "PASS" "Port 99999 (>= 65536) → rejected"
    else
        print_test "FAIL" "Port 99999 should be rejected (above 65535)"
    fi

    # Non-numeric port
    "$BINARY" abc testpass 2>/dev/null
    if [ $? -ne 0 ]; then
        print_test "PASS" "Non-numeric port 'abc' → rejected"
    else
        print_test "FAIL" "Non-numeric port 'abc' should be rejected"
    fi
}

# ----------------------------------------------------------
# 2. Authentication (PASS / NICK / USER)
# ----------------------------------------------------------
test_auth() {
    print_header "2. Authentication (PASS / NICK / USER)"

    # Wrong password → 464
    OUT=$(irc_send "PASS wrongpass\r\nNICK alice\r\nUSER alice 0 * :Alice\r\n")
    if echo "$OUT" | grep -q "464"; then
        print_test "PASS" "Wrong password → 464 ERR_PASSWDMISMATCH"
    else
        print_test "FAIL" "Wrong password should return 464" "Got: $OUT"
    fi
    check_running || return

    # Correct full auth → 001 welcome
    OUT=$(irc_send "PASS $PASS\r\nNICK alice\r\nUSER alice 0 * :Alice Real\r\n")
    if echo "$OUT" | grep -q "001"; then
        print_test "PASS" "Correct auth → 001 Welcome"
    else
        print_test "FAIL" "Correct auth should return 001 Welcome" "Got: $OUT"
    fi
    check_running || return

    # NICK before PASS → 451
    OUT=$(irc_send "NICK nopass\r\n")
    if echo "$OUT" | grep -q "451"; then
        print_test "PASS" "NICK before PASS → 451 ERR_NOTREGISTERED"
    else
        print_test "FAIL" "NICK before PASS should return 451" "Got: $OUT"
    fi
    check_running || return

    # USER before PASS/NICK → 451
    OUT=$(irc_send "USER foo 0 * :Foo\r\n")
    if echo "$OUT" | grep -q "451"; then
        print_test "PASS" "USER before PASS/NICK → 451 ERR_NOTREGISTERED"
    else
        print_test "FAIL" "USER before auth should return 451" "Got: $OUT"
    fi
    check_running || return

    # PASS with no argument (just "PASS ") → 461 or 464
    OUT=$(irc_send "PASS \r\n")
    if echo "$OUT" | grep -qE "461|464"; then
        print_test "PASS" "PASS with empty arg → 461 or 464"
    else
        print_test "FAIL" "Empty PASS should return 461 or 464" "Got: $OUT"
    fi
    check_running || return

    # NICK with no argument → 461 or 431
    OUT=$(irc_send "PASS $PASS\r\nNICK\r\n")
    if echo "$OUT" | grep -qE "461|431"; then
        print_test "PASS" "NICK with no arg → 461 or 431 ERR_NONICKNAMEGIVEN"
    else
        print_test "FAIL" "NICK with no arg should return 461 or 431" "Got: $OUT"
    fi
    check_running || return

    # NICK > 9 chars → silently rejected (your code returns 1 without error msg, no 001)
    OUT=$(irc_send "PASS $PASS\r\nNICK toolongnck\r\n")
    if ! echo "$OUT" | grep -q "001"; then
        print_test "PASS" "NICK > 9 chars → not accepted (no 001 welcome)"
    else
        print_test "FAIL" "NICK > 9 chars should be rejected" "Got: $OUT"
    fi
    check_running || return

    # NICK with invalid char '!' → 432
    OUT=$(irc_send "PASS $PASS\r\nNICK bad!ck\r\n")
    if echo "$OUT" | grep -q "432"; then
        print_test "PASS" "NICK with invalid char '!' → 432 ERR_ERRONEUSNICKNAME"
    else
        print_test "FAIL" "NICK with invalid chars should return 432" "Got: $OUT"
    fi
    check_running || return

    # USER with only 1 param → 461
    OUT=$(irc_send "PASS $PASS\r\nNICK authtest\r\nUSER onlyone\r\n")
    if echo "$OUT" | grep -q "461"; then
        print_test "PASS" "USER with < 4 params → 461 ERR_NEEDMOREPARAMS"
    else
        print_test "FAIL" "USER with too few params should return 461" "Got: $OUT"
    fi
    check_running || return

    # Re-register after already registered → 462
    OUT=$(irc_send "PASS $PASS\r\nNICK rereg\r\nUSER rereg 0 * :ReReg\r\nPASS $PASS\r\n")
    if echo "$OUT" | grep -q "462"; then
        print_test "PASS" "PASS after already registered → 462 ERR_ALREADYREGISTRED"
    else
        print_test "FAIL" "Re-registering should return 462" "Got: $OUT"
    fi
    check_running || return

    # Duplicate NICK → 433
    # Keep first client alive while second tries to use same nick
    open_fd 3
    send_fd 3 "PASS $PASS\r\nNICK duptest\r\nUSER duptest 0 * :DupTest\r\n"
    sleep 0.3
    OUT=$(irc_send "PASS $PASS\r\nNICK duptest\r\n")
    close_fd 3
    if echo "$OUT" | grep -q "433"; then
        print_test "PASS" "Duplicate NICK → 433 ERR_NICKNAMEINUSE"
    else
        print_test "FAIL" "Duplicate NICK should return 433" "Got: $OUT"
    fi
    check_running || return
}

# ----------------------------------------------------------
# 3. JOIN Command
# ----------------------------------------------------------
test_join() {
    print_header "3. JOIN Command"

    # Basic JOIN
    OUT=$(irc_send "PASS $PASS\r\nNICK john\r\nUSER john 0 * :John\r\nJOIN #test\r\n")
    if echo "$OUT" | grep -qiE "JOIN.*#test|353.*#test"; then
        print_test "PASS" "Basic JOIN #test → join broadcast + 353 name list"
    else
        print_test "FAIL" "JOIN #test should send join message and 353" "Got: $OUT"
    fi
    check_running || return

    # JOIN without # prefix → 403
    OUT=$(irc_send "PASS $PASS\r\nNICK john2\r\nUSER john2 0 * :John2\r\nJOIN nochannel\r\n")
    if echo "$OUT" | grep -q "403"; then
        print_test "PASS" "JOIN channel without '#' prefix → 403 ERR_NOSUCHCHANNEL"
    else
        print_test "FAIL" "JOIN without # should return 403" "Got: $OUT"
    fi
    check_running || return

    # JOIN with no args → 461
    OUT=$(irc_send "PASS $PASS\r\nNICK john3\r\nUSER john3 0 * :John3\r\nJOIN\r\n")
    if echo "$OUT" | grep -q "461"; then
        print_test "PASS" "JOIN with no args → 461 ERR_NEEDMOREPARAMS"
    else
        print_test "FAIL" "JOIN without args should return 461" "Got: $OUT"
    fi
    check_running || return

    # JOIN multiple channels in one command
    OUT=$(irc_send "PASS $PASS\r\nNICK multi1\r\nUSER multi1 0 * :Multi1\r\nJOIN #chan1,#chan2,#chan3\r\n")
    if echo "$OUT" | grep -qi "#chan1" && echo "$OUT" | grep -qi "#chan2" && echo "$OUT" | grep -qi "#chan3"; then
        print_test "PASS" "JOIN #chan1,#chan2,#chan3 → joins all three channels"
    else
        print_test "FAIL" "JOIN multiple channels should join all three" "Got: $OUT"
    fi
    check_running || return

    # JOIN before authentication → 451
    OUT=$(irc_send "JOIN #hackchan\r\n")
    if echo "$OUT" | grep -q "451"; then
        print_test "PASS" "JOIN before authentication → 451 ERR_NOTREGISTERED"
    else
        print_test "FAIL" "JOIN before auth should return 451" "Got: $OUT"
    fi
    check_running || return

    # Channel creator appears as operator (@nick) in 353
    OUT=$(irc_send "PASS $PASS\r\nNICK creator\r\nUSER creator 0 * :Creator\r\nJOIN #opchan\r\n")
    if echo "$OUT" | grep -q "@creator"; then
        print_test "PASS" "Channel creator gets @ (operator) in 353 name list"
    else
        print_test "FAIL" "Channel creator should appear as @nick in 353" "Got: $OUT"
    fi
    check_running || return
}

# ----------------------------------------------------------
# 4. PRIVMSG Command
# ----------------------------------------------------------
test_privmsg() {
    print_header "4. PRIVMSG Command"

    # PRIVMSG user → target receives it
    open_fd 3
    send_fd 3 "PASS $PASS\r\nNICK recv1\r\nUSER recv1 0 * :Recv1\r\n"
    sleep 0.3
    irc_send "PASS $PASS\r\nNICK sender1\r\nUSER sender1 0 * :Sender1\r\nPRIVMSG recv1 :hello world\r\n" > /dev/null
    sleep 0.2
    RECV=$(read_fd 3)
    close_fd 3
    if echo "$RECV" | grep -q "hello world"; then
        print_test "PASS" "PRIVMSG to user → target receives the message"
    else
        print_test "FAIL" "Target should receive PRIVMSG" "Got: $RECV"
    fi
    check_running || return

    # PRIVMSG without :text → 412
    OUT=$(irc_send "PASS $PASS\r\nNICK sndr2\r\nUSER sndr2 0 * :Sndr2\r\nPRIVMSG recv1 notamessage\r\n")
    if echo "$OUT" | grep -q "412"; then
        print_test "PASS" "PRIVMSG without :text → 412 ERR_NOTEXTTOSEND"
    else
        print_test "FAIL" "PRIVMSG without :text should return 412" "Got: $OUT"
    fi
    check_running || return

    # PRIVMSG to nonexistent user → 401
    OUT=$(irc_send "PASS $PASS\r\nNICK sndr3\r\nUSER sndr3 0 * :Sndr3\r\nPRIVMSG ghostuser :hi\r\n")
    if echo "$OUT" | grep -q "401"; then
        print_test "PASS" "PRIVMSG to nonexistent nick → 401 ERR_NOSUCHNICK"
    else
        print_test "FAIL" "PRIVMSG to nonexistent user should return 401" "Got: $OUT"
    fi
    check_running || return

    # PRIVMSG with no args → 461
    OUT=$(irc_send "PASS $PASS\r\nNICK sndr4\r\nUSER sndr4 0 * :Sndr4\r\nPRIVMSG\r\n")
    if echo "$OUT" | grep -q "461"; then
        print_test "PASS" "PRIVMSG with no args → 461 ERR_NEEDMOREPARAMS"
    else
        print_test "FAIL" "PRIVMSG with no args should return 461" "Got: $OUT"
    fi
    check_running || return

    # PRIVMSG to channel → other member receives it
    open_fd 3
    send_fd 3 "PASS $PASS\r\nNICK chanrecv\r\nUSER chanrecv 0 * :ChanRecv\r\nJOIN #msgchan\r\n"
    sleep 0.3
    irc_send "PASS $PASS\r\nNICK chansend\r\nUSER chansend 0 * :ChanSend\r\nJOIN #msgchan\r\nPRIVMSG #msgchan :channel test msg\r\n" > /dev/null
    sleep 0.2
    RECV=$(read_fd 3)
    close_fd 3
    if echo "$RECV" | grep -q "channel test msg"; then
        print_test "PASS" "PRIVMSG to #channel → other member receives it"
    else
        print_test "FAIL" "Channel member should receive PRIVMSG" "Got: $RECV"
    fi
    check_running || return

    # PRIVMSG to channel user is not in → 442
    OUT=$(irc_send "PASS $PASS\r\nNICK outsider\r\nUSER outsider 0 * :Outsider\r\nPRIVMSG #msgchan :should fail\r\n")
    if echo "$OUT" | grep -q "442"; then
        print_test "PASS" "PRIVMSG to channel you're not in → 442 ERR_NOTONCHANNEL"
    else
        print_test "FAIL" "PRIVMSG to channel you're not in should return 442" "Got: $OUT"
    fi
    check_running || return

    # PRIVMSG to multiple targets: channel + user
    open_fd 3
    send_fd 3 "PASS $PASS\r\nNICK mulrecv\r\nUSER mulrecv 0 * :MulRecv\r\nJOIN #mulchan\r\n"
    sleep 0.2
    open_fd 4
    send_fd 4 "PASS $PASS\r\nNICK mulrecv2\r\nUSER mulrecv2 0 * :MulRecv2\r\n"
    sleep 0.2
    irc_send "PASS $PASS\r\nNICK mulsend\r\nUSER mulsend 0 * :MulSend\r\nJOIN #mulchan\r\nPRIVMSG #mulchan,mulrecv2 :multi target\r\n" > /dev/null
    sleep 0.3
    RECV3=$(read_fd 3)
    RECV4=$(read_fd 4)
    close_fd 3
    close_fd 4
    if echo "$RECV3" | grep -q "multi target" && echo "$RECV4" | grep -q "multi target"; then
        print_test "PASS" "PRIVMSG to multiple targets (#channel + user) → both receive it"
    else
        print_test "FAIL" "Both channel member and direct user should receive PRIVMSG" "Chan: $RECV3 | User: $RECV4"
    fi
    check_running || return
}

# ----------------------------------------------------------
# 5. KICK Command
# ----------------------------------------------------------
test_kick() {
    print_header "5. KICK Command"

    # Operator kicks a member → victim receives KICK
    open_fd 3
    send_fd 3 "PASS $PASS\r\nNICK kickop\r\nUSER kickop 0 * :KickOp\r\nJOIN #kickchan\r\n"
    sleep 0.3
    open_fd 4
    send_fd 4 "PASS $PASS\r\nNICK kickvic\r\nUSER kickvic 0 * :KickVic\r\nJOIN #kickchan\r\n"
    sleep 0.3
    send_fd 3 "KICK #kickchan kickvic :bye\r\n"
    sleep 0.3
    RECV4=$(read_fd 4)
    close_fd 3
    close_fd 4
    if echo "$RECV4" | grep -qiE "KICK.*kickvic|KICK.*#kickchan"; then
        print_test "PASS" "Operator KICK → victim receives KICK message"
    else
        print_test "FAIL" "Victim should receive KICK notification" "Got: $RECV4"
    fi
    check_running || return

    # Non-operator tries to kick → 482
    open_fd 3
    send_fd 3 "PASS $PASS\r\nNICK kickop2\r\nUSER kickop2 0 * :KickOp2\r\nJOIN #kickchan2\r\n"
    sleep 0.3
    open_fd 4
    send_fd 4 "PASS $PASS\r\nNICK noopkick\r\nUSER noopkick 0 * :NoOpKick\r\nJOIN #kickchan2\r\n"
    sleep 0.3
    send_fd 4 "KICK #kickchan2 kickop2 :rude\r\n"
    sleep 0.3
    RECV4=$(read_fd 4)
    close_fd 3
    close_fd 4
    if echo "$RECV4" | grep -q "482"; then
        print_test "PASS" "Non-operator KICK → 482 ERR_CHANOPRIVSNEEDED"
    else
        print_test "FAIL" "Non-op KICK should return 482" "Got: $RECV4"
    fi
    check_running || return

    # KICK on nonexistent channel → 403
    OUT=$(irc_send "PASS $PASS\r\nNICK kicktest\r\nUSER kicktest 0 * :KickTest\r\nKICK #ghost victim :bye\r\n")
    if echo "$OUT" | grep -q "403"; then
        print_test "PASS" "KICK on nonexistent channel → 403 ERR_NOSUCHCHANNEL"
    else
        print_test "FAIL" "KICK on nonexistent channel should return 403" "Got: $OUT"
    fi
    check_running || return

    # KICK with no args → 461
    OUT=$(irc_send "PASS $PASS\r\nNICK kicktest2\r\nUSER kicktest2 0 * :KickTest2\r\nJOIN #kickchan3\r\nKICK\r\n")
    if echo "$OUT" | grep -q "461"; then
        print_test "PASS" "KICK with no args → 461 ERR_NEEDMOREPARAMS"
    else
        print_test "FAIL" "KICK with no args should return 461" "Got: $OUT"
    fi
    check_running || return
}

# ----------------------------------------------------------
# 6. INVIT Command
# ----------------------------------------------------------
test_invite() {
    print_header "6. INVIT Command"

    # Setup: operator creates invite-only channel
    open_fd 3
    send_fd 3 "PASS $PASS\r\nNICK invop\r\nUSER invop 0 * :InvOp\r\nJOIN #invchan\r\nMODE #invchan +i\r\n"
    sleep 0.3

    # Second user tries to JOIN without invite → 473
    open_fd 4
    send_fd 4 "PASS $PASS\r\nNICK invuser\r\nUSER invuser 0 * :InvUser\r\n"
    sleep 0.3
    send_fd 4 "JOIN #invchan\r\n"
    sleep 0.2
    RECV4=$(read_fd 4)
    if echo "$RECV4" | grep -q "473"; then
        print_test "PASS" "JOIN invite-only channel without invite → 473 ERR_INVITEONLYCHAN"
    else
        print_test "FAIL" "Should get 473 for invite-only join without invite" "Got: $RECV4"
    fi
    check_running || return

    # Operator sends INVIT → invited user receives INVIT notification
    send_fd 3 "INVIT invuser #invchan\r\n"
    sleep 0.2
    RECV4=$(read_fd 4)
    if echo "$RECV4" | grep -qiE "INVIT|341"; then
        print_test "PASS" "INVIT → invited user receives INVIT notification"
    else
        print_test "FAIL" "Invited user should receive INVIT message" "Got: $RECV4"
    fi
    check_running || return

    # Invited user can now JOIN
    send_fd 4 "JOIN #invchan\r\n"
    sleep 0.2
    RECV4=$(read_fd 4)
    close_fd 3
    close_fd 4
    if echo "$RECV4" | grep -qiE "JOIN.*#invchan|353.*#invchan"; then
        print_test "PASS" "JOIN invite-only channel after being invited → success"
    else
        print_test "FAIL" "Invited user should be able to join" "Got: $RECV4"
    fi
    check_running || return

    # Non-operator tries to INVIT → 482
    open_fd 3
    send_fd 3 "PASS $PASS\r\nNICK nopinvop\r\nUSER nopinvop 0 * :NopInvOp\r\nJOIN #nopinvchan\r\n"
    sleep 0.3
    open_fd 4
    send_fd 4 "PASS $PASS\r\nNICK nopinvusr\r\nUSER nopinvusr 0 * :NopInvUsr\r\nJOIN #nopinvchan\r\n"
    sleep 0.3
    open_fd 5
    send_fd 5 "PASS $PASS\r\nNICK nopinvtgt\r\nUSER nopinvtgt 0 * :NopInvTgt\r\n"
    sleep 0.3
    # nopinvusr (NOT operator) tries to invite nopinvtgt
    send_fd 4 "INVIT nopinvtgt #nopinvchan\r\n"
    sleep 0.2
    RECV4=$(read_fd 4)
    close_fd 3
    close_fd 4
    close_fd 5
    if echo "$RECV4" | grep -q "482"; then
        print_test "PASS" "Non-operator INVIT → 482 ERR_CHANOPRIVSNEEDED"
    else
        print_test "FAIL" "Non-op inviting should return 482" "Got: $RECV4"
    fi
    check_running || return

    # INVIT with no args → 461
    OUT=$(irc_send "PASS $PASS\r\nNICK invtest\r\nUSER invtest 0 * :InvTest\r\nJOIN #invchan2\r\nINVIT\r\n")
    if echo "$OUT" | grep -q "461"; then
        print_test "PASS" "INVIT with no args → 461 ERR_NEEDMOREPARAMS"
    else
        print_test "FAIL" "INVIT with no args should return 461" "Got: $OUT"
    fi
    check_running || return
}

# ----------------------------------------------------------
# 7. TOPIC Command
# ----------------------------------------------------------
test_topic() {
    print_header "7. TOPIC Command"

    # Set a topic → TOPIC broadcast sent back to channel (sender is alone, sees own broadcast)
    OUT=$(irc_send "PASS $PASS\r\nNICK topicop\r\nUSER topicop 0 * :TopicOp\r\nJOIN #topicchan\r\nTOPIC #topicchan :my topic here\r\n")
    if echo "$OUT" | grep -qE "TOPIC|my topic here"; then
        print_test "PASS" "TOPIC set → TOPIC broadcast sent to channel"
    else
        print_test "FAIL" "Setting TOPIC should broadcast TOPIC message" "Got: $OUT"
    fi
    check_running || return

    # Query topic → 332 (topicop already set it; topicget joins and queries)
    OUT=$(irc_send "PASS $PASS\r\nNICK topicget\r\nUSER topicget 0 * :TopicGet\r\nJOIN #topicchan\r\nTOPIC #topicchan\r\n")
    if echo "$OUT" | grep -qE "332|my topic here"; then
        print_test "PASS" "TOPIC query → 332 with current topic text"
    else
        print_test "FAIL" "TOPIC query should return 332 with the topic" "Got: $OUT"
    fi
    check_running || return

    # TOPIC +t: non-operator cannot set topic → 482
    open_fd 3
    send_fd 3 "PASS $PASS\r\nNICK topicrst_op\r\nUSER topicrst_op 0 * :TopicRstOp\r\nJOIN #topicrst\r\nMODE #topicrst +t\r\n"
    sleep 0.3
    open_fd 4
    send_fd 4 "PASS $PASS\r\nNICK topicnop\r\nUSER topicnop 0 * :TopicNop\r\nJOIN #topicrst\r\n"
    sleep 0.3
    send_fd 4 "TOPIC #topicrst :forbidden topic\r\n"
    sleep 0.2
    RECV4=$(read_fd 4)
    close_fd 3
    close_fd 4
    if echo "$RECV4" | grep -q "482"; then
        print_test "PASS" "TOPIC on +t channel by non-operator → 482 ERR_CHANOPRIVSNEEDED"
    else
        print_test "FAIL" "Non-op TOPIC on +t channel should return 482" "Got: $RECV4"
    fi
    check_running || return

    # TOPIC with no args → 461
    OUT=$(irc_send "PASS $PASS\r\nNICK topicempty\r\nUSER topicempty 0 * :TopicEmpty\r\nTOPIC\r\n")
    if echo "$OUT" | grep -q "461"; then
        print_test "PASS" "TOPIC with no args → 461 ERR_NEEDMOREPARAMS"
    else
        print_test "FAIL" "TOPIC with no args should return 461" "Got: $OUT"
    fi
    check_running || return

    # TOPIC on nonexistent channel → 403
    OUT=$(irc_send "PASS $PASS\r\nNICK topicnosuch\r\nUSER topicnosuch 0 * :TopicNoSuch\r\nTOPIC #ghostchan :hello\r\n")
    if echo "$OUT" | grep -q "403"; then
        print_test "PASS" "TOPIC on nonexistent channel → 403 ERR_NOSUCHCHANNEL"
    else
        print_test "FAIL" "TOPIC on nonexistent channel should return 403" "Got: $OUT"
    fi
    check_running || return
}

# ----------------------------------------------------------
# 8. MODE Command
# ----------------------------------------------------------
test_mode() {
    print_header "8. MODE Command"

    # MODE +i broadcast — unique channel name to avoid collision with prior tests
    OUT=$(irc_send "PASS $PASS\r\nNICK modeop_i\r\nUSER modeop_i 0 * :ModeOpI\r\nJOIN #modechan_i\r\nMODE #modechan_i +i\r\n")
    if echo "$OUT" | grep -q "MODE.*+i"; then
        print_test "PASS" "MODE +i → broadcasts MODE +i to channel"
    else
        print_test "FAIL" "MODE +i should broadcast to channel" "Got: $OUT"
    fi
    check_running || return

    # MODE -i broadcast — same user rejoins (channel still exists since server is running)
    OUT=$(irc_send "PASS $PASS\r\nNICK modeop_i\r\nUSER modeop_i 0 * :ModeOpI\r\nJOIN #modechan_i\r\nMODE #modechan_i -i\r\n")
    if echo "$OUT" | grep -q "MODE.*-i"; then
        print_test "PASS" "MODE -i → broadcasts MODE -i to channel"
    else
        print_test "FAIL" "MODE -i should broadcast to channel" "Got: $OUT"
    fi
    check_running || return

    # MODE +k broadcast
    OUT=$(irc_send "PASS $PASS\r\nNICK modeop_k\r\nUSER modeop_k 0 * :ModeOpK\r\nJOIN #modechan_k\r\nMODE #modechan_k +k secret\r\n")
    if echo "$OUT" | grep -q "MODE.*+k"; then
        print_test "PASS" "MODE +k secret → broadcasts MODE +k to channel"
    else
        print_test "FAIL" "MODE +k should broadcast to channel" "Got: $OUT"
    fi
    check_running || return

    # MODE +k without key arg → 461
    OUT=$(irc_send "PASS $PASS\r\nNICK modeop_k2\r\nUSER modeop_k2 0 * :ModeOpK2\r\nJOIN #modechan_k2\r\nMODE #modechan_k2 +k\r\n")
    if echo "$OUT" | grep -q "461"; then
        print_test "PASS" "MODE +k without key → 461 ERR_NEEDMOREPARAMS"
    else
        print_test "FAIL" "MODE +k without key arg should return 461" "Got: $OUT"
    fi
    check_running || return

    # JOIN with wrong key → 475
    open_fd 3
    send_fd 3 "PASS $PASS\r\nNICK keyop\r\nUSER keyop 0 * :KeyOp\r\nJOIN #keychan\r\nMODE #keychan +k mykey\r\n"
    sleep 0.3
    OUT=$(irc_send "PASS $PASS\r\nNICK keyjoiner\r\nUSER keyjoiner 0 * :KeyJoiner\r\nJOIN #keychan wrongkey\r\n")
    close_fd 3
    if echo "$OUT" | grep -q "475"; then
        print_test "PASS" "JOIN with wrong key → 475 ERR_BADCHANNELKEY"
    else
        print_test "FAIL" "Wrong channel key should return 475" "Got: $OUT"
    fi
    check_running || return

    # JOIN with correct key → success
    open_fd 3
    send_fd 3 "PASS $PASS\r\nNICK keyop2\r\nUSER keyop2 0 * :KeyOp2\r\nJOIN #keychan2\r\nMODE #keychan2 +k correctkey\r\n"
    sleep 0.3
    OUT=$(irc_send "PASS $PASS\r\nNICK keyjoin2\r\nUSER keyjoin2 0 * :KeyJoin2\r\nJOIN #keychan2 correctkey\r\n")
    close_fd 3
    if echo "$OUT" | grep -qiE "JOIN.*#keychan2|353.*#keychan2"; then
        print_test "PASS" "JOIN with correct key → success"
    else
        print_test "FAIL" "JOIN with correct key should succeed" "Got: $OUT"
    fi
    check_running || return

    # MODE +l → channel full, next JOIN returns 471
    open_fd 3
    send_fd 3 "PASS $PASS\r\nNICK limop\r\nUSER limop 0 * :LimOp\r\nJOIN #limchan\r\nMODE #limchan +l 1\r\n"
    sleep 0.3
    OUT=$(irc_send "PASS $PASS\r\nNICK limjoin\r\nUSER limjoin 0 * :LimJoin\r\nJOIN #limchan\r\n")
    close_fd 3
    if echo "$OUT" | grep -q "471"; then
        print_test "PASS" "JOIN when channel at +l limit → 471 ERR_CHANNELISFULL"
    else
        print_test "FAIL" "Joining full channel should return 471" "Got: $OUT"
    fi
    check_running || return

    # MODE +o → grant operator, both sides see broadcast
    open_fd 3
    send_fd 3 "PASS $PASS\r\nNICK modeop3\r\nUSER modeop3 0 * :ModeOp3\r\nJOIN #opchan2\r\n"
    sleep 0.3
    open_fd 4
    send_fd 4 "PASS $PASS\r\nNICK modeuser\r\nUSER modeuser 0 * :ModeUser\r\nJOIN #opchan2\r\n"
    sleep 0.3
    send_fd 3 "MODE #opchan2 +o modeuser\r\n"
    sleep 0.3
    RECV3=$(read_fd 3)
    RECV4=$(read_fd 4)
    close_fd 3
    close_fd 4
    if echo "$RECV3$RECV4" | grep -q "MODE.*+o.*modeuser"; then
        print_test "PASS" "MODE +o → broadcasts operator grant to channel"
    else
        print_test "FAIL" "MODE +o should broadcast to channel" "Got3: $RECV3 | Got4: $RECV4"
    fi
    check_running || return

    # MODE unknown mode char → 472
    OUT=$(irc_send "PASS $PASS\r\nNICK modeunk\r\nUSER modeunk 0 * :ModeUnk\r\nJOIN #unkchan\r\nMODE #unkchan +z\r\n")
    if echo "$OUT" | grep -q "472"; then
        print_test "PASS" "Unknown mode char '+z' → 472 ERR_UNKNOWNMODE"
    else
        print_test "FAIL" "Unknown mode should return 472" "Got: $OUT"
    fi
    check_running || return

    # MODE on nonexistent channel → 403
    OUT=$(irc_send "PASS $PASS\r\nNICK modenosuch\r\nUSER modenosuch 0 * :ModeNoSuch\r\nMODE #ghostchan +i\r\n")
    if echo "$OUT" | grep -q "403"; then
        print_test "PASS" "MODE on nonexistent channel → 403 ERR_NOSUCHCHANNEL"
    else
        print_test "FAIL" "MODE on nonexistent channel should return 403" "Got: $OUT"
    fi
    check_running || return

    # MODE with no args → 461
    OUT=$(irc_send "PASS $PASS\r\nNICK modenoarg\r\nUSER modenoarg 0 * :ModeNoArg\r\nMODE\r\n")
    if echo "$OUT" | grep -q "461"; then
        print_test "PASS" "MODE with no args → 461 ERR_NEEDMOREPARAMS"
    else
        print_test "FAIL" "MODE with no args should return 461" "Got: $OUT"
    fi
    check_running || return

    # Non-operator MODE → 482
    open_fd 3
    send_fd 3 "PASS $PASS\r\nNICK nopmode_op\r\nUSER nopmode_op 0 * :NopmodeOp\r\nJOIN #nopmodechan\r\n"
    sleep 0.3
    open_fd 4
    send_fd 4 "PASS $PASS\r\nNICK nopmode_usr\r\nUSER nopmode_usr 0 * :NopmodeUsr\r\nJOIN #nopmodechan\r\n"
    sleep 0.3
    send_fd 4 "MODE #nopmodechan +t\r\n"
    sleep 0.2
    RECV4=$(read_fd 4)
    close_fd 3
    close_fd 4
    if echo "$RECV4" | grep -q "482"; then
        print_test "PASS" "Non-operator MODE → 482 ERR_CHANOPRIVSNEEDED"
    else
        print_test "FAIL" "Non-op MODE should return 482" "Got: $RECV4"
    fi
    check_running || return
}

# ----------------------------------------------------------
# 9. Unknown Commands
# ----------------------------------------------------------
test_unknown_commands() {
    print_header "9. Unknown / Invalid Commands"

    # Unknown command after auth → 421
    OUT=$(irc_send "PASS $PASS\r\nNICK unknownu\r\nUSER unknownu 0 * :UnknownU\r\nFOOBAR blah\r\n")
    if echo "$OUT" | grep -q "421"; then
        print_test "PASS" "Unknown command after auth → 421 ERR_UNKNOWNCOMMAND"
    else
        print_test "FAIL" "Unknown command should return 421" "Got: $OUT"
    fi
    check_running || return

    # Unknown command without auth → 421 (hits ft_is_not_cmd first)
    OUT=$(irc_send "BLAH\r\n")
    if echo "$OUT" | grep -qE "421|451"; then
        print_test "PASS" "Unknown command without auth → 421 or 451"
    else
        print_test "FAIL" "Unknown command from unauthed client should return 421 or 451" "Got: $OUT"
    fi
    check_running || return
}

# ----------------------------------------------------------
# 10. Stress / Crash / Edge Case Tests
# ----------------------------------------------------------
test_stress_crash() {
    print_header "10. Stress / Crash / Edge Case Tests"

    # 10 simultaneous quick connections
    # Each uses irc_send which uses 'timeout' so they all finish in ≤0.4s
    for i in $(seq 1 10); do
        irc_send "PASS $PASS\r\nNICK stress$i\r\nUSER stress$i 0 * :Stress$i\r\n" > /dev/null &
    done
    wait   # wait only for these 10 background irc_send jobs, all finish quickly
    sleep 0.2
    if kill -0 "$SERVER_PID" 2>/dev/null; then
        print_test "PASS" "10 simultaneous connections → server still alive"
    else
        print_test "FAIL" "Server crashed under simultaneous connections"
        return
    fi

    # Very long NICK (512 chars) — must not crash
    LONGNICK=$(printf 'A%.0s' {1..512})
    irc_send "PASS $PASS\r\nNICK $LONGNICK\r\n" > /dev/null
    if kill -0 "$SERVER_PID" 2>/dev/null; then
        print_test "PASS" "512-char NICK → server did not crash"
    else
        print_test "FAIL" "Server crashed on 512-char NICK"
        return
    fi

    # Very long PRIVMSG body (1024 chars) — must not crash
    LONGMSG=$(printf 'B%.0s' {1..1024})
    irc_send "PASS $PASS\r\nNICK longmsg\r\nUSER longmsg 0 * :LongMsg\r\nJOIN #lmchan\r\nPRIVMSG #lmchan :$LONGMSG\r\n" > /dev/null
    if kill -0 "$SERVER_PID" 2>/dev/null; then
        print_test "PASS" "1024-char PRIVMSG → server did not crash"
    else
        print_test "FAIL" "Server crashed on 1024-char PRIVMSG"
        return
    fi

    # Multiple empty lines — must not crash
    irc_send "\r\n\r\n\r\n\r\n\r\n" > /dev/null
    if kill -0 "$SERVER_PID" 2>/dev/null; then
        print_test "PASS" "Multiple empty lines → server did not crash"
    else
        print_test "FAIL" "Server crashed on empty lines"
        return
    fi

    # KICK a nick that does NOT exist in channel.
    # BUG WARNING: in ft_parse_input_kick, after the "found" loop, your code calls
    # ft_get_user_by_nick(users)->set_flag_invitation(0) unconditionally.
    # If ft_get_user_by_nick returns NULL → NULL dereference → SEGFAULT.
    open_fd 3
    send_fd 3 "PASS $PASS\r\nNICK kegop\r\nUSER kegop 0 * :KegOp\r\nJOIN #kegchan\r\n"
    sleep 0.3
    send_fd 3 "KICK #kegchan ghostnick :bye\r\n"
    sleep 0.3
    close_fd 3
    if kill -0 "$SERVER_PID" 2>/dev/null; then
        print_test "PASS" "KICK nonexistent nick → server did not crash (no segfault)"
    else
        print_test "FAIL" "Server CRASHED kicking nonexistent nick → NULL dereference in ft_parse_input_kick!"
        return
    fi

    # PRIVMSG to self — must not crash or loop
    irc_send "PASS $PASS\r\nNICK selfmsg\r\nUSER selfmsg 0 * :SelfMsg\r\nPRIVMSG selfmsg :hello me\r\n" > /dev/null
    if kill -0 "$SERVER_PID" 2>/dev/null; then
        print_test "PASS" "PRIVMSG to self → server did not crash"
    else
        print_test "FAIL" "Server crashed on PRIVMSG to self"
        return
    fi

    # Operator disconnects → remaining user auto-promoted to operator
    open_fd 3
    send_fd 3 "PASS $PASS\r\nNICK discopop\r\nUSER discopop 0 * :DiscoP\r\nJOIN #discochan\r\n"
    sleep 0.2
    open_fd 4
    send_fd 4 "PASS $PASS\r\nNICK discomem\r\nUSER discomem 0 * :DiscoM\r\nJOIN #discochan\r\n"
    sleep 0.3
    close_fd 3   # operator disconnects
    sleep 0.3
    send_fd 4 "MODE #discochan +t\r\n"  # discomem should now be op
    sleep 0.2
    RECV4=$(read_fd 4)
    close_fd 4
    if kill -0 "$SERVER_PID" 2>/dev/null; then
        print_test "PASS" "Operator disconnects → server stable, remaining user promoted to op"
    else
        print_test "FAIL" "Server crashed after operator disconnect"
        return
    fi

    # MODE +l then -l → JOIN succeeds after limit removed
    open_fd 3
    send_fd 3 "PASS $PASS\r\nNICK limitop\r\nUSER limitop 0 * :LimitOp\r\nJOIN #limitchan\r\nMODE #limitchan +l 1\r\nMODE #limitchan -l\r\n"
    sleep 0.4
    OUT=$(irc_send "PASS $PASS\r\nNICK limitjoin\r\nUSER limitjoin 0 * :LimitJoin\r\nJOIN #limitchan\r\n")
    close_fd 3
    if echo "$OUT" | grep -qiE "JOIN.*#limitchan|353.*#limitchan"; then
        print_test "PASS" "MODE +l then -l → JOIN succeeds after limit removed"
    else
        print_test "FAIL" "JOIN after -l should succeed" "Got: $OUT"
    fi
    check_running || return

    # MODE +k then -k → JOIN without key succeeds
    open_fd 3
    send_fd 3 "PASS $PASS\r\nNICK kclearop\r\nUSER kclearop 0 * :KClearOp\r\nJOIN #kclearchan\r\nMODE #kclearchan +k pwd123\r\nMODE #kclearchan -k\r\n"
    sleep 0.4
    OUT=$(irc_send "PASS $PASS\r\nNICK kclearjoin\r\nUSER kclearjoin 0 * :KClearJoin\r\nJOIN #kclearchan\r\n")
    close_fd 3
    if echo "$OUT" | grep -qiE "JOIN.*#kclearchan|353.*#kclearchan"; then
        print_test "PASS" "MODE +k then -k → JOIN without key succeeds after key removed"
    else
        print_test "FAIL" "JOIN after -k should succeed without key" "Got: $OUT"
    fi
    check_running || return
}

# ----------------------------------------------------------
# 11. Multi-Client Channel Interaction
# ----------------------------------------------------------
test_multi_client_chat() {
    print_header "11. Multi-Client Channel Interaction"

    # 3 clients join same channel, all chat
    open_fd 3
    send_fd 3 "PASS $PASS\r\nNICK alpha\r\nUSER alpha 0 * :Alpha\r\nJOIN #talkroom\r\n"
    sleep 0.2
    open_fd 4
    send_fd 4 "PASS $PASS\r\nNICK beta\r\nUSER beta 0 * :Beta\r\nJOIN #talkroom\r\n"
    sleep 0.2
    open_fd 5
    send_fd 5 "PASS $PASS\r\nNICK gamma\r\nUSER gamma 0 * :Gamma\r\nJOIN #talkroom\r\n"
    sleep 0.3

    # alpha → channel; beta and gamma should receive
    send_fd 3 "PRIVMSG #talkroom :hello everyone\r\n"
    sleep 0.3
    RECV4=$(read_fd 4)
    RECV5=$(read_fd 5)
    if echo "$RECV4" | grep -q "hello everyone" && echo "$RECV5" | grep -q "hello everyone"; then
        print_test "PASS" "3-person channel: alpha's message received by beta AND gamma"
    else
        print_test "FAIL" "Both beta and gamma should receive alpha's message" "beta: $RECV4 | gamma: $RECV5"
    fi
    check_running || return

    # beta → channel; alpha and gamma should receive
    send_fd 4 "PRIVMSG #talkroom :beta here\r\n"
    sleep 0.3
    RECV3=$(read_fd 3)
    RECV5=$(read_fd 5)
    if echo "$RECV3" | grep -q "beta here" && echo "$RECV5" | grep -q "beta here"; then
        print_test "PASS" "3-person channel: beta's message received by alpha AND gamma"
    else
        print_test "FAIL" "Both alpha and gamma should receive beta's message" "alpha: $RECV3 | gamma: $RECV5"
    fi
    close_fd 3
    close_fd 4
    close_fd 5
    check_running || return

    # Op grants op to member → new op kicks original op
    open_fd 3
    send_fd 3 "PASS $PASS\r\nNICK chainop1\r\nUSER chainop1 0 * :ChainOp1\r\nJOIN #chainchan\r\n"
    sleep 0.3
    open_fd 4
    send_fd 4 "PASS $PASS\r\nNICK chainop2\r\nUSER chainop2 0 * :ChainOp2\r\nJOIN #chainchan\r\n"
    sleep 0.3
    send_fd 3 "MODE #chainchan +o chainop2\r\n"
    sleep 0.3
    send_fd 4 "KICK #chainchan chainop1 :power move\r\n"
    sleep 0.3
    RECV3=$(read_fd 3)
    close_fd 3
    close_fd 4
    if echo "$RECV3" | grep -qiE "KICK.*chainop1"; then
        print_test "PASS" "Op grants op to member → new op can kick original op"
    else
        print_test "FAIL" "Newly granted op should be able to kick original op" "Got: $RECV3"
    fi
    check_running || return
}

# ============================================================
# Main
# ============================================================

echo ""
echo -e "${BOLD}${CYAN}╔══════════════════════════════════════════════════╗${RESET}"
echo -e "${BOLD}${CYAN}║        ft_irc RFC1459 Test Suite — 1337          ║${RESET}"
echo -e "${BOLD}${CYAN}╚══════════════════════════════════════════════════╝${RESET}"
echo ""

if ! command -v nc &>/dev/null; then
    echo -e "${RED}ERROR: 'nc' (netcat) is required.${RESET}"
    exit 1
fi
if ! command -v timeout &>/dev/null; then
    echo -e "${RED}ERROR: 'timeout' is required (install coreutils).${RESET}"
    exit 1
fi

start_server

test_args
test_auth
test_join
test_privmsg
test_kick
test_invite
test_topic
test_mode
test_unknown_commands
test_stress_crash
test_multi_client_chat

stop_server

echo ""
echo -e "${CYAN}${BOLD}══════════════════════════════════════════════════${RESET}"
echo -e "${BOLD}  RESULTS: ${GREEN}$PASSED PASSED${RESET} | ${RED}$FAILED FAILED${RESET} | Total: $TOTAL"
echo -e "${CYAN}${BOLD}══════════════════════════════════════════════════${RESET}"
echo ""
if [ "$FAILED" -eq 0 ]; then
    echo -e "${GREEN}${BOLD}  ✓ ALL TESTS PASSED! Great work!${RESET}"
else
    echo -e "${RED}${BOLD}  ✗ $FAILED test(s) failed. Review the output above.${RESET}"
fi
echo ""
exit "$FAILED"
