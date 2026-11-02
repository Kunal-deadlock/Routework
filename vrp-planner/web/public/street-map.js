/* Leaflet/OpenStreetMap display adapter; OSRM road shapes arrive from the local API. */
(() => {
  let map, tiles, drawings, courier, enabled=false, fittedKey='', drawnKey='', lastStops=[], lastRoutes=[], lastRoads=[],lastVehicle=null;
  const tileURL='https://tile.openstreetmap.org/{z}/{x}/{y}.png';
  const el=id=>document.getElementById(id);
  function popup(stop,index,vehicle,snap) {
    const box=document.createElement('div'),name=document.createElement('strong'),detail=document.createElement('p');
    name.textContent=stop.name;
    detail.textContent=index===0?'Depot · start and return':`Stop ${stop.id} · ${stop.demand} demand units${vehicle===undefined?'':` · Vehicle ${vehicle+1}`}`;
    const coordinates=document.createElement('small');coordinates.textContent=`${stop.lat.toFixed(5)}, ${stop.lon.toFixed(5)}`;
    box.append(name,detail,coordinates);if(snap&&snap.meters>5) {const note=document.createElement('small');note.textContent=`Snapped ${Math.round(snap.meters)} m to a drivable road`;box.append(document.createElement('br'),note);}return box;
  }
  function init() {
    if(map) return;
    map=L.map('street-map',{zoomControl:false,attributionControl:true,minZoom:1,maxZoom:19,worldCopyJump:true}).setView([19.9975,73.7898],12);
    tiles=L.tileLayer(tileURL,{
      maxZoom:19,keepBuffer:0,updateWhenIdle:true,
      attribution:'&copy; <a href="https://www.openstreetmap.org/copyright" target="_blank" rel="noopener">OpenStreetMap contributors</a>'
    });
    // Request only visible tiles. Browser caching and the normal Referer are preserved.
    tiles.on('tileerror',()=>{if(enabled) el('map-network-note').hidden=false;});
    tiles.on('tileload',()=>{if(enabled) el('street-map').dataset.tilesLoaded='true';});
    drawings=L.layerGroup().addTo(map);
    L.control.scale({imperial:false,position:'bottomleft'}).addTo(map);
    map.attributionControl.addAttribution('<a href="https://project-osrm.org/" target="_blank" rel="noopener">OSRM routing</a>');
  }
  function setEnabled(value) {
    enabled=value;
    if(!map) return;
    if(value&&!map.hasLayer(tiles)) tiles.addTo(map);
    if(!value) {if(map.hasLayer(tiles)) map.removeLayer(tiles);el('map-network-note').hidden=true;}
  }
  function fit() {
    if(!map||!lastStops.length) return;
    const nodes=lastVehicle!==null&&lastRoutes[lastVehicle]?lastRoutes[lastVehicle].nodes.map(i=>lastStops[i]):lastStops;
    const roadPoints=lastRoads.flatMap((r,i)=>r?.status==='ready'&&(lastVehicle===null||lastVehicle===i)?r.points:[]);
    map.fitBounds(L.latLngBounds([...nodes.map(s=>[s.lat,s.lon]),...roadPoints]),{padding:[35,35],maxZoom:16,animate:false});
  }
  function render(stops,routes,vehicle,colors,baseline=false,roads=[]) {
    init();setEnabled(true);map.invalidateSize({pan:false,animate:false});
    lastStops=stops;lastRoutes=routes;lastRoads=roads;lastVehicle=vehicle;
    if(!stops.length) {drawings.clearLayers();setCourier(null,false);drawnKey='';return;}
    const key=JSON.stringify([stops.map(s=>[s.id,s.lat,s.lon,s.name,s.demand]),routes.map(r=>r.nodes),roads.map(r=>[r?.status,r?.distanceKm,r?.points?.length]),vehicle,baseline]);
    if(key!==drawnKey) {
      drawnKey=key;drawings.clearLayers();map.closePopup();
      const assigned=new Map(),snapped=new Map();
      routes.forEach((route,i)=>{
        route.nodes.forEach(index=>{if(index) assigned.set(index,i);});
        const road=roads[i],ready=road?.status==='ready';if(ready) route.nodes.forEach((index,j)=>snapped.set(index,road.snaps[j]));
        // Separate legs avoid inventing a straight join between independently snapped chunk boundaries.
        const points=ready?road.legs.map(leg=>leg.points):route.nodes.map(index=>[stops[index].lat,stops[index].lon]),active=vehicle===null||vehicle===i;
        L.polyline(points,{color:'#ffffff',weight:6,opacity:active?0.85:0.15,interactive:false}).addTo(drawings);
        L.polyline(points,{color:baseline?'#56656c':colors[i%colors.length],weight:3,opacity:active?0.95:0.15,dashArray:ready?null:'7 5',interactive:false}).addTo(drawings);
      });
      stops.forEach((stop,index)=> {
        const owner=assigned.get(index),active=vehicle===null||owner===vehicle||index===0;
        const color=index===0?'#243d33':owner===undefined?'#596d65':colors[owner%colors.length];
        const snap=snapped.get(index),location=snap?[snap.lat,snap.lon]:[stop.lat,stop.lon];
        if(snap?.meters>5) L.polyline([[stop.lat,stop.lon],location],{color:'#6b7871',weight:1,dashArray:'2 4',opacity:active?0.7:0.1,interactive:false}).addTo(drawings);
        const marker=L.circleMarker(location,{radius:index===0?9:6,color:index===0?'white':color,weight:index===0?3:2.5,fillColor:index===0?color:'white',fillOpacity:active?1:.3,opacity:active?1:.3}).addTo(drawings);
        marker.bindPopup(popup(stop,index,owner,snap),{maxWidth:250});
        const label=document.createElement('span');label.textContent=index===0?`Depot · ${stop.name}`:`${stop.id} · ${stop.name}`;
        marker.bindTooltip(label,{direction:'top'});
        const node=marker.getElement();node.setAttribute('tabindex','0');node.setAttribute('role','button');node.setAttribute('aria-label',index===0?`Depot ${stop.name}`:`Stop ${stop.id}: ${stop.name}, demand ${stop.demand}`);
        node.addEventListener('keydown',event=>{if(event.key==='Enter'||event.key===' ') {event.preventDefault();marker.openPopup();}});
      });
    }
    const fitKey=JSON.stringify(stops.map(s=>[s.lat,s.lon]));
    if(fittedKey!==fitKey) {fittedKey=fitKey;fit();}
  }
  function setCourier(point,visible) {
    if(!map) return;
    if(!visible||!point) {if(courier&&map.hasLayer(courier)) map.removeLayer(courier);return;}
    if(!courier) courier=L.circleMarker(point,{radius:8,color:'white',weight:3,fillColor:'#131416',fillOpacity:1,interactive:false});
    courier.setLatLng(point);if(!map.hasLayer(courier)) courier.addTo(map);courier.bringToFront();
  }
  window.StreetMap={
    available:()=>typeof window.L!=='undefined',render,setEnabled,fit,setCourier,
    zoomIn:()=>map?.zoomIn(),zoomOut:()=>map?.zoomOut()
  };
})();
