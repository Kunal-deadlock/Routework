# Dataset provenance

## K. K. Wagh import sample

`kkwagh_sample_10.csv` contains one depot at K. K. Wagh Institute of Engineering
Education & Research, Amrutdham, Panchavati, Nashik, and ten fictional delivery stops.
The approximate depot coordinate (20.0135914, 73.8222614) comes from the map embedded
on the college's [official contact page](https://www.kkwagh.edu.in/engineering/contact-us).
It is a campus map point, not a surveyed vehicle entrance. Delivery locations and
demands are illustrative sample data, not real customers.

Import it using **Import CSV**. Suggested settings: **2 vehicles**, **capacity 10**
each, **Clarke–Wright**, and **2-opt** enabled. Total demand is 18 units. Every
vehicle starts and ends at the depot automatically; include the depot only once
in the input CSV, rather than repeating it as the last row.

```sh
./vrp solve --input data/kkwagh_sample_10.csv --vehicles 2 --capacity 10 --algo cw+2opt
```

## Seeded Nashik samples

`nashik_tiffin_15.csv` and `nashik_tiffin_30.csv` are **illustrative synthetic**
datasets, not real customers or map-collected delivery stops. No private addresses
or observed road distances are included.

They use the sample depot from the PRD (19.9975, 73.7898), synthetic clusters around
that point, integer demands 1–3, and seed 7. The first 15 customers of the 30-stop
generator match the 15-stop dataset. Names are generic. The listed order alternates
between clusters; it was not obtained from a real delivery route.

Recreate from the project directory:

```sh
./vrp gen --n 15 --type clustered --seed 7 --vehicles 3 --capacity 20 --out data/nashik_tiffin_15.csv
./vrp gen --n 30 --type clustered --seed 7 --vehicles 5 --capacity 20 --out data/nashik_tiffin_30.csv
```

Distances are computed from coordinates each time. Benchmark E8's hard instances
instead use off-diagonal distances uniformly in [1,1.0001), symmetric with zero
diagonal. That valid synthetic metric intentionally exposes weak bounds; it is
not derived from the CSV coordinates and is only available in the benchmark.

For a field study, replace these files with consented/public stop coordinates,
actual demand, and a measured usual-order route. Actual road-matrix comparison and
published benchmark validation remain future work; no such evidence was supplied.
