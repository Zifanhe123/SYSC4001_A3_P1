#!/usr/bin/env python3
"""
metrics_101258593.py

Compute basic scheduling metrics from an execution log produced by
your SYSC4001 Assignment 3 simulator (table with | and + borders).

Usage:
    python3 metrics_101258593.py output_files/execution_EP_test2.txt
"""

import sys


# --------------------------------------------------------------
# Parse execution file
# --------------------------------------------------------------
def parse_execution_file(file_path):
    """
    Parse an execution.txt-style file with table borders, e.g.:

    +------------------------------------------------+
    |Time of Transition |PID | Old State | New State |
    +------------------------------------------------+
    |                 0 |  1 |       NEW |     READY |
    |                 0 |  1 |     READY |   RUNNING |
    ...

    We:
      - skip lines starting with '+'
      - parse lines starting with '|'
      - split by '|' and take columns: time, pid, old_state, new_state
    """
    transitions = []
    with open(file_path, "r") as f:
        lines = f.readlines()

    for line in lines:
        line = line.rstrip("\n")

        # Skip borders like +-----+
        if line.strip().startswith('+'):
            continue

        # Only parse table rows starting with '|'
        if not line.strip().startswith('|'):
            continue

        # Split row into columns
        cols = [c.strip() for c in line.split('|')]

        # Expect something like:
        # ['', '0', '1', 'NEW', 'READY', '']
        if len(cols) < 5:
            continue

        time_str = cols[1]
        pid      = cols[2]
        old_state = cols[3]
        new_state = cols[4]

        # Skip header row
        if time_str.startswith("Time"):
            continue

        # Convert time
        try:
            time = int(time_str)
        except ValueError:
            # Not a numeric time -> skip
            continue

        transitions.append((time, pid, old_state, new_state))

    return transitions


# --------------------------------------------------------------
# Compute metrics
# --------------------------------------------------------------
def compute_metrics(transitions):
    """
    Compute:
      - Throughput
      - Average waiting time
      - Average turnaround time
      - Average response time

    Definitions (per process):
      - arrival time: NEW -> READY transition time
      - first_run:    first time it goes READY/NEW/WAITING -> RUNNING
      - finish_time:  time of TERMINATED transition
      - waiting time: sum of (time in READY state) before each RUNNING
      - turnaround:   finish_time - arrival_time
      - response:     first_run - arrival_time
    """
    arrivals = {}
    first_run = {}
    finish_time = {}
    wait_time = {}
    last_ready_time = {}

    for (time, pid, old, new) in transitions:

        # NEW -> READY: arrival
        if new == "READY" and old == "NEW":
            arrivals[pid] = time
            last_ready_time[pid] = time

        # * -> RUNNING
        if new == "RUNNING":
            # first time it runs
            if pid not in first_run:
                first_run[pid] = time
            # if it was READY before this, add that waiting interval
            if pid in last_ready_time:
                wait_time[pid] = wait_time.get(pid, 0) + (time - last_ready_time[pid])

        # * -> TERMINATED
        if new == "TERMINATED":
            finish_time[pid] = time

        # WAITING -> READY: back into ready queue
        if new == "READY" and old == "WAITING":
            last_ready_time[pid] = time

    turnaround = {}
    response = {}

    for pid in arrivals:
        if pid in finish_time:
            turnaround[pid] = finish_time[pid] - arrivals[pid]
        if pid in first_run:
            response[pid] = first_run[pid] - arrivals[pid]

    if not finish_time:
        raise RuntimeError("No TERMINATED transitions found – file may be empty or malformed.")

    n = len(arrivals) if arrivals else 1
    total_finish_time = max(finish_time.values())

    metrics = {
        "throughput": n / total_finish_time if total_finish_time > 0 else 0.0,
        "avg_wait_time": sum(wait_time.values()) / n if n > 0 else 0.0,
        "avg_turnaround": sum(turnaround.values()) / n if n > 0 else 0.0,
        "avg_response": sum(response.values()) / n if n > 0 else 0.0,
    }
    return metrics


# --------------------------------------------------------------
# Pretty print metrics
# --------------------------------------------------------------
def print_metrics(name, metrics):
    print(f"\n===== Metrics for {name} =====")
    print(f"Throughput:        {metrics['throughput']:.4f} processes/ms")
    print(f"Avg Wait Time:     {metrics['avg_wait_time']:.2f} ms")
    print(f"Avg Turnaround:    {metrics['avg_turnaround']:.2f} ms")
    print(f"Avg Response Time: {metrics['avg_response']:.2f} ms")


# --------------------------------------------------------------
# Main
# --------------------------------------------------------------
def main():
    if len(sys.argv) != 2:
        print("Usage: python3 metrics_101258593.py <execution_file>")
        return

    file_path = sys.argv[1]
    transitions = parse_execution_file(file_path)
    if not transitions:
        print("No valid transitions found in file – check format.")
        return

    metrics = compute_metrics(transitions)
    print_metrics(file_path, metrics)


if __name__ == "__main__":
    main()
