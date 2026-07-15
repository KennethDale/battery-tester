"""Command-line interface for the battery logger companion tool."""

from __future__ import annotations

import sys
import time
from pathlib import Path
from typing import Optional

import click
from rich.console import Console
from rich.table import Table

from . import __version__
from .client import BatteryTesterClient, BatteryTesterError

console = Console()


def _client(host: str) -> BatteryTesterClient:
    return BatteryTesterClient(host)


def _fmt_time(s: int) -> str:
    if s is None:
        return "—"
    s = int(s)
    h, rem = divmod(s, 3600)
    m, sec = divmod(rem, 60)
    return f"{h}h {m}m {sec}s" if h else f"{m}m {sec}s"


@click.group()
@click.version_option(__version__, prog_name="battery-tester")
@click.option(
    "--host",
    envvar="BATTERY_TESTER_HOST",
    default="batterylogger.local",
    show_default=True,
    help="Hostname or IP of the battery logger (or set BATTERY_TESTER_HOST).",
)
@click.pass_context
def cli(ctx: click.Context, host: str) -> None:
    """Talk to the ESP8266 battery capacity logger from your computer."""
    ctx.obj = {"host": host}


@cli.command()
@click.pass_context
def info(ctx: click.Context) -> None:
    """Show device info."""
    try:
        data = _client(ctx.obj["host"]).info()
    except BatteryTesterError as e:
        console.print(f"[red]error:[/red] {e}")
        sys.exit(1)
    table = Table(title="Device Info", show_header=False)
    for k, v in data.items():
        table.add_row(str(k), str(v))
    console.print(table)


@cli.command()
@click.pass_context
def status(ctx: click.Context) -> None:
    """Print a one-shot snapshot of all channels."""
    try:
        channels = _client(ctx.obj["host"]).status()
    except BatteryTesterError as e:
        console.print(f"[red]error:[/red] {e}")
        sys.exit(1)
    _print_channel_table(channels)


@cli.command()
@click.option("-i", "--interval", default=5.0, show_default=True, help="Seconds between polls.")
@click.pass_context
def watch(ctx: click.Context, interval: float) -> None:
    """Continuously refresh channel status until Ctrl-C."""
    client = _client(ctx.obj["host"])
    try:
        while True:
            try:
                channels = client.status()
            except BatteryTesterError as e:
                console.print(f"[red]error:[/red] {e}")
            else:
                console.clear()
                _print_channel_table(channels)
            time.sleep(interval)
    except KeyboardInterrupt:
        console.print("\n[dim]stopped[/dim]")


@cli.command()
@click.argument("channel", type=int)
@click.pass_context
def reset(ctx: click.Context, channel: int) -> None:
    """Reset CHANNEL (0-indexed) — clear data and return to waiting."""
    try:
        result = _client(ctx.obj["host"]).reset(channel)
    except BatteryTesterError as e:
        console.print(f"[red]error:[/red] {e}")
        sys.exit(1)
    console.print(f"[yellow]reset[/yellow] channel {channel}")
    _print_channel_table([result])


@cli.command()
@click.argument("channel", type=int)
@click.option(
    "-o",
    "--output",
    type=click.Path(path_type=Path),
    default=None,
    help="Write CSV to this file instead of stdout.",
)
@click.pass_context
def export(ctx: click.Context, channel: int, output: Optional[Path]) -> None:
    """Download a CHANNEL's history as CSV."""
    try:
        csv_text = _client(ctx.obj["host"]).history_csv(channel)
    except BatteryTesterError as e:
        console.print(f"[red]error:[/red] {e}")
        sys.exit(1)

    if output is None:
        console.print(csv_text, end="")
        return

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(csv_text, encoding="utf-8")
    console.print(f"[green]wrote[/green] {output} ({len(csv_text)} bytes)")


def _print_channel_table(channels) -> None:
    table = Table(title="Battery Channels")
    table.add_column("Ch", justify="right")
    table.add_column("State")
    table.add_column("INA219", justify="center")
    table.add_column("Voltage", justify="right")
    table.add_column("Current", justify="right")
    table.add_column("Capacity", justify="right")
    table.add_column("Est. final", justify="right")
    table.add_column("Elapsed", justify="right")
    for ch in channels:
        if ch.estimated_capacity_mah is not None:
            est = f"~{ch.estimated_capacity_mah:.0f} mAh"
            if ch.estimated_capacity_wh is not None:
                est += f" · ~{ch.estimated_capacity_wh:.2f} Wh"
        else:
            est = "—"
        table.add_row(
            str(ch.channel),
            ch.state,
            "✓" if ch.connected else "✗",
            f"{ch.voltage:.3f} V",
            f"{ch.current:.3f} A",
            f"{ch.capacity_mah:.0f} mAh",
            est,
            _fmt_time(ch.elapsed_s),
        )
    console.print(table)


def main() -> None:
    cli(obj={})


if __name__ == "__main__":
    main()