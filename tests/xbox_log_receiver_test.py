#!/usr/bin/env python3
"""Loopback integration checks for the read-only UDP diagnostic receiver."""

import json
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import unittest

RECEIVER = Path(__file__).resolve().parents[1] / "scripts" / "receive-xbox-log.py"


class ReceiverTest(unittest.TestCase):
    def start_receiver(self, output, *options, quiet=True):
        process = subprocess.Popen(
            [sys.executable, str(RECEIVER), str(output), "--bind", "127.0.0.1", "--port", "0", *(["--quiet"] if quiet else []), *options],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        )
        self.addCleanup(self.close_receiver, process)
        ready = json.loads(process.stdout.readline())
        return process, ready["port"]

    @staticmethod
    def close_receiver(process):
        if process.poll() is None:
            process.terminate()
        process.communicate(timeout=3)

    def test_preserves_packet_boundaries_and_sends_no_reply(self):
        with tempfile.TemporaryDirectory() as folder:
            output = Path(folder) / "capture.jsonl"
            process, port = self.start_receiver(output, "--max-packets", "3", "--duration", "3", "--peer", "127.0.0.1")
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender:
                sender.settimeout(0.1)
                for packet in [b"<SE-UDP seq=7 tick=4294967295> <LOG> PLAYER X=1470\nsecond line", b"unparsed \xff", b"x" * 1023]:
                    sender.sendto(packet, ("127.0.0.1", port))
                with self.assertRaises(socket.timeout):
                    sender.recvfrom(2048)
            stdout, stderr = process.communicate(timeout=4)
            self.assertEqual(process.returncode, 0, stderr)
            rows = [json.loads(line) for line in output.read_text().splitlines()]
            packets = [row for row in rows if row["event"] == "packet"]
            self.assertEqual(len(packets), 3)
            self.assertEqual(packets[0]["sequence"], 7)
            self.assertEqual(packets[0]["tick_ms"], 4294967295)
            self.assertEqual(packets[0]["message"], "<LOG> PLAYER X=1470\nsecond line")
            self.assertNotIn("sequence", packets[1])
            self.assertIn("\ufffd", packets[1]["raw"])
            self.assertTrue(packets[2]["at_packet_limit"])
            self.assertEqual(json.loads(stdout.splitlines()[-1])["packets"], 3)

    def test_filters_other_peers_and_stops_at_deadline(self):
        with tempfile.TemporaryDirectory() as folder:
            output = Path(folder) / "capture.jsonl"
            process, port = self.start_receiver(output, "--duration", "0.4", "--peer", "127.0.0.2")
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender:
                sender.sendto(b"<SE-UDP seq=0 tick=1> ignored", ("127.0.0.1", port))
            stdout, stderr = process.communicate(timeout=3)
            self.assertEqual(process.returncode, 0, stderr)
            summary = json.loads(stdout.splitlines()[-1])
            self.assertEqual(summary["packets"], 0)
            self.assertEqual(summary["ignored_peer_packets"], 1)

    def test_refuses_to_overwrite_evidence(self):
        with tempfile.TemporaryDirectory() as folder:
            output = Path(folder) / "capture.jsonl"
            output.write_text("original evidence\n")
            result = subprocess.run([sys.executable, str(RECEIVER), str(output)], capture_output=True, text=True, timeout=3)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("refusing to overwrite", result.stderr)
            self.assertEqual(output.read_text(), "original evidence\n")

    def test_console_output_escapes_project_control_characters(self):
        with tempfile.TemporaryDirectory() as folder:
            output = Path(folder) / "capture.jsonl"
            process, port = self.start_receiver(output, "--max-packets", "1", "--duration", "3", quiet=False)
            message = "<LOG> value=\x1b]52;c;AAAA\x07\nnext"
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender:
                sender.sendto(("<SE-UDP seq=1 tick=2> " + message).encode(), ("127.0.0.1", port))
            stdout, stderr = process.communicate(timeout=4)
            self.assertEqual(process.returncode, 0, stderr)
            self.assertNotIn("\x1b", stdout)
            self.assertEqual(json.loads(stdout.splitlines()[0])["message"], message)


if __name__ == "__main__":
    unittest.main()
