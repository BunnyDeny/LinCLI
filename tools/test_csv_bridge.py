#!/usr/bin/env python3
"""
Test script for lincli_csv_bridge.py (PTY subprocess mode).

Automatically sends commands to a.out, collects CSV data, and verifies output.
"""

import os
import pty
import subprocess
import sys
import time


def main():
    exec_cmd = sys.argv[1] if len(sys.argv) > 1 else './build/bin/a.out'
    csv_path = sys.argv[2] if len(sys.argv) > 2 else '/tmp/scope_test.csv'

    master_fd, slave_fd = pty.openpty()

    env = os.environ.copy()
    env['TERM'] = 'xterm'

    proc = subprocess.Popen(
        exec_cmd.split(),
        stdin=slave_fd,
        stdout=slave_fd,
        stderr=slave_fd,
        close_fds=True,
        env=env
    )
    os.close(slave_fd)

    buf = b""
    csv_lines = []
    header_seen = False

    def flush_stdout():
        text = buf.decode('utf-8', errors='replace')
        sys.stdout.write(text)
        sys.stdout.flush()

    def send(cmd):
        os.write(master_fd, (cmd + '\r').encode())
        time.sleep(0.2)

    try:
        # Wait for prompt
        time.sleep(0.5)
        data = os.read(master_fd, 4096)
        buf += data
        flush_stdout()
        buf = b""

        # Send scope command
        send("scope -p 50 -d 1")
        time.sleep(1.5)

        # Collect output
        while True:
            r, _, _ = select.select([master_fd], [], [], 0.5)
            if not r:
                break
            data = os.read(master_fd, 4096)
            if not data:
                break
            text = data.decode('utf-8', errors='replace')
            sys.stdout.write(text)
            sys.stdout.flush()

            # Parse CSV data: split by \r (single-line refresh, no \n between frames)
            import re
            for part in text.split('\r'):
                part = re.sub(r'\x1b\[[0-9;]*m', '', part)
                part = part.strip()
                if part:
                    # Check if it looks like CSV data (numbers and commas)
                    if re.match(r'^[\d,\s]+$', part) or part.startswith('timestamp'):
                        csv_lines.append(part)
                        if not header_seen and part.startswith('timestamp'):
                            header_seen = True

        # Send exit
        send("exit")
        time.sleep(0.3)

    except KeyboardInterrupt:
        pass
    finally:
        proc.terminate()
        try:
            os.close(master_fd)
        except OSError:
            pass
        proc.wait()

    # Write CSV file
    with open(csv_path, 'w') as f:
        for line in csv_lines:
            f.write(line + '\n')

    print(f"\n[TEST] Collected {len(csv_lines)} CSV lines")
    print(f"[TEST] CSV saved to: {csv_path}")

    if csv_lines:
        print(f"[TEST] First data line: {csv_lines[0]}")
        if len(csv_lines) > 1:
            print(f"[TEST] Last  data line: {csv_lines[-1]}")
        print("[TEST] PASS")
        return 0
    else:
        print("[TEST] FAIL: no CSV data collected")
        return 1


if __name__ == '__main__':
    import select
    sys.exit(main())
