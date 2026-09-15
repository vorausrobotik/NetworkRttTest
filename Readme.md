# Overview
This Repository contains a small benchmark for Network communication.
It can be used to answer whether a system is real-time capable enough, to be used for EtherCAT or similar fieldbuses with the voraus software.
At the moment, a EtherCAT device is needed as a loopback device.

## Basic procedure
A single Ethernet frame is sent repeatedly with a given cycletime.
The testframe contains an EtherCAT command that reads the timestamp of the first device, when the packet arrives at the slave (register 0x910).
The time until the response arrives is measured (round trip time (rtt)) and recorded in a histogram.

The cycle of the benchmark is synchronized to the slave time in such a way, that this timestamp should be as close as possible to time % cycletime.
This is done by shorten or lengthen the cycle slightly with a PI regulator, similar to a PLL.
The error is also recorded in a histogram.
This should get rid of "predictable" kind of shifts and leaves only the non-static jitter as an error.

# Usage
Build the release binary and run it as root on the interface that is connected to the EtherCAT device:

```sh
python pipe.py configure_release build_release
sudo ./pipe-release/src/NetworkRttTest -i eth0 -c 1000 -p 90 --cpu-affinity 2 -m 1 -s 1
```

Useful options (see `--help` for all):
- `-i, --interface`: network interface to use (required)
- `-c, --cycletime`: cycle time in microseconds
- `-p, --priority`: SCHED_FIFO priority of the benchmark thread, `-1` for SCHED_OTHER
- `--cpu-affinity`: pin the benchmark thread to this CPU
- `-m, --lock-memory` / `-s, --prevent-sleep-states`: typical real-time tuning
- `-r, --results-path`: directory for the result files, defaults to `results` in the working directory
- `--create-results-subdir`: create a timestamped subdirectory inside the results path for each run

The results directory is created if it does not exist. To avoid overwriting previous results, the benchmark aborts
if the directory is not empty. Either pass a different `--results-path`, clear the directory, or use
`--create-results-subdir 1` to get a new subdirectory per run.

Statistics are printed to the console while the benchmark runs. Stop it with Ctrl+C.

# Results
The runtime of the general operations are measured and then stored in a histogram using RtBenchmarks.
The following benchmarks exists:

## send / send.json
The duration of the send syscall. This should be somewhat close to zero.

## delta / jitter_at_ecat_device.json
The difference between planned arrival of the frame at the slave and actual arrival.
The values here somewhat reflect the frame jitter including the OS and hardware parts.
The average of this histogram should be very close to zero as this is what the regulator regulates.

## rtt / round_trip.json
The roundtrip time of the packet.


# Development

Dependencies are managed with conan, the build with CMake presets.
`python pipe.py` runs the complete pipeline: format check, debug build with sanitizers and coverage, unit tests, release build and clang-tidy.
Single stages can be run by name, e.g. `python pipe.py build_debug test_unit`.
The `Dockerfile` provides a basic development environment, `Dockerfile.prod` builds a minimal image containing only the binary.
