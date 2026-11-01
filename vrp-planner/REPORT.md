# Smart local delivery and collection route planner

## Problem and implementation

Given one depot, n customers with nonnegative demand, K identical vehicles and
capacity Q, find at most K depot-to-depot routes of minimum total distance.
Every customer must appear exactly once and every route load must be at most Q.
Distances are symmetric. The implementation is C++17 using the standard library.

The decision version is in NP: coverage, capacity and route distance can be checked
in polynomial time. TSP reduces to CVRP by choosing K=1 and Q at least the sum of
unit demands. A distance-budget TSP tour exists exactly when the resulting CVRP
plan meets that budget. Thus the decision version is NP-complete and optimisation
is NP-hard. Exact search is suitable only for small instances.

The foundation loads CSV, rejects malformed inputs, constructs a dense Haversine
matrix, and independently verifies every exported plan. The generator uses a
fixed `mt19937` seed. Depot and customer CSV IDs are preserved separately from
internal indices. Timing uses `steady_clock`; the solver interface returns a
common Solution containing routes, total distance, metrics and proof status.

## Algorithms and correctness

The baseline follows listed customer order, splitting at capacity. NN repeatedly
selects the closest unvisited customer that fits. Both can produce too many routes,
which is reported as a heuristic failure rather than proven infeasibility.

Clarke–Wright starts with singleton routes and sorts savings
`d(0,i)+d(0,j)-d(i,j)`. It merges only endpoints of distinct routes whose combined
load fits. Reversal aligns the endpoints before concatenation. Sweep sorts polar
angles about the depot, fills capacity clusters, then solves clusters of at most
12 customers using Held–Karp. Larger clusters use NN and 2-opt. If cw/sweep exceed
the fleet, a bounded descending-demand packing search attempts repair; failure
is explicitly reported. Repair is an exceptional fallback, not part of the
classical O(n² log n) bound for Clarke–Wright.

2-opt reverses an intra-route segment only when the four-edge delta is negative.
It preserves customer assignment and load. Relocate and swap search between
routes, accept only capacity-feasible distance reductions, and rerun 2-opt. These
are local optima; no global quality guarantee is claimed.

Backtracking enumerates unvisited next customers and route closures subject to
capacity and fleet size. Each completed route must contain the smallest customer
still unserved when it opened. This removes equivalent route orderings without
restricting order inside a route. Partial cost and remaining fleet capacity prune
branches. Search exhaustion proves optimality or infeasibility.

Branch and Bound adds an admissible completion bound. For CVRP, every unvisited
customer and the current endpoint must leave via an edge to the depot or another
unvisited customer. Summing their cheapest possible outgoing edges underestimates
completion cost; additional depot departures are deliberately omitted. For K=1,
row/column reduction of the remaining path's assignment matrix is used instead.
Ignoring subtours weakens the relaxation but cannot overestimate a valid tour.
FIFO, LIFO and a custom array-backed min-heap provide the three live-node orders.
Clarke–Wright, NN and listed order provide feasible upper bounds. Search limits
return a feasible incumbent with `optimal=0`, or a no-solution-found limit status.

Held–Karp stores shortest depot-to-endpoint paths for each visited subset and
reconstructs via predecessor pointers. The same table gives the best closed tour
for every subset. Subset VRP DP chooses a capacity-feasible submask containing the
lowest remaining bit, and combines it with a smaller residual mask and one fewer
available route. The route-count dimension enforces at most K, including when
total demand alone does not establish packability. Optimality follows by considering
the route containing that lowest bit in any optimal residual partition.

The union of vehicle routes is connected and spans all customers and the depot.
Removing cycles yields a spanning tree of no greater length; therefore the MST is
a valid lower bound. Both Prim and Kruskal with union-find are implemented and
cross-checked. MST ratios are quality indicators, not optimality gaps.

## Complexity and measurement

| Algorithm | Time | Additional memory |
|---|---|---|
| Distance matrix | Θ(n²) | Θ(n²) |
| Baseline | Θ(n) | O(n) |
| NN | Θ(n²) candidate scans | O(n) |
| Clarke–Wright | O(n² log n), excluding bounded packing fallback | O(n²) |
| Sweep | O(n log n) plus per-cluster solver | O(n) plus cluster DP |
| 2-opt | Θ(r²) edge tests per pass on r stops; number of passes varies | O(1) |
| Relocate/swap | O(n²) candidate tests per pass, plus accepted-move updates | O(n) |
| Held–Karp | Θ(n² 2ⁿ) | Θ(n 2ⁿ) |
| Subset VRP DP | O(K 3ⁿ + n² 2ⁿ) | O((n+K) 2ⁿ) |
| Backtracking | O(n! 2ⁿ) conservative bound with arbitrary route boundaries | O(n²) here, because child states copy paths |
| Branch and Bound | Same exponential search space, O(n²) bound work per state | O(n × live nodes) |
| Heap push/pop | O(log m) | O(m) for m entries |
| Heap sort | O(m log m) worst case | O(1) |
| Prim | Θ(n²) | O(n) |
| Kruskal | O(n² log n) for a complete graph | O(n²) |

