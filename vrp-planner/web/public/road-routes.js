// Cache road geometry by ordered coordinates, never by a vehicle number or label.
(() => {
  const cache=new Map(),distances=new WeakMap();
  const key=(stops,route)=>JSON.stringify(route.nodes.map(i=>[stops[i].lon,stops[i].lat]));
  function peek(stops,route) {return cache.get(key(stops,route));}
  async function load(stops,route) {
    const id=key(stops,route),existing=cache.get(id);if(existing) return existing.promise||existing;
    const entry={status:'loading'};cache.set(id,entry);
    entry.promise=(async()=> {
      try {
        const response=await fetch('/api/road-route',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({points:route.nodes.map(i=>[stops[i].lon,stops[i].lat])})});
        const result=await response.json();if(!response.ok) throw Error(result.error||'Road routing failed.');
        if(result.legs.length!==route.nodes.length-1) throw Error('Road geometry does not match this route.');
        Object.assign(entry,result,{status:'ready'});
      }catch(error){entry.status='error';entry.message=error.message;}
      finally{delete entry.promise;}
      return entry;
    })();
    return entry.promise;
  }
  // Interpolate along the returned road shape, rather than between customer coordinates.
  function position(leg,fraction) {
    const points=leg.points;if(points.length<2) return points[0];
    if(!distances.has(leg)) {
      const lengths=[0];
      for(let i=1;i<points.length;i++) {const [a,b]=[points[i-1],points[i]],cos=Math.cos((a[0]+b[0])*Math.PI/360);lengths.push(lengths.at(-1)+Math.hypot(a[0]-b[0],(a[1]-b[1])*cos));}
      distances.set(leg,lengths);
    }
    const lengths=distances.get(leg),target=Math.max(0,Math.min(1,fraction))*lengths.at(-1);
    let i=1;while(i<lengths.length-1&&lengths[i]<target) i++;
    const t=(target-lengths[i-1])/(lengths[i]-lengths[i-1]||1);
    return [points[i-1][0]+(points[i][0]-points[i-1][0])*t,points[i-1][1]+(points[i][1]-points[i-1][1])*t];
  }
  window.RoadRoutes={peek,load,position,clearErrors:()=>{for(const [id,value] of cache) if(value.status==='error') cache.delete(id);}};
})();
