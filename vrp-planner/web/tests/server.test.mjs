import { test, before, after } from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { createApp } from '../server.mjs';
let server,base,csv;
before(async()=> {
  csv=await readFile(fileURLToPath(new URL('../../data/nashik_tiffin_15.csv',import.meta.url)),'utf8');
  server=createApp();await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));base=`http://127.0.0.1:${server.address().port}`;
});
after(async()=> {server.closeAllConnections();await new Promise(resolve=>server.close(resolve));});
const post=payload=>fetch(base+'/api/solve',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({csv,vehicles:3,capacity:20,algo:'cw+2opt',...payload})});
function verifyJSON(data,s) {
  assert.equal(s.verified,true);assert.ok(s.routes.length<=data.instance.vehicles);
  const seen=[];let total=0;
  for(const r of s.routes) {
    assert.equal(r.nodes[0],0);assert.equal(r.nodes.at(-1),0);
    const customers=r.nodes.slice(1,-1);seen.push(...customers);
    const load=customers.reduce((sum,i)=>sum+data.instance.stops[i].demand,0);
    assert.equal(load,r.load);assert.ok(load<=data.instance.capacity);
    assert.equal(r.nodes.length,r.visits.length);assert.ok(Math.abs(r.visits.at(-1).km-r.dist)<1e-8);
    for(let i=1;i<r.visits.length;i++) {assert.ok(r.visits[i].km>=r.visits[i-1].km);assert.ok(r.visits[i].arrival>=r.visits[i-1].arrival);}
    total+=r.dist;
  }
  assert.deepEqual(seen.sort((a,b)=>a-b),data.instance.stops.slice(1).map(s=>s.index));assert.ok(Math.abs(total-s.total)<1e-8);assert.ok(s.total+1e-8>=data.mst);
}
test('static interface, fonts and health are served locally',async()=> {
  const page=await fetch(base);assert.equal(page.status,200);assert.match(await page.text(),/Routework/);
  for(const asset of ['/styles.css','/app.js','/charts.js','/fonts/plex-sans-400.ttf']) assert.equal((await fetch(base+asset)).status,200);
  assert.deepEqual(await (await fetch(base+'/api/health')).json(),{ok:true,engine:'C++17'});
});
test('sample dataset and experiment files are available',async()=> {
  const sample=await (await fetch(base+'/api/sample?size=30')).json();assert.equal(sample.vehicles,5);assert.equal(sample.sample,true);assert.equal(sample.csv.trim().split('\n').length,32);
  const rows=await (await fetch(base+'/api/benchmarks')).text();assert.equal(rows.trim().split('\n').length,2061);
  for(let e=1;e<=8;e++) {const r=await fetch(base+`/experiments/E${e}.svg`);assert.equal(r.status,200);assert.match(await r.text(),/<svg/);}
});
test('solving uses the actual C++ engine and preserves all route data',async()=> {
  const response=await post({});assert.equal(response.status,200);const data=await response.json();
  const solution=data.solutions.find(s=>s.algo==='cw+2opt');verifyJSON(data,solution);
  assert.ok(Math.abs(solution.total-37.8236312177)<1e-7);assert.equal(solution.routes.length,2);assert.ok(solution.ops>0);
  verifyJSON(data,data.solutions.find(s=>s.algo==='baseline'));
});
test('comparison preserves exact proofs and unsupported statuses',async()=> {
  const response=await post({mode:'compare'});assert.equal(response.status,200);const data=await response.json();assert.equal(data.solutions.length,18);
  const exact=data.solutions.find(s=>s.algo==='exact');assert.equal(exact.optimal,true);verifyJSON(data,exact);
  for(const s of data.solutions) {if(s.verified) {verifyJSON(data,s);assert.ok(s.total+1e-7>=exact.total);}else {assert.equal(s.total,null);assert.deepEqual(s.routes,[]);}}
  assert.equal(data.solutions.find(s=>s.algo==='bt').status,'unsupported');
});
test('one full-capacity stop, zero distances and JSON escaping',async()=> {
  const tiny='type,id,name,lat,lon,demand,tw_open,tw_close,service\ndepot,5,Depot,20,73,0,0,1440,0\ncustomer,99,"A, ""quoted"" \\ name",20,73,5,0,1440,3\n';
  const response=await post({csv:tiny,vehicles:1,capacity:5,algo:'exact'});assert.equal(response.status,200);const data=await response.json();const s=data.solutions[0];verifyJSON(data,s);assert.equal(s.total,0);assert.equal(s.optimal,true);assert.equal(data.instance.stops[1].id,99);assert.equal(data.instance.stops[1].name,'A, "quoted" \\ name');
});
test('C++ input validation is returned as a clear error',async()=> {
  const response=await post({capacity:1});assert.equal(response.status,400);assert.match((await response.json()).error,/demand|capacity/i);
});
test('adapter rejects malformed, excessive and unsafe requests',async()=> {
  for(const payload of [{vehicles:0},{capacity:1.5},{circuity:Infinity},{algo:'cw; echo unsafe'},{mode:'bench'},{csv:''},{csv:'x'.repeat(128001)}]) {
    const response=await post(payload);assert.equal(response.status,400);assert.ok((await response.json()).error);
  }
  assert.equal((await fetch(base+'/api/solve',{method:'POST',headers:{'Content-Type':'application/json'},body:'{broken'})).status,400);
  assert.equal((await fetch(base+'/api/solve',{method:'POST',body:'wrong type'})).status,415);
  assert.equal((await fetch(base+'/api/solve',{method:'POST',headers:{Origin:'https://unrelated.example','Content-Type':'application/json'},body:'{}'})).status,403);
  assert.equal((await fetch(base+'/src/main.cpp')).status,404);
  assert.equal((await fetch(base+'/api/sample?size=../../src/main.cpp')).status,400);
});
test('non-geographic infeasibility is represented without a fake route',async()=> {
  const pack='depot,0,Depot,20,73,0,0,1440,0\ncustomer,1,A,20,73,2,0,1440,0\ncustomer,2,B,20,73,2,0,1440,0\ncustomer,3,C,20,73,2,0,1440,0\n';
  const response=await post({csv:pack,capacity:3,vehicles:2,algo:'exact'});assert.equal(response.status,200);const data=await response.json();assert.equal(data.solutions[0].status,'infeasible');assert.equal(data.solutions[0].verified,false);assert.equal(data.solutions[0].total,null);
});
