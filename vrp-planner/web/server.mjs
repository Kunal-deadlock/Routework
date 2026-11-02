// Local-only HTTP adapter. C++ remains the sole routing/verification engine.
// No npm dependencies. Start from any directory: node web/server.mjs.
import http from 'node:http';
import { readFile, mkdtemp, writeFile, rm } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { dirname, join, extname, resolve } from 'node:path';
import { tmpdir } from 'node:os';
import { spawn } from 'node:child_process';
import { createRoadRouter } from './roads.mjs';
const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const assets = join(root, 'web', 'public');
const binary = join(root, process.platform === 'win32' ? 'vrp.exe' : 'vrp');
const algorithms = new Set(['baseline','nn','cw','sweep','bt','bb-fifo','bb-lifo','bb-lc','hk','exact', ...['baseline','nn','cw','sweep'].flatMap(a => [a+'+2opt',a+'+local'])]);
const files = new Map(['index.html','app.js','styles.css','charts.js','street-map.js','road-routes.js','vendor/leaflet/leaflet.js','vendor/leaflet/leaflet.css','vendor/leaflet/LICENSE',...['sans-400','sans-500','sans-600','mono-400','mono-500'].map(name=>'fonts/plex-'+name+'.ttf')].map(name => ['/'+name, join(assets,name)]));
const types = {'.html':'text/html; charset=utf-8','.js':'text/javascript; charset=utf-8','.css':'text/css; charset=utf-8','.ttf':'font/ttf','.csv':'text/csv; charset=utf-8','.svg':'image/svg+xml'};
let active = 0;
function fail(message, status=400) {return Object.assign(new Error(message), {status});}
function send(res, status, body, type='application/json; charset=utf-8') {
  res.writeHead(status, {'Content-Type':type,'Cache-Control':'no-store','X-Content-Type-Options':'nosniff','Referrer-Policy':'strict-origin-when-cross-origin','Content-Security-Policy':"default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' https://tile.openstreetmap.org data: blob:; font-src 'self'; connect-src 'self'; object-src 'none'; frame-ancestors 'none'"});
  res.end(typeof body === 'string' || Buffer.isBuffer(body) ? body : JSON.stringify(body));
}
async function body(req) {
  const chunks=[];let size=0;
  for await (const chunk of req) {size+=chunk.length;if(size>256000) throw fail('The upload is too large. Use a CSV with at most 200 customers.',413);chunks.push(chunk);}
  try {return JSON.parse(Buffer.concat(chunks).toString('utf8'));} catch {throw fail('The request must contain valid JSON.');}
}
function integer(value, name, max) {if(!Number.isInteger(value) || value<1 || value>max) throw fail(`${name} must be a whole number between 1 and ${max}.`);return value;}
export async function solve(payload) {
  if(!payload || typeof payload !== 'object') throw fail('Missing routing request.');
  const {csv,algo='cw+2opt',mode='solve',vehicles,capacity,circuity=1.3}=payload;
  if(typeof csv !== 'string' || !csv.trim()) throw fail('Add a depot and at least one delivery stop.');
  if(Buffer.byteLength(csv)>128000 || csv.split(/\r?\n/).filter(Boolean).length>202) throw fail('The web interface supports at most 200 customers. Use the CLI for larger inputs.');
  integer(vehicles,'Vehicles',200);integer(capacity,'Capacity',1000000);
  if(typeof circuity!=='number' || !Number.isFinite(circuity) || circuity<0.1 || circuity>10) throw fail('Distance multiplier must be between 0.1 and 10.');
  if(!algorithms.has(algo)) throw fail('Choose a supported routing method.');
  if(!['solve','compare'].includes(mode)) throw fail('Unknown routing action.');
  if(active>=2) throw fail('The planner is busy. Try again in a moment.',429);
  ++active;
  let dir;
  try {
    dir=await mkdtemp(join(tmpdir(),'vrp-web-'));
    const input=join(dir,'input.csv');await writeFile(input,csv);
    const args=[mode,'--input',input,'--vehicles',String(vehicles),'--capacity',String(capacity),'--circuity',String(circuity),'--format','json','--node-limit','50000','--time-limit-ms','500'];
    if(mode==='solve') args.push('--algo',algo);
    const result=await new Promise((resolveResult,reject) => {
      const child=spawn(binary,args,{cwd:root,windowsHide:true,shell:false});
      let stdout='',stderr='',settled=false;
      const timer=setTimeout(()=> {child.kill();reject(fail('This run took too long. Try a fast routing method or fewer stops.',408));},15000);
      const finish=(error,value)=> {if(settled) return;settled=true;clearTimeout(timer);if(error) reject(error);else resolveResult(value);};
      child.stdout.on('data',data=> {stdout+=data;if(stdout.length>8000000) {child.kill();finish(fail('The response is too large.',413));}});
      child.stderr.on('data',data=> {if(stderr.length<8000) stderr+=data;});
      child.on('error',()=>finish(fail('The C++ planner could not start. Build it with make before starting the web server.',503)));
      child.on('close',code=> {
        if(![0,2,3,4].includes(code)) {
          let message=stderr.trim().replace(/^Error:\s*/, '') || 'The planner could not complete this request.';
          if(message.includes('demand outside')) message='Each stop must have demand between zero and the capacity of one vehicle. Increase vehicle capacity or edit the stop demand.';
          if(message.includes('total demand exceeds fleet capacity')) message='Your fleet cannot carry all the demand. Add vehicles or increase the capacity per vehicle.';
          return finish(fail(message));
        }
        try {finish(null,JSON.parse(stdout));} catch {finish(fail('The planner returned an unreadable result.',500));}
      });
    });
    return result;
  } finally {--active;if(dir) await rm(dir,{recursive:true,force:true});}
}
export function createApp({roadRouter=createRoadRouter()}={}) {
  return http.createServer(async (req,res) => {
    try {
      const host=req.headers.host || '';
      if(!/^(127\.0\.0\.1|localhost)(:\d+)?$/.test(host)) throw fail('Use the local planner address.',403);
      if(req.headers.origin && ![`http://${host}`].includes(req.headers.origin)) throw fail('Cross-site requests are not allowed.',403);
      const url=new URL(req.url,`http://${host}`);
      if(req.method==='POST' && url.pathname==='/api/solve') {
        if(!/^application\/json\b/.test(req.headers['content-type']||'')) throw fail('Send routing data as JSON.',415);
        return send(res,200,await solve(await body(req)));
      }
      if(req.method==='POST' && url.pathname==='/api/road-route') {
        if(!/^application\/json\b/.test(req.headers['content-type']||'')) throw fail('Send road coordinates as JSON.',415);
        return send(res,200,await roadRouter(await body(req)));
      }
      if(req.method!=='GET') throw fail('This action is not supported.',405);
      if(url.pathname==='/api/health') return send(res,200,{ok:true,engine:'C++17'});
      if(url.pathname==='/api/sample') {
        const size=url.searchParams.get('size') || '15';if(!['15','30'].includes(size)) throw fail('Choose the 15-stop or 30-stop sample.');
        return send(res,200,{csv:await readFile(join(root,'data',`nashik_tiffin_${size}.csv`),'utf8'),vehicles:size==='15'?3:5,capacity:20,name:`Nashik · ${size} stops`,sample:true});
      }
      if(url.pathname==='/api/benchmarks') return send(res,200,await readFile(join(root,'results','results.csv')),'text/csv; charset=utf-8');
      const graph=/^\/experiments\/(E[1-8])\.svg$/.exec(url.pathname);
      if(graph) return send(res,200,await readFile(join(root,'results',graph[1]+'.svg')),'image/svg+xml');
      const file=files.get(url.pathname==='/'?'/index.html':url.pathname);
      if(!file) throw fail('Page not found.',404);
      return send(res,200,await readFile(file),types[extname(file)] || 'application/octet-stream');
    } catch(error) {if(!res.headersSent) send(res,error.status || (error.code==='ENOENT'?404:500),{error:error.status?error.message:error.code==='ENOENT'?'The requested file is not available.':'An unexpected error occurred. Please try again.'});else res.end();}
  });
}
if(process.argv[1] && resolve(process.argv[1])===fileURLToPath(import.meta.url)) {
  const port=Number(process.env.PORT || 3000);
  if(!Number.isInteger(port) || port<1 || port>65535) throw new Error('PORT must be between 1 and 65535.');
  const server=createApp();
  server.on('error',error=> {console.error(error.code==='EADDRINUSE'?`Port ${port} is in use. Set PORT to a different number.`:error.message);process.exitCode=1;});
  server.listen(port,'127.0.0.1',()=>console.log(`Routework ready: http://127.0.0.1:${port}\nC++ engine: ${binary}\nKeep this terminal open. Press Ctrl+C to stop.`));
}
