#!/usr/bin/env python3
"""
lincli_csv_bridge.py

LinCLI serial/PTY bridge with real-time CSV capture and optional live plotting.

Usage (serial mode - real MCU):
    python3 lincli_csv_bridge.py /dev/ttyUSB0 115200 output.csv [--plot]

Usage (subprocess mode - PC simulation via PTY):
    python3 lincli_csv_bridge.py --exec ./build/bin/a.out output.csv [--plot]

Dependencies (install when needed):
    sudo apt update && sudo apt install python3-serial python3-matplotlib -y

The script:
1. Opens serial port or launches a subprocess with a pseudo-terminal
2. Presents a normal CLI terminal (prompts, command input, output)
3. Detects \r-prefixed data lines and extracts CSV fields
4. Writes CSV data to file
5. Optionally displays real-time matplotlib curves (requires matplotlib)
"""

import argparse
import csv
import os
import pty
import re
import select
import subprocess
import sys
import termios
import threading
import time
import tty
from collections import deque

# ---------------------------------------------------------------------------
# Serial (optional, only needed for real serial port mode)
# ---------------------------------------------------------------------------
try:
    import serial
    SERIAL_OK = True
except ImportError:
    SERIAL_OK = False

# ---------------------------------------------------------------------------
# Matplotlib (optional)
# ---------------------------------------------------------------------------
try:
    import matplotlib
    matplotlib.use('TkAgg')
    import matplotlib.pyplot as plt
    MATPLOTLIB_OK = True
except Exception:
    MATPLOTLIB_OK = False


class DataParser:
    """Parse \r-prefixed data lines into dicts."""

    DATA_RE = re.compile(r'^\s*([^:]+):\s*([-\d.eE]+)\s*$')
    ANSI_RE = re.compile(r'\x1b\[[0-9;]*m')

    @staticmethod
    def parse(line):
        line = DataParser.ANSI_RE.sub('', line.lstrip('\r').strip())
        if not line:
            return None
        # Try simple float list: "0.523,1500.0,2.3"
        try:
            vals = [float(x.strip()) for x in line.split(',')]
            return {'_raw': vals}
        except ValueError:
            pass
        # Try key:value pairs: "theta:0.523, speed:1500.0"
        result = {}
        for field in line.split(','):
            m = DataParser.DATA_RE.match(field.strip())
            if m:
                try:
                    result[m.group(1)] = float(m.group(2))
                except ValueError:
                    result[m.group(1)] = m.group(2)
        return result if result else None


class CsvWriter:
    """Thread-safe CSV writer with auto-header."""

    def __init__(self, path):
        self.path = path
        self.file = open(path, 'w', newline='')
        self.writer = None
        self.lock = threading.Lock()
        self.header_written = False

    def write(self, data_dict):
        with self.lock:
            if not data_dict:
                return
            keys = list(data_dict.keys())
            if not self.header_written or set(keys) != set(self.writer.fieldnames if self.writer else []):
                self.writer = csv.DictWriter(self.file, fieldnames=keys)
                self.writer.writeheader()
                self.header_written = True
            self.writer.writerow(data_dict)
            self.file.flush()

    def close(self):
        self.file.close()


