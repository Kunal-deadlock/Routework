// OSRM road geometry for an already planned stop order. C++ optimisation stays local.
const bad=(message,status=400)=>Object.assign(new Error(message),{status});
const pause=ms=>new Promise(resolve=>setTimeout(resolve,ms));
export function createRoadRouter({fetcher=fetch,baseURL=process.env.ROUTING_BASE_URL||'https://router.project-osrm.org',intervalMs=1100}={}) {
  const base=new URL(baseURL);if(!['https:','http:'].includes(base.protocol)) throw Error('ROUTING_BASE_URL must be an HTTP(S) OSRM endpoint.');
  const cache=new Map(),pending=new Map();let queue=Promise.resolve(),lastStart=0;
  function validate(points) {
    if(!Array.isArray(points)||points.length<2||points.length>202) throw bad('A road route needs 2 to 202 ordered coordinates.');
    return points.map(point=> {
      if(!Array.isArray(point)||point.length!==2||!point.every(Number.isFinite)||Math.abs(point[0])>180||Math.abs(point[1])>90) throw bad('Road coordinates must be valid longitude, latitude pairs.');
      return point.map(n=>Number(n.toFixed(6)));
    });
  }
  async function request(points) {
    const key=JSON.stringify(points),hit=cache.get(key);
    if(hit&&hit.expires>Date.now()) {cache.delete(key);cache.set(key,hit);return hit.value;}
    if(pending.has(key)) return pending.get(key);
    if(pending.size>=16) throw bad('Road routing is busy. Wait for the current paths to finish.',429);
    const task=queue.then(async()=> {
      await pause(Math.max(0,lastStart+intervalMs-Date.now()));lastStart=Date.now();
      const coordinates=points.map(p=>p.join(',')).join(';');
      const url=new URL(base.pathname.replace(/\/$/,'')+'/route/v1/driving/'+coordinates,base.origin);
      url.search=new URLSearchParams({steps:'true',overview:'false',geometries:'geojson',alternatives:'false',generate_hints:'false',radiuses:points.map(()=>2000).join(';')});
      let response,data;
      try {
        response=await fetcher(url,{signal:AbortSignal.timeout(15000),headers:{'User-Agent':'Routework/1.0 (+https://github.com/rehan-03/DSA)','Accept':'application/json'}});
        data=await response.json();
      }catch {throw bad('The road-routing service could not be reached. Try again or use estimated links.',502);}
      if(!response.ok||data.code!=='Ok'||!data.routes?.[0]) throw bad(data.code==='NoSegment'?'A stop is too far from a drivable road. Check its coordinates.':'No drivable route was found between these stops.',502);
      const route=data.routes[0];
      if(!Array.isArray(route.legs)||route.legs.length!==points.length-1||!Array.isArray(data.waypoints)||data.waypoints.length!==points.length) throw bad('The road service returned incomplete route data.',502);
      const legs=route.legs.map(leg=> {
        if(!Number.isFinite(leg.distance)||leg.distance<0||!Number.isFinite(leg.duration)||leg.duration<0) throw bad('The road service returned invalid route measurements.',502);
        const path=[];
        for(const step of leg.steps||[]) for(const point of step.geometry?.coordinates||[]) {
          if(!Array.isArray(point)||point.length<2||!point.slice(0,2).every(Number.isFinite)||Math.abs(point[0])>180||Math.abs(point[1])>90) throw bad('The road service returned invalid geometry.',502);
          const latlon=[point[1],point[0]],last=path.at(-1);if(!last||last[0]!==latlon[0]||last[1]!==latlon[1]) path.push(latlon);
        }
        if(!path.length) throw bad('The road service returned no road geometry.',502);
        return {points:path,distanceKm:leg.distance/1000,durationMinutes:leg.duration/60};
      });
      const snaps=data.waypoints.map(point=> {
        if(!Array.isArray(point.location)||point.location.length!==2||!point.location.every(Number.isFinite)||!Number.isFinite(point.distance)||point.distance<0) throw bad('The road service returned invalid snapped stops.',502);
        return {lat:point.location[1],lon:point.location[0],meters:point.distance};
      });
      const value={legs,snaps};cache.set(key,{value,expires:Date.now()+3600000});
      while(cache.size>64) cache.delete(cache.keys().next().value);
      return value;
    });
    queue=task.catch(()=>{});pending.set(key,task);
    try{return await task;}finally{pending.delete(key);}
  }
  return async payload=> {
    const points=validate(payload?.points),legs=[],snaps=[];
    // OSRM requests remain below 50 coordinates, with one shared chunk boundary.
    for(let start=0;start<points.length-1;start+=49) {
      const chunk=await request(points.slice(start,start+50));legs.push(...chunk.legs);snaps.push(...(start?chunk.snaps.slice(1):chunk.snaps));
    }
    const geometry=[];
    for(const leg of legs) for(const point of leg.points) {const last=geometry.at(-1);if(!last||last[0]!==point[0]||last[1]!==point[1]) geometry.push(point);}
    return {source:'OSRM',profile:'driving',points:geometry,legs,snaps,distanceKm:legs.reduce((s,l)=>s+l.distanceKm,0),durationMinutes:legs.reduce((s,l)=>s+l.durationMinutes,0)};
  };
}
