# Routework interface notes

Style: framed-sheet, dealt with seed **397299373** by the frontend design skill.
No redraws. A green-grey local-delivery frame, a white sheet, locally bundled IBM
Plex Sans, and a branching route mark give the product its identity. The signature
element is the real depot-to-customer route drawing paired with capacity tracks
and delivery playback. Data hues identify vehicles; chrome stays neutral.

The planner's question is “How should these stops be served within this fleet?”
Its biggest region is the geographic route view, next to loads and ordered visits.
The experiments screen asks “How did routing quality and effort change?” It uses
the actual saved CSV, numeric size axes, selectable methods and per-status tables.
Cutoffs and completed searches are never merged into one statistical table row.

Sample data: `data/nashik_tiffin_15.csv` and `data/nashik_tiffin_30.csv` are synthetic.
No addresses, road shapes, traffic, business savings, or current experiment runs
are invented. The experiment page shows recorded observations, not live reruns.
The browser summary subtracts displayed two-decimal distances so the displayed
distance savings reconcile; exports retain higher-precision measurements and
identify whether distances come from the estimate matrix or OSRM driving paths.

Checks: the C++ suite, eight real-engine API tests, and browser interactions for
editing, replanning, selecting a vehicle, capacity rejection, route playback,
listed-order comparison and experiment navigation. Screenshots are captured at
1440, 768 and 390 pixels by the skill inspector. SVG is appropriate here: the map
shows the actual coordinates and computed paths, rather than decorative imagery.

The lint's monospace warning is intentionally retained for stop IDs and experiment
IDs, as the app-screen rules allow. Em dashes mean missing measurements, not prose
punctuation. The app inspector is the controlling check for this app interface.
Its remaining small-text warning covers axis ticks, metadata and compact status
labels; routing controls and numeric table values use 13px or larger text.

The live-map extension preserves this design and adds locally bundled Leaflet
1.9.4 (BSD-2-Clause, `public/vendor/leaflet/LICENSE`) with attributed OpenStreetMap
tiles. Road shapes come from OSRM via the local server. Solid coloured paths are
road geometry; dashed coloured links are estimates, and fine dotted links show
snapping from original coordinates to drivable roads. Raw tile colours are map
data, not additional interface accent colours. Both coordinate view and tile-error
fallback remain available. Playback follows the road geometry per delivery leg.

The main distance metric is explicitly a planner estimate. Road totals, driving
minutes, vehicle cards and exported CSVs identify their different source. The
estimate optimiser's proof status does not claim optimality on road distances.
The recorded E1–E8 data has not been reinterpreted as road-network measurements.

Local font files are IBM Plex, redistributed under the SIL Open Font License in
`public/fonts/OFL.txt`. `charts.js` starts from the design skill's app chart helper;
the local version adds numeric size spacing and matching numeric-axis hover.

Final results: 6,473 C++ checks and the existing CLI regression suite passed.
All eight web API tests passed. Browser tests confirmed the mobile Plan routes
button, the 18-method comparison, edited names in computed visit order, capacity
errors clearing stale results, the 30-stop/four-vehicle sample, and an actual
CSV download with 30 distinct customers and eight depot endpoints.
Both screen inspections passed with zero FAIL findings at all three widths.

Road-map extension validation: all 16 web/routing tests pass, and all 6,473 C++
checks still pass. The real synthetic 15-stop lookup returned 717 road vertices
for vehicle 1 and 404 for vehicle 2, with 11 and 6 delivery legs respectively.
The real road distances were 32.9054 km and 14.1173 km; the downloaded CSV retained
their sum, 47.0227 km, with `osrm_driving` source labels. The visible road total
sums the displayed two-decimal vehicle distances. Browser checks cover road / link
switching, road-following playback, offline coordinate fallback and CSV export.
Desktop, tablet and phone inspections pass; Leaflet's default scale text shadow
was removed to preserve the existing flat interface style.
