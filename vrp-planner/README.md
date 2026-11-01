# VRP planner

A C++17 command-line CVRP mini project using only the standard library. It assigns
customers to at most K homogeneous vehicles and minimises symmetric travel distance.
The layout follows Section 10 of the supplied PRD. All six phases were implemented
in order; checks run at each gate are recorded in `results/phase-tests.md`.

## Build and test

Requires a C++17 compiler and GNU Make. Run commands from this folder.

```sh
make
make test
./vrp --help
```

Windows PowerShell with the portable toolchain downloaded beside this project:

```powershell
$env:PATH = (Resolve-Path ..\w64devkit\bin).Path + ';' + $env:PATH
make
make test
.\vrp.exe --help
```

On Windows, use `vrp.exe` in place of `vrp`. On Linux/macOS install GCC or Clang
and GNU Make through your usual package manager. `make CXX=clang++` selects Clang.
No Python, package manager, plotting library, or external solver is needed.
To build without Make:

```sh
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Iinclude src/*.cpp -o vrp
```

## Run

### Browser interface

The local **Routework** interface uses the same C++ executable for optimisation
and verification. Requires Node.js 22+ in addition to the C++ build tools. It has
no npm dependencies or external map/font requests.

On Windows, double-click `start-web.cmd`, or run:

```sh
make web
```

If the executable is already built:

```sh
node web/server.mjs
```

Open **http://127.0.0.1:3000**. Keep the server running while using the interface.
To use another port, set the `PORT` environment variable before starting it.

- Choose the 15- or 30-stop sample, import a CSV, or edit/add/remove stops.
- Set vehicles, capacity and routing method, then press **Plan routes**.
- View geographic routes, zoom/pan, inspect vehicle loads and ordered stops,
  switch to listed order, and play or scrub delivery progress.
- Use **Compare methods** to run all algorithms on the same input. **View route**
  displays any feasible result; exact proofs and search cutoffs remain distinct.
- **Export routes** downloads a route CSV with cumulative distance and arrival time.
- The **Experiments** tab reads the saved E1–E8 observations, with metric selection,
  selectable chart series, per-status means/sample SDs, and raw CSV download.

The map is an offline coordinate view, not street navigation. Distances use the
configured Haversine multiplier. The web adapter caps uploads at 200 customers,
searches at 50,000 expanded/live nodes and 500 ms, and subprocesses at 15 seconds.
It binds to loopback only; this is a local project interface, not a hosted service.
Requests use temporary CSV files that are cleaned up after solving. Uploaded or
edited stops are held in the current browser page; download them before refreshing.
The illustrative Nashik samples remain explicitly synthetic.

```sh
make web-test
```

This runs eight API integration tests against the real executable, covering
route coverage, capacity, visit distances, comparisons/optimality, JSON escaping,
zero distances, infeasible packing, invalid input and local request restrictions.
The original `make test` suite still runs independently. Browser verification and
screenshots are documented in `web/DESIGN.md`.

For other clients, `solve` and `compare` accept `--format json`. JSON output
includes stops, parameters, MST, solver metrics, proof status and ordered visits;
it does not write the default route CSV/SVG files. Text output remains unchanged.

### Command line

```sh
./vrp solve --input data/nashik_tiffin_15.csv --vehicles 3 --capacity 20 --algo cw+2opt
./vrp compare --input data/nashik_tiffin_15.csv --vehicles 3 --capacity 20
./vrp gen --n 30 --type clustered --seed 7 --out data/gen_30.csv
./vrp solve --input data/gen_30.csv --vehicles 6 --capacity 20 --algo sweep
./vrp solve --input data/nashik_tiffin_15.csv --vehicles 3 --capacity 20 --algo exact --routes results/exact_routes.csv --svg results/exact_routes.svg
```

`solve` prints stop IDs, loads, route distances, total distance, runtime, lookup
count, search nodes, MST bound/ratio, and savings relative to listed order. It writes
`routes.csv` and `routes.svg` by default; use `--routes` and `--svg` to choose paths.
Every emitted feasible plan is independently verified. `compare` prints all solver
results on the same input, including optimality gaps when subset DP is supported.

| Algorithm | Description | Guard |
|---|---|---|
| baseline | Input order, capacity splits | Any input size |
| nn | Nearest unvisited customer that fits | Any input size |
| cw | Clarke–Wright endpoint savings merges | Any input size |
| sweep | Angular clusters, Held–Karp up to 12 stops per cluster, otherwise NN + 2-opt | Any input size |
| bt | Backtracking over customer order and route boundaries | n ≤ 9 |
| bb-fifo / bb-lifo / bb-lc | Branch and Bound, queue / stack / custom binary min-heap | n ≤ 12 |
| hk | Held–Karp, a single tour containing all customers | n ≤ 15 and all demand fits one vehicle |
| exact | Held–Karp all-subset tours plus route-count subset DP | n ≤ 15 |

Append `+2opt` or `+local` to baseline, nn, cw, or sweep. `+local` combines 2-opt
with inter-route relocate and swap. `--sort heap` selects the custom in-place heap
sort for savings; `--sort std` is the default. Sort ties use customer indices so
both sorts produce identical plans. The optional packing repair in cw/sweep has
a 100,000-node cap; it is used only when the initial clustering exceeds the fleet.