class LivePlotter:
    """Real-time matplotlib plotter (optional)."""

    def __init__(self, maxlen=5000):
        self.maxlen = maxlen
        self.data = {}
        self.lock = threading.Lock()
        self.fig = None
        self.ax = None
        self.lines = {}
        self.labels = {'ch1': 'theta', 'ch2': 'speed', 'ch3': 'iq'}

    def add(self, data_dict):
        with self.lock:
            for k, v in data_dict.items():
                if k == '_raw' and len(v) >= 4:
                    # Use the data's own timestamp (first column, ms -> s)
                    ts = float(v[0]) / 1000.0
                    for i, val in enumerate(v[1:], start=1):
                        key = f'ch{i}'
                        if key not in self.data:
                            self.data[key] = deque(maxlen=self.maxlen)
                        self.data[key].append((ts, float(val)))
                elif k != '_raw':
                    if k not in self.data:
                        self.data[k] = deque(maxlen=self.maxlen)
                    try:
                        self.data[k].append((time.time(), float(v)))
                    except (ValueError, TypeError):
                        pass

    def _init_plot(self):
        self.fig, self.ax = plt.subplots(dpi=150)
        self.ax.set_xlabel('time (s)')
        self.ax.set_ylabel('value')
        self.ax.set_title('LinCLI Real-time Scope')
        plt.ion()
        plt.show(block=False)
        plt.pause(0.001)

    def update(self):
        if self.fig is None:
            self._init_plot()

        with self.lock:
            for key, series in self.data.items():
                if not series:
                    continue
                xs = [p[0] for p in series]
                ys = [p[1] for p in series]
                label = self.labels.get(key, key)
                if key not in self.lines:
                    self.lines[key], = self.ax.plot(
                        xs, ys, label=label,
                        linewidth=1.2, marker='.', markersize=3,
                        antialiased=True)
                else:
                    self.lines[key].set_data(xs, ys)

            if self.lines:
                self.ax.relim()
                self.ax.autoscale_view()
                self.ax.legend(loc='upper right')
                self.fig.canvas.draw()
                self.fig.canvas.flush_events()



class SerialBridge:
    """Bridge for real serial port."""

    def __init__(self, port, baud, csv_path, plot=False):
        if not SERIAL_OK:
            raise RuntimeError("pyserial not installed. Run: pip install pyserial")
        self.ser = serial.Serial(port, baud, timeout=0.01)
        self.csv = CsvWriter(csv_path)
        self.plotter = LivePlotter() if plot and MATPLOTLIB_OK else None
        self.raw_mode = False

    def setup_tty(self):
        self.fd = sys.stdin.fileno()
        if os.isatty(self.fd):
            self.old_settings = termios.tcgetattr(self.fd)
            tty.setraw(self.fd)
            self.raw_mode = True

    def restore_tty(self):
        if self.raw_mode:
            termios.tcsetattr(self.fd, termios.TCSADRAIN, self.old_settings)
            self.raw_mode = False

    def run(self):
        self.setup_tty()

        buf = ""
        last_plot = 0
        try:
            while True:
                data = self.ser.read(1024)
                if data:
                    text = data.decode('utf-8', errors='replace')
                    buf += text
                    # Split by \r (single-line refresh from MCU)
                    while True:
                        idx = buf.find('\r')
                        if idx == -1:
                            break
                        self._handle_frame(buf[:idx])
                        buf = buf[idx + 1:]
                    sys.stdout.buffer.write(data)
                    sys.stdout.flush()

                if select.select([sys.stdin], [], [], 0)[0]:
                    key = sys.stdin.read(1)
                    if key == '\x03':  # Ctrl+C
                        break
                    self.ser.write(key.encode())
                    sys.stdout.write(key)
                    sys.stdout.flush()

                if self.plotter and time.time() - last_plot > 0.02:
                    self.plotter.update()
                    last_plot = time.time()
        except KeyboardInterrupt:
            pass
        finally:
            self.restore_tty()
            self.csv.close()
            self.ser.close()

    def _handle_frame(self, buf):
        row = DataParser.parse(buf)
        if row:
            self.csv.write(row)
            if self.plotter:
                self.plotter.add(row)


