# SPDX-License-Identifier: MIT
"""A run's directory: the raw console transcript, an event log and saved output.

Each tool run gets DATA_DIR/runs/<UTC time>-<tool>/ holding rx.bin (every byte
the console sent, unchanged), events.jsonl (what the tool did and saw, one JSON
object per line) and the output of the commands worth keeping. Nothing in a run
directory is needed again; it is there to diagnose a failed run.
"""
import datetime
import hashlib
import json
import os
from pathlib import Path
import threading
import time

# Event kinds that are logged but not printed: there are thousands of them.
QUIET = frozenset({'rx', 'tx_attempt', 'tx_written', 'command_result', 'tftp'})
PRINTED_FIELD_LIMIT = 160


def utc_stamp():
    return datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, 'rb') as stream:
        for block in iter(lambda: stream.read(1 << 20), b''):
            digest.update(block)
    return digest.hexdigest()


class RunLog:
    def __init__(self, data_dir, tool):
        self.dir = Path(data_dir).expanduser() / 'runs' / f'{utc_stamp()}-{tool}'
        self.dir.mkdir(parents=True, mode=0o700)
        self.events = (self.dir / 'events.jsonl').open('a', buffering=1)
        self.lock = threading.Lock()

    def event(self, kind, **fields):
        record = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                      monotonic_ns=time.monotonic_ns(), event=kind, **fields)
        with self.lock:
            self.events.write(json.dumps(record, default=str) + '\n')
            if kind not in QUIET:
                print(f'  {kind}: {self._readable(fields)}', flush=True)

    @staticmethod
    def _readable(fields):
        parts = []
        for key, value in fields.items():
            text = value if isinstance(value, str) else json.dumps(value, default=str)
            text = ' '.join(text.split())
            if len(text) > PRINTED_FIELD_LIMIT:
                text = text[:PRINTED_FIELD_LIMIT] + '...'
            parts.append(f'{key}={text}')
        return ' '.join(parts) or '-'

    def text(self, name, body):
        """Keep one command's console output in the run directory."""
        path = self.dir / name
        path.write_text(body)
        return path

    def close(self):
        with self.lock:
            self.events.close()


def private_file(path, data, mode=0o600):
    """Write a file only the user can read; the parent is created likewise."""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, mode)
    with os.fdopen(descriptor, 'wb') as stream:
        stream.write(data)
    os.chmod(path, mode)
    return path
