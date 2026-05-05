#!/usr/bin/env python3
"""Send a message to a webhook URL.

Examples:
    python webhook_sender.py --url "https://discord.com/api/webhooks/ID/TOKEN" --message "Hello from Python"
  python webhook_sender.py --message "Uses WEBHOOK_URL env var"
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time
import urllib.error
import urllib.request


def send_webhook_message(url: str, message: str) -> None:
    payload = {"content": message}
    body = json.dumps(payload).encode("utf-8")

    req = urllib.request.Request(
        url=url,
        data=body,
        headers={
            "Content-Type": "application/json",
            "User-Agent": "webhook_sender.py/1.0",
        },
        method="POST",
    )

    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            status = getattr(resp, "status", None) or resp.getcode()
            print(f"Message sent. HTTP {status}")
    except urllib.error.HTTPError as exc:
        error_body = exc.read().decode("utf-8", errors="replace")
        print(f"HTTP error: {exc.code} {exc.reason}\n{error_body}", file=sys.stderr)
        if exc.code == 403 and "1010" in error_body:
            print(
                "Hint: 403/1010 means the endpoint is blocking this request (firewall/proxy). "
                "Verify the webhook URL is correct and active, and use the provider's official webhook endpoint.",
                file=sys.stderr,
            )
        sys.exit(1)
    except urllib.error.URLError as exc:
        print(f"Connection error: {exc.reason}", file=sys.stderr)
        sys.exit(1)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Send a message through a webhook URL")
    parser.add_argument(
        "--url",
        default=os.getenv("WEBHOOK_URL"),
        help="Webhook URL (or set WEBHOOK_URL environment variable)",
    )
    parser.add_argument("--message", required=True, help="Message text to send")
    parser.add_argument(
        "--count",
        type=int,
        default=1,
        help="How many times to send the message",
    )
    parser.add_argument(
        "--interval",
        type=float,
        default=1.0,
        help="Seconds between sends (for example: 1, 0.5, 0.1, 0)",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()

    if not args.url:
        print("Missing webhook URL. Pass --url or set WEBHOOK_URL.", file=sys.stderr)
        sys.exit(2)

    if args.count < 1:
        print("--count must be at least 1.", file=sys.stderr)
        sys.exit(2)

    if args.interval < 0:
        print("--interval must be 0 or greater.", file=sys.stderr)
        sys.exit(2)

    for i in range(args.count):
        send_webhook_message(args.url, args.message)
        if i < args.count - 1:
            time.sleep(args.interval)


if __name__ == "__main__":
    main()
