#!/usr/bin/env python3
"""Test LinCLI tab-completion + backspace interaction via PTY."""
import os
import pty
import select
import time

def run_test(keys, description):
    master_fd, slave_fd = pty.openpty()
    pid = os.fork()
    if pid == 0:
        os.close(master_fd)
        os.setsid()
        os.dup2(slave_fd, 0)
        os.dup2(slave_fd, 1)
        os.dup2(slave_fd, 2)
        os.close(slave_fd)
        os.execv("./build/bin/a.out", ["./build/bin/a.out"])
        os._exit(1)
    os.close(slave_fd)
    time.sleep(0.4)
    
    while True:
        ready, _, _ = select.select([master_fd], [], [], 0.3)
        if not ready:
            break
        try:
            chunk = os.read(master_fd, 4096)
            if not chunk:
                break
        except OSError:
            break
    
    output = b""
    for key in keys:
        os.write(master_fd, key)
        time.sleep(0.12)
    time.sleep(0.5)
    
    while True:
        ready, _, _ = select.select([master_fd], [], [], 0.5)
        if not ready:
            break
        try:
            chunk = os.read(master_fd, 4096)
            if not chunk:
                break
            output += chunk
        except OSError:
            break
    
    os.kill(pid, 9)
    os.close(master_fd)
    
    text = output.decode('utf-8', errors='replace')
    text = text.replace('\x1b[2m', '[DIM]').replace('\x1b[0m', '[RESET]').replace('\x1b[7m', '[REV]')
    text = text.replace('\x1b[K', '[CLR]').replace('\x1b[2K', '[CLR2]')
    text = text.replace('\x1b[1A', '[UP1]').replace('\x1b[D', '[LEFT1]')
    text = text.replace('\x1b[H\x1b[2J', '[CLS]')
    text = text.replace('\r\n', '\n').replace('\r', '\n')
    text = text.replace('\x07', '[BELL]')
    
    print(f"\n===== {description} =====")
    print(text)
    return text

if __name__ == "__main__":
    # t + tab + tab -> "tb ", then ONE backspace -> "tb", then tab
    run_test([b't', b'\t', b'\t', b'\x7f', b'\t'],
             "t<tab><tab><backspace><tab>  (only ONE backspace)")
    
    # t + tab + tab -> "tb ", then ONE backspace -> "tb", then tab tab
    run_test([b't', b'\t', b'\t', b'\x7f', b'\t', b'\t'],
             "t<tab><tab><backspace><tab><tab>")
