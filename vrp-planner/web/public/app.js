/* Routework: browser interaction only. All optimisation and verification run in C++. */
'use strict';
const $ = id => document.getElementById(id);
const colors = ['#1b6c80','#2f70bb','#a86f0c','#56656c'];
const state = {stops:[], result:null, selected:'cw+2opt', vehicle:null, layer:'planned', busy:false, dirty:false, name:'Nashik · 15 stops', sample:true, zoom:1, panX:0, panY:0, progress:0, playing:false, frame:null, experiment:'E1', benchmarkRows:null};
const seriesChoices={};
const titles = {baseline:'Listed order',nn:'Nearest neighbour',cw:'Clarke–Wright',sweep:'Sweep clusters',exact:'Subset DP',hk:'Held–Karp',bt:'Backtracking','bb-fifo':'Branch & bound · FIFO','bb-lifo':'Branch & bound · LIFO','bb-lc':'Branch & bound · least cost'};
const experiments = {
  E1:['Exact scaling','How exact solvers grow from 5 to 15 stops. One vehicle keeps the problem identical across methods.','ms'],
  E2:['Route quality','How far each heuristic is from the proven optimum, across identical seeded instances.','gap_pct'],
  E3:['Heuristic scaling','Runtime and distance growth from 20 to 500 delivery stops.','ms'],
  E4:['Effect of 2-opt','Compare construction with route reversals. The table also retains zero-gain improvements.','distance_km'],
  E5:['Nashik scenario','Listed order versus planned delivery on the fixed, illustrative Nashik samples.','distance_km'],
  E6:['Sorting savings','Heap sort versus std::sort on the same savings. Only sorting is timed.','ms'],
  E7:['MST lower bound','Route distance divided by the minimum spanning tree bound. A ratio is not an optimality gap.','mst_ratio'],
  E8:['Search strategies','FIFO, LIFO and least-cost search on clustered and near-equal-distance instances.','nodes']
};
const esc = value => String(value).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const fmt = (value, digits=2) => value == null || !Number.isFinite(Number(value)) ? '—' : Number(value).toLocaleString('en-IN',{minimumFractionDigits:digits,maximumFractionDigits:digits});
const title = algo => {const [base,extra]=algo.split('+');return (titles[base]||base)+(extra==='2opt'?' + 2-opt':extra==='local'?' + local search':'');};
const hasPlan = solution => Boolean(solution?.verified && solution.routes?.length);
const current = () => state.result?.solutions.find(s=>s.algo===state.selected);
const baseline = () => state.result?.solutions.find(s=>s.algo==='baseline');
const header = ['type','id','name','lat','lon','demand','tw_open','tw_close','service'];
function parseCSV(text) {
  const rows=[];let row=[],cell='',quoted=false,closed=false;
  text=text.replace(/^\uFEFF/,'');
  for(let i=0;i<=text.length;i++) {
    const c=text[i] ?? '\n';
    if(quoted) {if(c==='"') {if(text[i+1]==='"') {cell+='"';i++;}else {quoted=false;closed=true;}}else {if(c==='\n' || c==='\r') throw Error('CSV names cannot contain line breaks.');cell+=c;}continue;}
    if(c==='"' && !cell && !closed) {quoted=true;continue;}
    if(c===',' || c==='\n' || c==='\r') {row.push(cell);cell='';closed=false;if(c!==',') {if(row.some(v=>v!=='')) rows.push(row);row=[];if(c==='\r' && text[i+1]==='\n') i++;}continue;}
    if(closed || c==='"') throw Error('Check the CSV quoting around stop names.');
    cell+=c;
  }
  if(quoted) throw Error('A quoted CSV field was not closed.');
  return rows;
}
function readStops(text) {
  const rows=parseCSV(text);if(rows[0]?.[0]==='type') {if(rows.shift().join(',')!==header.join(',')) throw Error('Use the nine-column CSV template from this project.');}
  if(rows.length<2) throw Error('Add one depot and at least one delivery stop.');
  if(rows.length>201) throw Error('The web planner supports at most 200 customers.');
  const ids=new Set();
  return rows.map((row,i)=> {
    if(row.length!==9) throw Error(`Stop row ${i+1} needs all nine CSV columns.`);
    const stop=Object.fromEntries(header.map((key,j)=>[key,j<3?row[j]:Number(row[j])]));stop.id=Number(row[1]);
    if(row.slice(1).some((v,j)=>j!==1 && v.trim()==='') || !Number.isInteger(stop.id)) throw Error(`Stop row ${i+1} has an invalid ID or missing value.`);
    if(ids.has(stop.id)) throw Error(`Duplicate stop ID: ${stop.id}.`);ids.add(stop.id);
    if(stop.type!==(i===0?'depot':'customer')) throw Error('Keep exactly one depot in the first row, followed by customers.');
    if(!Number.isFinite(stop.lat)||Math.abs(stop.lat)>90||!Number.isFinite(stop.lon)||Math.abs(stop.lon)>180) throw Error(`Check the coordinates for ${stop.name||stop.id}.`);
    if(!Number.isInteger(stop.demand)||stop.demand<0||(i===0&&stop.demand!==0)) throw Error('Use whole, nonnegative demands and zero demand for the depot.');
    for(const key of ['tw_open','tw_close','service']) if(!Number.isInteger(stop[key])||stop[key]<0) throw Error('Time metadata must contain nonnegative whole minutes.');
    if(stop.tw_close<stop.tw_open) throw Error('Closing time must follow opening time.');
    return stop;
  });
}
const quote = value => '"'+String(value).replaceAll('"','""')+'"';
function stopsCSV(stops=state.stops) {return [header.join(','),...stops.map(stop=>header.map(key=>key==='name'?quote(stop[key]):stop[key]).join(','))].join('\n')+'\n';}
function showError(message,element=$('error')) {element.textContent=message;element.hidden=!message;}
function notice(message) {$('notice').textContent=message;$('notice').hidden=!message;}
function markDirty() {
  pause();state.dirty=true;state.result=null;state.vehicle=null;state.progress=0;
  notice('Your stops or settings changed. Plan routes to update the results.');
  render();
}
function updateStopSummary() {
  const demand=state.stops.reduce((sum,s)=>sum+s.demand,0),fleet=Number($('vehicles').value)*Number($('capacity').value);
  $('demand-summary').textContent=`${demand} units to deliver · ${Number.isFinite(fleet)?fleet:'—'} fleet capacity`;
  $('stop-count').textContent=Math.max(0,state.stops.length-1);
  $('page-subtitle').textContent=state.stops.length?`${state.name} · ${state.stops[0].name==='Depot'?'Central depot':state.stops[0].name}`:'Add delivery stops to get started.';
  $('data-provenance').textContent=state.sample?'Illustrative sample · synthetic Nashik coordinates':'Your uploaded or edited delivery coordinates';
  $('dataset-note').textContent=state.sample?'Illustrative sample, around Nashik.':'Your stops stay on this computer.';
}
async function loadSample(size,compare=false) {
  if(state.busy) return;
  try {
    showError('');const response=await fetch('/api/sample?size='+size);const sample=await response.json();if(!response.ok) throw Error(sample.error);
    state.stops=readStops(sample.csv);state.name=sample.name;state.sample=true;state.result=null;state.dirty=false;state.vehicle=null;state.zoom=1;state.panX=state.panY=0;
    $('vehicles').value=sample.vehicles;$('capacity').value=sample.capacity;$('dataset').value=size;
    updateStopSummary();render();await plan(compare);
  }catch(error){showError(error.message);}
}
function getAlgorithm() {const base=$('algorithm').value;return ['baseline','nn','cw','sweep'].includes(base)&&$('improve').checked?base+'+2opt':base;}
function setAlgorithm(algo) {const [base,suffix]=algo.split('+');$('algorithm').value=base;$('improve').checked=Boolean(suffix);updateAlgorithm();}
function updateAlgorithm() {
  const base=$('algorithm').value,constructive=['baseline','nn','cw','sweep'].includes(base);$('improve').disabled=!constructive;
  $('algorithm-note').textContent=base==='cw'?'Join nearby stops, then shorten each route.':base==='nn'?'Choose the closest unserved stop that fits.':base==='sweep'?'Group stops around the depot, then order each group.':base==='baseline'?'Follow CSV order and split routes at capacity.':base==='exact'?'Proves the best fleet plan for up to 15 stops.':base==='hk'?'Exact single tour; all demand must fit one vehicle.':'Small-input exact search with a 500 ms / 50,000-node limit.';
}
function setBusy(busy,compare=false) {
  state.busy=busy;document.body.classList.toggle('loading',busy);document.querySelector('.sheet').setAttribute('aria-busy',String(busy));
  for(const id of ['plan-button','mobile-plan-button','compare-button','edit-button','import-button','dataset','algorithm','vehicles','capacity','circuity']) $(id).disabled=busy;
  $('improve').disabled=busy||!['baseline','nn','cw','sweep'].includes($('algorithm').value);
  $('plan-button').innerHTML=busy?'Planning…':'Plan routes<svg><use href="#i-arrow"/></svg>';
  $('mobile-plan-button').textContent=busy?'Planning…':'Plan routes';
  $('compare-button').textContent=busy&&compare?'Comparing…':'Compare methods';
}
async function plan(compare=false) {
  if(state.busy) return;
  try {
    showError('');pause();updateStopSummary();setBusy(true,compare);
    const payload={csv:stopsCSV(),vehicles:Number($('vehicles').value),capacity:Number($('capacity').value),circuity:Number($('circuity').value),algo:getAlgorithm(),mode:compare?'compare':'solve'};
    // Never leave a previous plan displayed as if it used newly submitted settings.
    state.result=null;state.vehicle=null;state.progress=0;render();notice('Planning and checking your delivery routes…');
    const response=await fetch('/api/solve',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)});
    const result=await response.json();if(!response.ok) throw Error(result.error || 'The planner could not run.');
    state.result=result;state.selected=payload.algo;state.vehicle=null;state.dirty=false;state.progress=0;state.zoom=1;state.panX=state.panY=0;
    const selected=current();notice('');
    if(!hasPlan(selected)) showError(selected?.message||'No feasible route was produced. Try another method or increase fleet capacity.');
    else if(selected.status==='limit') notice('Search limit reached. This plan is feasible, but its optimality is not proven.');
    render();
  }catch(error){notice('');showError(error.message || 'Could not connect to the planner. Keep the local server running.');}
  finally{setBusy(false);$('sidebar').classList.remove('open');}
}
function render() {updateStopSummary();renderMetrics();renderVehicles();renderComparison();renderMap();}
function renderMetrics() {
  const solution=current(),base=baseline(),valid=hasPlan(solution),saved=valid&&hasPlan(base)?Number(base.total.toFixed(2))-Number(solution.total.toFixed(2)):null;
  $('total-distance').innerHTML=valid?`${fmt(solution.total)}<em>km</em>`:'—';
  $('distance-caption').textContent=valid?title(solution.algo):'Plan a route to see its distance';
  $('saved-distance').innerHTML=saved===null?'—':`${fmt(saved)}<em>km</em>`;$('saved-distance').classList.toggle('saving',saved>0.005);
  $('saved-caption').textContent=saved===null?'Same stops, same capacity':base.total>0?`${fmt(100*saved/base.total,1)}% less than listed order`:'Listed-order distance is zero';
  $('fleet-used').innerHTML=valid?`${solution.routes.length}<em>/ ${state.result.instance.vehicles}</em>`:'—';
  const demand=state.stops.reduce((sum,s)=>sum+s.demand,0);
  $('fleet-caption').textContent=valid?`${demand} demand units · all stops covered`:'Within your available fleet';
  $('planning-time').innerHTML=valid?`${fmt(solution.ms,2)}<em>ms</em>`:'—';
  $('time-caption').textContent=valid?`${fmt(solution.ops,0)} distance lookups`:'Measured by the C++ engine';
  $('export-button').disabled=!valid;$('play-button').disabled=!valid;$('playback-range').disabled=!valid;
}
function renderVehicles() {
  const solution=state.layer==='baseline'?baseline():current();
  if(!hasPlan(solution)) {$('vehicle-list').innerHTML='<p class="empty">Plan routes to assign stops to vehicles.</p>';$('verification').classList.add('unverified');$('verification').querySelector('span').textContent='Waiting for a verified plan';return;}
  const stops=state.result.instance.stops,capacity=state.result.instance.capacity;
  $('vehicle-list').innerHTML=solution.routes.map((route,i)=> {
    const selected=state.vehicle===i;
    const visits=route.visits.map((visit,j)=>`<li><span>${j===0?'Start · ':j===route.visits.length-1?'Return · ':j+'. '}${esc(stops[visit.index].name)}</span><small>${fmt(visit.km)} km</small></li>`).join('');
    return `<article class="vehicle-card${selected?' selected':''}" style="--route-color:${colors[i%colors.length]}"><button class="vehicle-top" data-vehicle="${i}" aria-expanded="${selected}"><span class="route-swatch"></span><strong>Vehicle ${i+1}</strong><span class="route-distance">${fmt(route.dist)} km</span></button><p class="vehicle-facts">${route.nodes.length-2} stops · starts & ends at depot</p><div class="capacity-track" role="meter" aria-label="Vehicle ${i+1} load" aria-valuenow="${route.load}" aria-valuemin="0" aria-valuemax="${capacity}"><span style="width:${100*route.load/capacity}%"></span></div><div class="capacity-label"><span>Load ${route.load} / ${capacity}</span><span>${fmt(100*route.load/capacity,0)}% full</span></div>${selected?'<ol class="route-detail">'+visits+'</ol>':''}</article>`;
  }).join('');
  $('verification').classList.remove('unverified');$('verification').querySelector('span').textContent=solution.optimal?'Verified · proven optimal':'Verified · every stop, within capacity';
}
function renderComparison() {
  if(!state.result || state.result.solutions.length<3) {$('comparison-body').innerHTML='<tr><td colspan="7" class="empty">Compare methods to see their distances, work, and proof status.</td></tr>';$('comparison-note').textContent='Compare distance and effort on exactly the same stops.';return;}
  const optimal=state.result.solutions.find(s=>hasPlan(s)&&s.optimal);
  $('comparison-note').textContent=optimal?`Best proven: ${fmt(optimal.total)} km · ${title(optimal.algo)}. Select a method to view its routes.`:'Select a method to view its routes. A proven optimum is unavailable at this size.';
  $('comparison-body').innerHTML=state.result.solutions.map(s=> {
    const valid=hasPlan(s),gap=valid&&optimal?(optimal.total>0?100*(s.total-optimal.total)/optimal.total:0):null;
    const label=s.optimal&&valid?'Optimal':s.status==='limit'?'Search limit':valid?'Feasible':s.status==='unsupported'?'Size / scope limit':'No plan';
    return `<tr${s.algo===state.selected?' class="current-result"':''}><td>${esc(title(s.algo))}</td><td class="num">${valid?fmt(s.total)+' km':'—'}</td><td class="num">${gap===null?'—':fmt(Math.max(0,gap),2)+'%'}</td><td class="num">${fmt(s.ms)} ms</td><td class="num">${fmt(s.ops,0)}</td><td><span title="${esc(s.message||label)}" class="status-pill ${s.optimal&&valid?'proven':s.status==='limit'?'limit':''}">${label}</span></td><td>${valid?`<button class="use-button" data-algo="${esc(s.algo)}">${s.algo===state.selected?'Viewing':'View route'}</button>`:''}</td></tr>`;
  }).join('');
}
let projection=null;
function renderMap() {
  const svg=$('route-map');const stops=state.result?.instance.stops || state.stops;
  if(!stops.length) {svg.innerHTML='<text x="400" y="200" text-anchor="middle" fill="#5d5f60" font-size="14">Add a depot and delivery stops to begin.</text>';return;}
  const latitudes=stops.map(s=>s.lat),longitudes=stops.map(s=>s.lon),centerLat=(Math.min(...latitudes)+Math.max(...latitudes))/2,centerLon=(Math.min(...longitudes)+Math.max(...longitudes))/2;
  const cos=Math.max(.05,Math.cos(centerLat*Math.PI/180));
  const W=svg.clientWidth || 800,H=svg.clientHeight || 326,cx=W/2,cy=H/2-7;
  const spanX=Math.max(.006,Math.max(...longitudes)-Math.min(...longitudes))*cos,spanY=Math.max(.006,Math.max(...latitudes)-Math.min(...latitudes));
  const scale=Math.min((W-100)/spanX,(H-110)/spanY),x=lon=>cx+(lon-centerLon)*cos*scale,y=lat=>cy-(lat-centerLat)*scale;
  projection={x,y,invert:(px,py)=>({lat:centerLat+(cy-py)/scale,lon:centerLon+(px-cx)/(cos*scale)})};
  const width=W/state.zoom,height=H/state.zoom;svg.setAttribute('viewBox',`${cx-width/2+state.panX} ${H/2-height/2+state.panY} ${width} ${height}`);
  const solution=state.layer==='baseline'?baseline():current(),routes=hasPlan(solution)?solution.routes:[];
  let grid='';
  for(let i=0;i<4;i++) {const yy=40+i*(H-75)/3,lat=projection.invert(0,yy).lat;grid+=`<path d="M0 ${yy}H${W}" stroke="#dce5e0" stroke-width=".8"/><text x="8" y="${yy-6}" font-size="11" fill="#61776d">${Math.abs(lat).toFixed(3)}° ${lat>=0?'N':'S'}</text>`;}
  for(let i=0;i<3;i++) {const xx=65+i*(W-130)/2,lon=projection.invert(xx,0).lon;grid+=`<path d="M${xx} 0V${H}" stroke="#dce5e0" stroke-width=".8"/><text x="${xx+4}" y="18" font-size="11" fill="#61776d">${Math.abs(lon).toFixed(3)}° ${lon>=0?'E':'W'}</text>`;}
  const markerDefs=colors.map((color,i)=>`<marker id="arrow-${i}" viewBox="0 0 10 10" refX="7" refY="5" markerWidth="4" markerHeight="4" orient="auto-start-reverse"><path d="M0 1 8 5 0 9" fill="none" stroke="${color}" stroke-width="1.5"/></marker>`).join('');
  const assigned=new Map();let links='';
  routes.forEach((route,i)=> {
    for(const index of route.nodes) if(index) assigned.set(index,i);
    const opacity=state.vehicle===null||state.vehicle===i?1:.13;
    for(let j=1;j<route.nodes.length;j++) {const a=stops[route.nodes[j-1]],b=stops[route.nodes[j]],ax=x(a.lon),ay=y(a.lat),bx=x(b.lon),by=y(b.lat),color=state.layer==='baseline'?'#6b7871':colors[i%4];
      links+=`<path d="M${ax} ${ay}L${bx} ${by}" fill="none" stroke="${color}" stroke-width="2.3" opacity="${opacity}"${i>=4?' stroke-dasharray="7 4"':''}/>`;
      if(Math.hypot(ax-bx,ay-by)>32&&state.layer!=='baseline') links+=`<path d="M${(ax+bx)/2} ${(ay+by)/2}L${ax*.45+bx*.55} ${ay*.45+by*.55}" fill="none" stroke="${color}" stroke-width="2" marker-end="url(#arrow-${i%4})" opacity="${opacity}"/>`;
    }
  });
  const occupied=stops.map(stop=>({left:x(stop.lon)-8,right:x(stop.lon)+8,top:y(stop.lat)-8,bottom:y(stop.lat)+8})),labels=new Map();
  occupied.push({left:x(stops[0].lon)+10,right:x(stops[0].lon)+55,top:y(stops[0].lat)-8,bottom:y(stops[0].lat)+10});
  const intersects=(a,b)=>a.left<b.right+2&&a.right>b.left-2&&a.top<b.bottom+2&&a.bottom>b.top-2;
  for(let i=1;i<stops.length;i++) {
    if(stops.length>51) break;
    const xx=x(stops[i].lon),yy=y(stops[i].lat),text=String(stops[i].id),w=text.length*7;
    for(const [dx,dy] of [[10,-7],[10,15],[-10-w,-7],[-10-w,15],[-w/2,-19],[-w/2,26],[20,3],[-22-w,3],[16,-21],[-16-w,26]]) {
      const box={left:xx+dx,right:xx+dx+w,top:yy+dy-11,bottom:yy+dy+2};
      if(box.left<8||box.right>W-8||box.top<22||box.bottom>H-42||occupied.some(b=>intersects(box,b))) continue;
      labels.set(i,{x:xx+dx,y:yy+dy});occupied.push(box);break;
    }
  }
  const markers=stops.map((stop,i)=> {
    const xx=x(stop.lon),yy=y(stop.lat),route=assigned.get(i),opacity=state.vehicle===null||route===state.vehicle||i===0?1:.25,color=route===undefined?'#596d65':colors[route%4];
    const label=labels.get(i);
    return i===0?`<g data-stop="0" tabindex="0" role="button" aria-label="Depot ${esc(stop.name)}"><rect x="${xx-9}" y="${yy-9}" width="18" height="18" rx="4" fill="#243d33" stroke="white" stroke-width="3"/><path d="m${xx-4} ${yy+2}v-4l4-3 4 3v4Z" stroke="white" fill="none" stroke-width="1"/><text x="${xx+15}" y="${yy+5}" font-size="12" font-weight="500" fill="#243d33">Depot</text><title>${esc(stop.name)} · depot</title></g>`:`<g data-stop="${i}" tabindex="0" role="button" aria-label="${esc(stop.name)}, demand ${stop.demand}" opacity="${opacity}"><circle cx="${xx}" cy="${yy}" r="6" fill="white" stroke="${color}" stroke-width="2.5"/>${label?`<text x="${label.x}" y="${label.y}" fill="${color}" font-size="11">${stop.id}</text>`:''}<title>${esc(stop.name)} · ${stop.demand} units</title></g>`;
  }).join('');
  const bar=W<500?50:80,km=bar/scale*111.195;
  svg.innerHTML=`<defs>${markerDefs}</defs><g aria-hidden="true">${grid}</g>${links}${markers}<g aria-hidden="true"><path d="M20 ${H-47}v5h${bar}v-5" stroke="#61776d" fill="none"/><text x="${20+bar/2}" y="${H-53}" text-anchor="middle" font-size="11" fill="#4a6159">${fmt(km,1)} km</text></g><circle id="courier" cx="0" cy="0" r="8" fill="#131416" stroke="white" stroke-width="3" visibility="hidden"/>`;
  updatePlayback();
}
function routeLegs() {
  const solution=state.layer==='baseline'?baseline():current(),legs=[];
  if(!hasPlan(solution)) return legs;
  solution.routes.forEach((route,i)=> {if(state.vehicle!==null&&state.vehicle!==i) return;for(let j=1;j<route.nodes.length;j++) legs.push({from:route.nodes[j-1],to:route.nodes[j],vehicle:i,sequence:j});});return legs;
}
function updatePlayback() {
  const legs=routeLegs(),p=state.progress/1000*legs.length,index=Math.min(Math.floor(p),legs.length-1),leg=legs[index];
  $('playback-range').value=state.progress;
  if(!leg||!projection) {$('playback-label').textContent='Ready to follow the route';return;}
  const a=state.result.instance.stops[leg.from],b=state.result.instance.stops[leg.to],t=Math.min(1,p-index),point=$('courier');
  if(point) {point.setAttribute('cx',projection.x(a.lon)+(projection.x(b.lon)-projection.x(a.lon))*t);point.setAttribute('cy',projection.y(a.lat)+(projection.y(b.lat)-projection.y(a.lat))*t);point.setAttribute('visibility',state.progress>0?'visible':'hidden');}
  $('playback-label').textContent=state.progress===0?'Ready to follow the route':state.progress===1000?'All routes complete':`V${leg.vehicle+1} → ${b.index===0?'depot':'stop '+b.id}`;
}
function pause() {state.playing=false;if(state.frame) cancelAnimationFrame(state.frame);state.frame=null;$('play-button').textContent='▶';$('play-button').setAttribute('aria-label','Play delivery route');}
function play() {
  if(state.playing) return pause();if(!hasPlan(current())) return;
  if(state.progress>=1000) state.progress=0;state.playing=true;$('play-button').textContent='Ⅱ';$('play-button').setAttribute('aria-label','Pause delivery route');let previous;
  const tick=time=> {if(!state.playing) return;if(previous!==undefined) state.progress=Math.min(1000,state.progress+(time-previous)*1000/Math.max(8000,routeLegs().length*700));previous=time;updatePlayback();if(state.progress===1000) return pause();state.frame=requestAnimationFrame(tick);};state.frame=requestAnimationFrame(tick);
}
let draft=[];
function openEditor() {draft=structuredClone(state.stops);showError('',$('editor-error'));renderEditor();$('stops-dialog').showModal();}
function renderEditor() {
  $('stop-editor').innerHTML=draft.map((s,i)=>`<tr><td class="stop-id">${s.id}${i===0?'<span class="table-sub">Depot</span>':''}</td>${['name','lat','lon','demand'].map(key=>`<td><input aria-label="${esc(key)} for ${i===0?'depot':'stop '+s.id}" data-index="${i}" data-field="${key}" value="${esc(s[key])}" ${key==='name'?'type="text" maxlength="120"':`type="number" step="${key==='demand'?'1':'any'}" ${key==='demand'?'min="0"':key==='lat'?'min="-90" max="90"':'min="-180" max="180"'}`} required${i===0&&key==='demand'?' readonly':''}></td>`).join('')}<td>${i?`<button class="icon-button" data-remove="${i}" type="button" aria-label="Remove stop ${s.id}">×</button>`:''}</td></tr>`).join('');
}
function download(name,content,type) {const url=URL.createObjectURL(new Blob([content],{type})),a=document.createElement('a');a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}
function exportRoutes() {
  const solution=current();if(!hasPlan(solution)) return;const stops=state.result.instance.stops;
  const rows=['vehicle,sequence,stop_id,name,cumulative_km,arrival_minutes'];
  solution.routes.forEach((r,v)=>r.visits.forEach((visit,j)=>rows.push([v+1,j,stops[visit.index].id,quote(stops[visit.index].name),visit.km.toFixed(6),visit.arrival.toFixed(2)].join(','))));
  download('routes.csv',rows.join('\n')+'\n','text/csv');
}
async function showView(view) {
  $('planner-view').hidden=view!=='planner';$('experiments-view').hidden=view!=='experiments';
  // Keep exactly one semantic page heading for the currently active screen.
  for(const id of ['planner','experiments']) {
    const heading=$(id+'-view').querySelector('.page-heading h1, .page-heading h2'),tag=id===view?'H1':'H2';
    if(heading.tagName!==tag) {const replacement=document.createElement(tag.toLowerCase());replacement.textContent=heading.textContent;heading.replaceWith(replacement);}
  }
  document.querySelectorAll('[data-view]').forEach(button=>button.classList.toggle('active',button.dataset.view===view));
  if(view==='experiments') {
    $('sidebar').classList.remove('open');
    try {if(!state.benchmarkRows) {const response=await fetch('/api/benchmarks');if(!response.ok) throw Error('Run the benchmark command to create results.csv.');const rows=parseCSV(await response.text()),keys=rows.shift();state.benchmarkRows=rows.map(row=>Object.fromEntries(keys.map((key,i)=>[key,row[i]])));}renderExperiment();}
    catch(error){$('experiment-chart').innerHTML=`<p class="empty">${esc(error.message)}</p>`;}
  }
}
function renderExperiment() {
  const exp=state.experiment,[name,description]=experiments[exp],metric=$('experiment-metric').value;
  $('experiment-title').textContent=name;$('experiment-description').textContent=description;
  document.querySelectorAll('[data-experiment]').forEach(button=>button.classList.toggle('active',button.dataset.experiment===exp));
  const rows=state.benchmarkRows?.filter(row=>row.experiment===exp)||[],groups=new Map();
  for(const row of rows) {const key=[row.algorithm,exp==='E6'?row.sort:'',row.type,row.n,row.status].join('|');if(!groups.has(key)) groups.set(key,{row,values:[]});if(row[metric]!==''&&Number.isFinite(Number(row[metric]))) groups.get(key).values.push(Number(row[metric]));}
  const aggregates=[...groups.values()].map(({row,values})=> {const mean=values.length?values.reduce((a,b)=>a+b,0)/values.length:null;const sd=values.length>1?Math.sqrt(values.reduce((sum,v)=>sum+(v-mean)**2,0)/(values.length-1)):values.length?0:null;return {row,mean,sd,count:values.length};}).sort((a,b)=>Number(a.row.n)-Number(b.row.n)||a.row.algorithm.localeCompare(b.row.algorithm));
  const series=new Map(),sizes=[...new Set(rows.map(row=>Number(row.n)))].sort((a,b)=>a-b);
  for(const a of aggregates) {if(a.mean===null||a.row.status==='unsupported') continue;const key=(a.row.algorithm==='savings-sort'?a.row.sort:title(a.row.algorithm))+(exp==='E8'?' / '+a.row.type:'')+(a.row.status==='limit'?' (cutoff)':'');if(!series.has(key)) series.set(key,new Map());series.get(key).set(Number(a.row.n),a.mean);}
  $('experiment-summary').innerHTML=`<span><strong>${rows.length.toLocaleString()}</strong> observations</span><span><strong>${sizes.join(', ')}</strong> stops</span><span><strong>${rows.filter(r=>r.status==='limit').length}</strong> search cutoffs</span>`;
  if(!seriesChoices[exp]) {
    const preferred=exp==='E1'?['bt','bb-lc','hk','exact'].map(title):exp==='E2'||exp==='E7'?['nn+2opt','cw+2opt','sweep+2opt'].map(title):exp==='E3'?['nn','cw','sweep'].map(title):exp==='E4'?['nn','nn+2opt','cw','cw+2opt'].map(title):exp==='E5'?['baseline','cw+2opt','nn+2opt','sweep+2opt'].map(title):[...series.keys()].slice(0,4);
    seriesChoices[exp]=new Set(preferred.filter(key=>series.has(key)));
  }
  const selected=seriesChoices[exp];
  $('series-options').innerHTML=[...series.keys()].map(key=>`<label><input type="checkbox" data-series="${esc(key)}"${selected.has(key)?' checked':''}${!selected.has(key)&&selected.size>=4?' disabled':''}>${esc(key)}</label>`).join('');
  const chartSeries=[...series].filter(([key])=>selected.has(key)).map(([name,values],i)=>({name,values:sizes.map(n=>values.get(n)??null),colour:colors[i]}));
  const xAt=[0],plotWidth=Math.max(100,$('experiment-chart').clientWidth-70),span=Math.max(1,sizes.at(-1)-sizes[0]);
  sizes.forEach((n,i)=>{if(i&&(n-sizes[xAt.at(-1)])/span*plotWidth>=52) xAt.push(i);});
  if(sizes.length>1&&xAt.at(-1)!==sizes.length-1) {if((sizes.at(-1)-sizes[xAt.at(-1)])/span*plotWidth<30&&xAt.length>1) xAt.pop();xAt.push(sizes.length-1);}
  if(chartSeries.length) {Charts.line($('experiment-chart'),{labels:sizes.map(String),xValues:sizes,xAt,series:chartSeries,format:value=>fmt(value,metric==='nodes'||metric==='comparisons'?0:2),legend:$('experiment-legend')});}
  else {$('experiment-chart').innerHTML=`<p class="empty">${series.size?'Choose at least one chart series below.':'No measurements for this metric in this experiment. Choose another measure.'}</p>`;$('experiment-legend').textContent='';}
  $('measure-heading').textContent=$('experiment-metric').selectedOptions[0].textContent;
  $('experiment-body').innerHTML=aggregates.map(a=>`<tr><td>${esc(a.row.algorithm==='savings-sort'?'Savings · '+a.row.sort:title(a.row.algorithm))}<span class="table-sub">${esc(a.row.type)}</span></td><td class="num">${a.row.n}</td><td class="num">${a.count}</td><td class="num">${fmt(a.mean,metric==='nodes'||metric==='comparisons'?0:3)}</td><td class="num">${fmt(a.sd,metric==='nodes'||metric==='comparisons'?0:3)}</td><td><span class="status-pill${a.row.status==='limit'?' limit':''}">${esc(a.row.status==='sort_only'?'Sort only':a.row.status==='unsupported'?'Size limit':a.row.status==='limit'?'Search cutoff':'Completed')}</span></td></tr>`).join('');
}
// Event delegation keeps rebuilt map, vehicle, editor and comparison rows interactive.
$('plan-form').addEventListener('submit',event=>{event.preventDefault();plan(false);});
$('mobile-plan-button').addEventListener('click',()=>plan(false));
$('compare-button').addEventListener('click',()=>plan(true));
$('dataset').addEventListener('change',event=> {if(event.target.value==='custom') openEditor();else loadSample(event.target.value,true);});
$('import-button').addEventListener('click',()=>$('csv-file').click());
$('csv-file').addEventListener('change',async event=> {try {const file=event.target.files[0];if(!file) return;if(file.size>128000) throw Error('Use a CSV smaller than 128 KB.');state.stops=readStops(await file.text());state.name=file.name.replace(/\.csv$/i,'');state.sample=false;$('dataset').value='custom';state.zoom=1;state.panX=state.panY=0;markDirty();showError('');}catch(error){showError(error.message);}finally{event.target.value='';}});
for(const id of ['vehicles','capacity','circuity']) $(id).addEventListener('change',markDirty);
$('algorithm').addEventListener('change',()=>{updateAlgorithm();if(state.result) notice('Routing method changed. Plan routes to apply it.');});
$('improve').addEventListener('change',()=>{if(state.result) notice('Improvement setting changed. Plan routes to apply it.');});
$('edit-button').addEventListener('click',openEditor);$('close-editor').addEventListener('click',()=>$('stops-dialog').close());
$('add-stop').addEventListener('click',()=>{if(draft.length>=201) return showError('The web planner supports 200 customers.',$('editor-error'));const id=Math.max(0,...draft.map(s=>s.id))+1,depot=draft[0];draft.push({type:depot?'customer':'depot',id:depot?id:0,name:depot?'Delivery '+id:'Depot',lat:depot?.lat??19.9975,lon:depot?.lon??73.7898,demand:depot?1:0,tw_open:0,tw_close:1440,service:3});renderEditor();$('stop-editor').lastElementChild?.scrollIntoView({block:'nearest'});});
$('stop-editor').addEventListener('input',event=>{const {index,field}=event.target.dataset;if(index===undefined) return;draft[Number(index)][field]=field==='name'?event.target.value:Number(event.target.value);});
$('stop-editor').addEventListener('click',event=>{const button=event.target.closest('[data-remove]');if(button) {draft.splice(Number(button.dataset.remove),1);renderEditor();}});
$('stops-form').addEventListener('submit',event=>{event.preventDefault();try {const checked=readStops(stopsCSV(draft));state.stops=checked;state.name='Custom delivery';state.sample=false;$('dataset').value='custom';state.zoom=1;state.panX=state.panY=0;markDirty();$('stops-dialog').close();}catch(error){showError(error.message,$('editor-error'));}});
$('download-stops').addEventListener('click',()=>download('delivery-stops.csv',stopsCSV(draft),'text/csv'));
$('export-button').addEventListener('click',exportRoutes);
$('vehicle-list').addEventListener('click',event=>{const button=event.target.closest('[data-vehicle]');if(!button) return;pause();const index=Number(button.dataset.vehicle);state.vehicle=state.vehicle===index?null:index;state.progress=0;renderVehicles();renderMap();});
$('all-routes').addEventListener('click',()=>{pause();state.vehicle=null;state.progress=0;renderVehicles();renderMap();});
$('comparison-body').addEventListener('click',event=>{const button=event.target.closest('[data-algo]');if(!button) return;pause();state.selected=button.dataset.algo;setAlgorithm(state.selected);state.vehicle=null;state.layer='planned';state.progress=0;setLayer('planned');render();});
function setLayer(layer) {pause();state.layer=layer;state.vehicle=null;state.progress=0;$('planned-layer').classList.toggle('selected',layer==='planned');$('baseline-layer').classList.toggle('selected',layer==='baseline');$('planned-layer').setAttribute('aria-pressed',String(layer==='planned'));$('baseline-layer').setAttribute('aria-pressed',String(layer==='baseline'));renderVehicles();renderMap();}
$('planned-layer').addEventListener('click',()=>setLayer('planned'));$('baseline-layer').addEventListener('click',()=>setLayer('baseline'));
$('play-button').addEventListener('click',play);$('playback-range').addEventListener('input',event=>{pause();state.progress=Number(event.target.value);updatePlayback();});
function zoom(factor) {state.zoom=Math.max(.5,Math.min(8,state.zoom*factor));renderMap();}
$('zoom-in').addEventListener('click',()=>zoom(1.3));$('zoom-out').addEventListener('click',()=>zoom(1/1.3));$('fit-map').addEventListener('click',()=>{state.zoom=1;state.panX=state.panY=0;renderMap();});
let drag=null,moved=false;
$('route-map').addEventListener('pointerdown',event=>{if(event.target.closest('[data-stop]')) return;drag={x:event.clientX,y:event.clientY,panX:state.panX,panY:state.panY};moved=false;event.currentTarget.setPointerCapture(event.pointerId);});
$('route-map').addEventListener('pointermove',event=>{if(!drag) return;state.panX=drag.panX-(event.clientX-drag.x)/state.zoom;state.panY=drag.panY-(event.clientY-drag.y)/state.zoom;moved=true;renderMap();});
$('route-map').addEventListener('pointerup',()=>{drag=null;});$('route-map').addEventListener('pointercancel',()=>{drag=null;});
function selectStop(event) {const marker=event.target.closest('[data-stop]');if(!marker||moved) {moved=false;return;}const index=Number(marker.dataset.stop),s=(state.result?.instance.stops||state.stops)[index];$('map-tooltip').innerHTML=`${esc(s.name)}<small>${index===0?'Depot':s.demand+' demand units'} · ${fmt(s.lat,5)}, ${fmt(s.lon,5)}</small>`;$('map-tooltip').hidden=false;}
$('route-map').addEventListener('click',selectStop);$('route-map').addEventListener('keydown',event=>{if(event.key==='Enter'||event.key===' ') {event.preventDefault();selectStop(event);}});
$('route-map').addEventListener('pointerleave',()=>{$('map-tooltip').hidden=true;});
$('settings-toggle').addEventListener('click',()=>$('sidebar').classList.toggle('open'));$('settings-close').addEventListener('click',()=>$('sidebar').classList.remove('open'));
document.querySelectorAll('[data-view]').forEach(button=>button.addEventListener('click',()=>{location.hash=button.dataset.view;}));
window.addEventListener('hashchange',()=>showView(location.hash==='#experiments'?'experiments':'planner'));
$('experiment-nav').innerHTML=Object.entries(experiments).map(([id,[name]])=>`<button class="experiment-tab${id==='E1'?' active':''}" data-experiment="${id}"><span>${id}</span>${name}</button>`).join('');
$('experiment-nav').addEventListener('click',event=>{const button=event.target.closest('[data-experiment]');if(button){state.experiment=button.dataset.experiment;$('experiment-metric').value=experiments[state.experiment][2];renderExperiment();}});
$('experiment-metric').addEventListener('change',renderExperiment);
$('series-options').addEventListener('change',event=>{const key=event.target.dataset.series;if(key===undefined) return;const selected=seriesChoices[state.experiment];event.target.checked?selected.add(key):selected.delete(key);renderExperiment();});
new ResizeObserver(()=>{if(!$('planner-view').hidden) renderMap();}).observe($('map-stage'));
fetch('/api/health').then(response=>{if(!response.ok) throw Error();$('connection').textContent='connected';}).catch(()=>{$('connection').textContent='offline';showError('Start the local web server to connect to the C++ planner.');});
updateAlgorithm();loadSample('15',true).then(()=>{if(location.hash==='#experiments') showView('experiments');});