Exact searches default to `--node-limit 1000000 --time-limit-ms 5000`. The node
limit applies both to expanded nodes and the live frontier. These bounds protect
against exponential growth; an incumbent returned at a limit is **not** labelled
optimal. Lower bounds use cheapest remaining outgoing edges for multi-vehicle
VRP, and row/column matrix reduction for a single tour. B&B starts with a verified
Clarke–Wright incumbent, or a better NN/listed-order incumbent when available.

Exit codes: 0 success, 1 invalid input/options or internal verification failure,
2 unsupported exact size/problem, 3 no feasible plan produced, 4 feasible incumbent
returned at a search limit. `compare` and `bench` keep per-algorithm statuses and
continue past unsupported sizes. A heuristic packing failure does not establish
infeasibility; subset DP or an exhausted exact search does.

## CSV input

```csv
type,id,name,lat,lon,demand,tw_open,tw_close,service
depot,0,Kitchen,19.9975,73.7898,0,0,1440,0
customer,1,"Home, A",20.0010,73.7800,2,0,1440,3
customer,2,Home B,19.9900,73.8000,1,0,1440,3
```

Exactly one depot must come first; at least one customer is required. IDs are
unique integers and may differ from internal indices. Quoted commas and escaped
quotes, UTF-8 BOM, LF and CRLF are supported; embedded newlines in names are not.
The nine-column header is optional. Fleet parameters come from the CLI, defaulting
to 3 vehicles and capacity 20. Empty inputs, duplicate IDs, invalid/nonfinite
coordinates, negative demand, oversized demand, and excessive total demand are
rejected. Total fleet capacity is a necessary condition, not a sufficient packing
test. Zero-demand customers and duplicate coordinates are supported.

Distances are Haversine kilometres × `--circuity` (default 1.3), with Earth radius
6371 km. Input time fields are validated metadata. This CVRP MVP does **not** enforce
time windows. Output arrival minutes start at zero and use 20 km/h plus service
time, without time-window waiting. A user-supplied road matrix, road graph,
reachability checks, and full VRPTW are deferred optional extensions.

## Reproduce E1–E8

The supplied results were generated with:

```sh
./vrp bench --runs 10 --seed 42 --node-limit 100000 --time-limit-ms 500 --out results/results.csv
```

This writes 2,060 rows, `summary.csv` with counts/means/sample standard deviations,
eight SVG graphs, and `run-info.txt`. Output files are overwritten. The same seed
and instance are shared by all algorithms within an experiment. Runtime can vary
between runs; routes and counts are deterministic for completed, node-limited runs.
Time-limited results can vary with hardware load. E5 repeats a fixed dataset ten
times for timing; its distance values are not ten independent observations.

```sh
./vrp bench --sizes 5,8,10,12,15,20,30,50,100 --runs 10 --seed 42 --out results/custom/results.csv
./vrp bench --experiments E6 --sizes 50,100,500,1000 --runs 10 --out results/sorting/results.csv
```

Without `--sizes`, each experiment uses its PRD range. With it, sizes are reused
across experiments, subject to E1/E2 n≤15, E8 n≤12, and E5 n=15/30.

| Experiment | Default sizes | Measurement |
|---|---|---|
| E1 | 5,6,7,8,9,10,12,15 | Exact scaling; K=1 so hk and VRP are comparable |
| E2 | 8,10,12,15 | Six constructive/2-opt gaps to optimum |
| E3 | 20,30,50,100,200,500 | Heuristic runtime and distance growth |
| E4 | 8,15,30,100 | Before/after 2-opt, passes, extra lookup count |
| E5 | 15,30 | Illustrative Nashik listed order versus planner |
| E6 | 50,100,200,500,1000 | Savings sorting only, heap versus std, comparisons |
| E7 | 8,12,15,30,100 | MST ratios and exact optima when supported |
| E8 | 5,8,10,12 | FIFO/LIFO/LC on clustered and near-equal metrics |

`ops` counts instrumented distance-matrix lookups inside a solver, including its
seed construction and final route evaluation. `comparisons` separately counts
savings-sort comparator calls. Matrix construction, MST reporting, verification,
and output writing are outside the solver timer/counter. DP submask checks are
not distance lookups, so `ops` alone cannot measure the subset partitioning cost.
E6 excludes savings construction and checking from its timer, has zero distance
lookups in the sort, and intentionally leaves route fields empty (`sort_only`).
Missing/undefined measurements are empty CSV cells. No zero-distance placeholder
is recorded for unsupported or failed plans. Exact guard rows have status
`unsupported`; search cutoffs have `limit` with `optimal=0`. Never interpret their
runtimes as completed exact solves. Graph E1 and E8 axes use log10(1+y), explicitly
labelled so zero expanded nodes can be shown.

## Data, validation, and report

The Nashik files are **synthetic illustrative coordinates** around the PRD depot,
generated with clustered seed 7. They are not collected addresses or measured road
distances. Their interleaved listed order makes the savings illustrative, not a
claim about actual local delivery operations. See `data/README.md` for provenance.
No external CVRPLIB best-known value or road-distance validation is claimed.

`make test` covers distance properties, CSV errors/roundtrips, hand-checkable
optima, independent exact cross-checks through n=9, all three B&B strategies,
heuristic versus exact consistency, min-heap/heap-sort equivalence, Prim/Kruskal,
verifier failures, capacity/coverage edge cases, timers/counters, and route exports.
The optional Windows CLI regression suite is `powershell -File tests/cli.ps1`.
Read `REPORT.md` for complexity, results and limitations; `DEMO.md` is a short viva
walkthrough. Build artifacts can be removed with `make clean`.
