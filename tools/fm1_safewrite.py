#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Durable, no-overwrite writes for the backup tools.

A backup is evidence: an existing file is never replaced, and a file that was
reported as written is on the disk, not only in the page cache. This is the
same rule M-Flasher 0.11.0 applies to the two files it saves (create_new +
write_synced + sync_dir); these helpers give the Python tools the same
behaviour.

  write_new(path, data)  create path with O_EXCL, write, flush, fsync
  sync_dir(path)         fsync the directory entry, where the OS supports it
"""
import os
from pathlib import Path


def write_new(path, data):
    """Create `path` with O_EXCL and make its bytes durable. FileExistsError if it exists."""
    path = Path(path)
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    try:
        with os.fdopen(fd, "wb") as f:
            f.write(data)
            f.flush()
            os.fsync(f.fileno())
    except BaseException:
        # A half-written file is not evidence: remove it, then report the original error.
        try:
            path.unlink()
        except OSError:
            pass
        raise
    return path


def sync_dir(path):
    """fsync a directory so a new name survives a power cut; a no-op where that is unsupported."""
    try:
        fd = os.open(path, os.O_RDONLY)
    except OSError:
        return
    try:
        os.fsync(fd)
    except OSError:
        pass
    finally:
        os.close(fd)
