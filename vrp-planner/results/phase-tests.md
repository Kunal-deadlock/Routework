# Phase validation log

Built with portable GCC, C++17, `-O2 -Wall -Wextra -Wpedantic` on Windows.
Each entry was recorded after running the tests, before implementing the next phase.

1. Foundation: PASS, 298 checks (distance symmetry, diagonal, triangle inequality,
   CSV roundtrip, invalid inputs, one customer, full capacity, duplicate coordinates,
   verifier metadata and coverage checks).
2. Baselines: PASS, 302 checks; listed-order and nearest-neighbour verified on
   generated and duplicate-coordinate inputs.
3. Exact: PASS, 618 checks; 27 TSP instances through n=9 cross-checked across
   Held-Karp, subset DP, BT and FIFO/LIFO/LC; 15 multi-vehicle cases through n=7;
   infeasible bin packing, size guards, search limits, known line optimum.
4. Heuristics and supporting structures: PASS, 6,398 checks; heap/sort equivalence,
   Prim/Kruskal equality, MST bounds, all heuristics versus subset DP through n=15,
   2-opt and inter-route local search feasibility and nonincreasing cost.
5. Benchmarking: PASS, 6,432 checks; all dispatch names, counters/timers, invalid
   algorithms/suffixes. CLI gen/solve/compare and 76-row E1-E4/E6-E8 smoke bench
   passed; CSV gaps and MST ratios checked, outputs verified, exit codes checked.
6. Scenario and report: PASS, 6,473 checks after adding multi-vehicle n=8/9 exact
   cross-checks, quoted names/external IDs, malformed/empty CSVs, all algorithms on
   one/full-capacity/zero-distance input, route exports, and full compare variants.
   The first expanded fixture incorrectly called the validated generator with
   random demand above its temporary fleet capacity; the fixture was corrected
   to set its fleet after replacing demands. No phase gate was bypassed.

   Final `make test`: PASS, 6,473 checks, warning-free C++17/O2 build.
   Final `tests/cli.ps1`: PASS, CLI argument validation and exit codes,
   gen/solve/compare, 152 benchmark smoke rows, gap/MST checks and SVG XML.
   Final E1-E8 benchmark: PASS, 2,060 rows (ten runs per size), independent verifier
   on every emitted feasible plan; CSV gaps, MST bounds, proof statuses and all
   SVG XML files checked. 1,821 normal solution rows, 79 feasible search-limit
   rows, 100 isolated-sort rows, and 60 unsupported-size rows.

The final exact cross-checks include the TSP row/column-reduction bound as well as
the CVRP cheapest-outgoing bound. The source test corpus contains 27 seeded TSP
cases through n=9, 15 multi-vehicle cases through n=7 plus n=8/9 cases, and
45 heuristic-versus-subset-DP scenarios through n=15.

Field validation remains unavailable: Nashik coordinates are explicitly synthetic;
no real road-distance or CVRPLIB best-known-value claim is made.
