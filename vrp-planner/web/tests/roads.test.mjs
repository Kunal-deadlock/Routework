import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import vm from 'node:vm';
import {createRoadRouter} from '../roads.mjs';
function fakeOSRM(url) {
  const points=decodeURIComponent(url.pathname).split('/').at(-1).split(';').map(p=>p.split(',').map(Number));
  return {ok:true,json:async()=>({code:'Ok',waypoints:points.map(location=>({location,distance:3})),routes:[{legs:points.slice(1).map((b,i)=>({distance:1200,duration:180,steps:[{geometry:{coordinates:[points[i],[(points[i][0]+b[0])/2,(points[i][1]+b[1])/2+.001],b]}}]}))}]})};
}
const stops=[[73.7898,19.9975],[73.8000,20.001],[73.7898,19.9975]];
test('road routes preserve visit order, geometry, distance, duration and snap locations',async()=> {
  let called;
  const route=createRoadRouter({intervalMs:0,fetcher:async(url,options)=>{called={url,options};return fakeOSRM(url);}});
  const result=await route({points:stops});
  assert.equal(result.legs.length,2);assert.equal(result.points.length,5);assert.equal(result.distanceKm,2.4);assert.equal(result.durationMinutes,6);
  assert.deepEqual(result.points[0],[19.9975,73.7898]);assert.deepEqual(result.points.at(-1),[19.9975,73.7898]);
  assert.equal(result.snaps.length,3);assert.equal(result.snaps[0].meters,3);
  assert.equal(called.url.searchParams.get('geometries'),'geojson');assert.equal(called.url.searchParams.get('steps'),'true');assert.match(called.options.headers['User-Agent'],/Routework/);
});
test('identical and concurrent route lookups share the cached request',async()=> {
  let calls=0;const route=createRoadRouter({intervalMs:0,fetcher:async url=>{calls++;return fakeOSRM(url);}});
  await Promise.all([route({points:stops}),route({points:stops})]);await route({points:stops});assert.equal(calls,1);
});
test('large routes are chunked with a shared waypoint and reconstructed without lost legs',async()=> {
  const chunkSizes=[];const route=createRoadRouter({intervalMs:0,fetcher:async url=>{chunkSizes.push(url.pathname.split('/').at(-1).split(';').length);return fakeOSRM(url);}});
  const points=Array.from({length:202},(_,i)=>[73.7+i*.0001,20]);
  const result=await route({points});assert.equal(result.legs.length,201);assert.equal(result.snaps.length,202);assert.ok(chunkSizes.every(n=>n<=50));assert.deepEqual(result.points.at(-1),[20,73.7201]);
});
test('invalid coordinates and unrouteable or malformed responses produce clear errors',async()=> {
  const route=createRoadRouter({intervalMs:0,fetcher:async()=>({ok:false,json:async()=>({code:'NoSegment'})})});
  for(const points of [[],[[0,0]],[[181,0],[0,0]],[[0,NaN],[1,2]],[[0,0,1],[1,2]]]) await assert.rejects(()=>route({points}),{status:400});
  await assert.rejects(()=>route({points:stops}),/too far from a drivable road/);
  const malformed=createRoadRouter({intervalMs:0,fetcher:async()=>({ok:true,json:async()=>({code:'Ok',routes:[{legs:[]}],waypoints:[]})})});
  await assert.rejects(()=>malformed({points:stops}),/incomplete/);
  const offline=createRoadRouter({intervalMs:0,fetcher:async()=>{throw Error('offline');}});await assert.rejects(()=>offline({points:stops}),/could not be reached/);
});
test('different lookups are throttled instead of flooding the public service',async()=> {
  const starts=[];const route=createRoadRouter({intervalMs:50,fetcher:async url=>{starts.push(Date.now());return fakeOSRM(url);}});
  await Promise.all([route({points:stops}),route({points:[[73,20],[74,21]]})]);assert.equal(starts.length,2);assert.ok(starts[1]-starts[0]>=40);
});
test('playback interpolates along the bent road shape and caches browser geometry by stop order',async()=> {
  const source=await readFile(new URL('../public/road-routes.js',import.meta.url),'utf8');let calls=0;
  const sandbox={window:{},fetch:async()=>{calls++;return {ok:true,json:async()=>({legs:[{points:[[0,0],[0,1],[1,1]],distanceKm:2,durationMinutes:1}],points:[[0,0],[0,1],[1,1]],snaps:[],distanceKm:2})};}};
  vm.runInNewContext(source,sandbox);const road=sandbox.window.RoadRoutes,leg={points:[[0,0],[0,1],[1,1]]};
  assert.deepEqual(Array.from(road.position(leg,.25)),[0,.5]);assert.deepEqual(Array.from(road.position(leg,.75)),[.5,1]);
  const input=[{lon:0,lat:0},{lon:1,lat:1}],route={nodes:[0,1]};await road.load(input,route);await road.load(input,route);assert.equal(calls,1);assert.equal(road.peek(input,route).status,'ready');
});
