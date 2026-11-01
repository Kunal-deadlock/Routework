# Five-minute demo

1. Run `make test` and show the passing phase log in `results/phase-tests.md`.
2. Run `./vrp solve --input data/nashik_tiffin_15.csv --vehicles 3 --capacity 20 --algo baseline`.
3. Repeat with `--algo cw+2opt`. Explain each saving as the two depot legs replaced
   by a direct customer connection. Show capacity checks and the routes SVG.
4. Run `./vrp compare --input data/nashik_tiffin_15.csv --vehicles 3 --capacity 20`.
   Explain why bt and B&B refuse this size, and why hk is only a single-route solver.
   Subset DP provides the multi-route optimum and heuristic gaps.
5. Open `results/E2.svg`, `results/E6.svg`, and `results/E8.svg`; show the matching
   raw rows in `results/results.csv`. Explain limits before comparing runtimes.
6. Run `./vrp gen --n 8 --type uniform --seed 42 --out data/demo_8.csv`, then compare
   with one vehicle, capacity 30. All five exact approaches should agree.

Viva prompts: Held–Karp reuses subset/end-point states; subset DP adds a vehicle
count; 2-opt is a local optimum; a heap orders live nodes without proving their
quality; the union of vehicle routes contains a spanning tree, so MST is a lower
bound. TSP reduces to CVRP by using one vehicle and sufficient capacity. A feasible
plan is verifiable in polynomial time; finding the optimum is NP-hard.