The PRD's factorial-search shorthand suppresses route-boundary choices. A direct
permutation/boundary implementation has at most n!·2^(n−1) encodings; with fixed K
the boundary factor is at most O(n^(K−1)). Strong pruning often reduces measured
nodes, but does not change that conservative upper bound. For one vehicle the
usual O(n!) enumeration applies. Similarly, the K dimension must be included in
the subset-DP complexity rather than silently omitted.

Best/average/worst measured runtimes depend on pruning, clustering, capacity and
machine load. NN performs Θ(n²) scans in all cases; construction sort uses a
standard O(m log m) comparison bound. Heap sort has an O(m log m) worst-case
guarantee, but all-equal keys can require fewer comparisons in this implementation;
the experiment uses a deterministic total tie order. Held–Karp's state enumeration
does not depend on distances. 2-opt's best case is one quadratic pass; its worst
case needs many passes. Search has an Ω(1) expanded-node case when an incumbent
matches the root bound, and exponential worst-case growth. Examples of strict
asymptotic separation: n log n = o(n²), and n! = ω(2ⁿ).

`ops` counts only distance-matrix lookups; comparisons in savings sorting have a
separate counter. DP partition enumeration has no distance lookups, so its
exponential work is visible in runtime rather than fully represented by `ops`.
Verifier, matrix construction, MST reporting and file I/O are outside solver timing.
E6 measures sorting only. Times below a millisecond are noisy and should not be
used to claim meaningful rankings from a single run.

## Experiment design and evidence

The shipped run uses ten seeds, 42–51, `-O2`, a 100,000 expanded/live-node cap and
a 500 ms search cap. `results/run-info.txt` records the settings. Raw observations
are in `results/results.csv`; `results/summary.csv` provides metric sample counts,
means and sample standard deviations, grouped by experiment, size, distribution,
algorithm, sort and status. The eight `E*.svg` plots show the respective means.
Search-limit observations are censored; unsupported sizes have empty route metrics.
E1 uses a single vehicle so all applicable exact methods solve the same problem.

| Experiment | Rows | Evidence |
|---|---:|---|
| E1 | 480 | Exact scaling n=5..15, preserving unsupported and limit statuses |
| E2 | 240 | Heuristic gaps n=8,10,12,15 |
| E3 | 360 | Heuristic runtime/distance n=20..500 |
| E4 | 240 | Before/after 2-opt, passes and extra operations |
| E5 | 100 | Repeated timings on fixed illustrative Nashik data |
| E6 | 100 | Savings sorting n=50..1000, time and comparisons |
| E7 | 300 | MST ratios n=8..100, optimum through n=15 |
| E8 | 240 | B&B strategies n=5,8,10,12 on clustered and near-equal metrics |

E2 at n=15 gives mean gaps: NN 20.3123%, NN+2-opt 16.6114%, Clarke–Wright
1.5500%, Clarke–Wright+2-opt 0.8381%, and Sweep/Sweep+2-opt 8.7946%.
The observed small cw+2opt gap is a result for these ten samples, not a guarantee.
Sweep's internal cluster solver already optimises small clusters, so an additional
2-opt pass often makes no change. E4 records these zero-gain cases as well.

E3's n=200 runs finish comfortably below one second for every heuristic. E6 compares
sorting 499,500 savings at n=1000; use summary.csv for the machine-specific means
and comparison counts. E7 confirms all feasible distances exceed the MST bound.
E8 shows that live-frontier exhaustion can stop FIFO/LC before the expanded-node
cap, and that clustered versus near-equal inputs do not give a universal strategy
ranking. Check per-status sample counts before comparing mean timings.

| Illustrative scenario | Listed order (km) | cw+2opt (km) | Saving (km) | Saving (%) |
|---|---:|---:|---:|---:|
| Nashik 15 stops | 147.996685 | 37.823631 | 110.173053 | 74.442920 |
| Nashik 30 stops | 302.415134 | 63.915288 | 238.499847 | 78.865050 |

The 15-stop cw+2opt distance matches the subset-DP optimum on this sample.
These are synthetic clustered stops with an interleaved listed order. The figures
demonstrate functionality only; they do not measure an actual business saving.
E5's ten repeats are timing repetitions, not independent dataset samples.

## Validation and remaining limits

The phase log records successive tests. Final tests independently cross-check
Held–Karp, BT, subset DP and all B&B orders through n=9, including CVRP partitions;
test known optima, invalid CSV, verifier tampering, one stop, capacity equality,
infeasible packing despite adequate total fleet capacity, zero distances, sort
equivalence, MST equality, local improvement monotonicity, timing/counters and
exports. A separate CLI suite checks argument errors, exit codes, unsupported sizes,
search-limit honesty, 152 benchmark rows, gap/MST consistency and SVG XML.

No real-address dataset, measured road-distance matrix or published CVRPLIB
comparison was supplied; those field-validation parts of the PRD remain unverified.
Time windows are metadata, not constraints. Optional road graphs, external road
matrices and metaheuristics are deferred. The implemented MVP, supporting data
structures, experiment runner and illustrative scenario are complete and runnable.