class SubprocessBridge:
    """Bridge for PC simulation via PTY."""

    def __init__(self, exec_cmd, csv_path, plot=False):
        self.exec_cmd = exec_cmd
        self.csv = CsvWriter(csv_path)
        self.plotter = LivePlotter() if plot and MATPLOTLIB_OK else None
        self.raw_mode = False

    def setup_tty(self):
        self.fd = sys.stdin.fileno()
        if os.isatty(self.fd):
            try:
                self.old_settings = termios.tcgetattr(self.fd)
                tty.setraw(self.fd)
                self.raw_mode = True
            except termios.error:
                self.raw_mode = False

    def restore_tty(self):
        if self.raw_mode:
            termios.tcsetattr(self.fd, termios.TCSADRAIN, self.old_settings)
            self.raw_mode = False

    def run(self):
        # Init plotter window before tty raw mode to avoid Tk init issues
        if self.plotter:
            self.plotter._init_plot()

        self.setup_tty()

        master_fd, slave_fd = pty.openpty()

        env = os.environ.copy()
        env['TERM'] = 'xterm'

        proc = subprocess.Popen(
            self.exec_cmd,
            stdin=slave_fd,
            stdout=slave_fd,
            stderr=slave_fd,
            close_fds=True,
            env=env
        )
        os.close(slave_fd)

        buf = b""
        stdin_fd = sys.stdin.fileno()
        last_plot = 0
        try:
            while proc.poll() is None:
                rfds = [master_fd, stdin_fd]

                r, _, _ = select.select(rfds, [], [], 0.01)

                if master_fd in r:
                    try:
                        data = os.read(master_fd, 1024)
                    except OSError:
                        break
                    if not data:
                        break
                    sys.stdout.buffer.write(data)
                    sys.stdout.flush()
                    buf = self._process_buffer(buf + data)

                if stdin_fd in r:
                    try:
                        key = os.read(stdin_fd, 1024)
                    except OSError:
                        break
                    if not key:
                        break
                    if b'\x03' in key:  # Ctrl+C
                        break
                    os.write(master_fd, key)

                if self.plotter and time.time() - last_plot > 0.02:
                    self.plotter.update()
                    last_plot = time.time()

        except KeyboardInterrupt:
            proc.terminate()
        finally:
            self.restore_tty()
            self.csv.close()
            try:
                os.close(master_fd)
            except OSError:
                pass
            proc.wait()

    def _process_buffer(self, buf):
        # scope data lines are separated by \r (carriage return without \n)
        while True:
            idx = buf.find(b'\r')
            if idx == -1:
                return buf
            frame = buf[:idx]
            buf = buf[idx + 1:]
            text = frame.decode('utf-8', errors='replace').lstrip('\n')
            if not text:
                continue
            row = DataParser.parse(text)
            if row:
                self.csv.write(row)
                if self.plotter:
                    self.plotter.add(row)


def main():
    parser = argparse.ArgumentParser(description='LinCLI CSV bridge with live plotting')
    parser.add_argument('--exec', metavar='CMD', help='Run subprocess via PTY (PC simulation mode)')
    parser.add_argument('--plot', action='store_true', help='Enable real-time matplotlib plotting')
    parser.add_argument('port_or_csv', nargs='?', help='Serial port (serial mode) or CSV file (subprocess mode)')
    parser.add_argument('baud_or_exec', nargs='?', help='Baud rate (serial mode)')
    parser.add_argument('csv_file', nargs='?', help='CSV output file (serial mode)')
    args = parser.parse_args()

    if args.exec:
        if not args.port_or_csv:
            print("ERROR: CSV output file required in subprocess mode", file=sys.stderr)
            sys.exit(1)
        bridge = SubprocessBridge(args.exec.split(), args.port_or_csv, plot=args.plot)
        bridge.run()
    else:
        if not all([args.port_or_csv, args.baud_or_exec, args.csv_file]):
            print("Usage (serial): lincli_csv_bridge.py <port> <baud> <csv_file>", file=sys.stderr)
            print("Usage (subproc): lincli_csv_bridge.py --exec <cmd> <csv_file> [--plot]", file=sys.stderr)
            sys.exit(1)
        bridge = SerialBridge(args.port_or_csv, int(args.baud_or_exec), args.csv_file, plot=args.plot)
        bridge.run()


if __name__ == '__main__':
    main()
