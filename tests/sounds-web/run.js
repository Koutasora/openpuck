// Browser test of docs/index.html: Sounds piano roll, MIDI drop, >32 guards, live playback over a fake WebHID,
// and the Debug test panel. Playwright's bundled Chromium, headless (never Edge). Run: npm test
const http=require("http"), fs=require("fs"), path=require("path");
const {chromium}=require("playwright");

const DOCS=path.resolve(__dirname,"../../docs");
const TYPES={".html":"text/html",".js":"text/javascript",".css":"text/css",".mid":"audio/midi",".json":"application/json",".png":"image/png",".svg":"image/svg+xml"};
const server=http.createServer((req,res)=>{
  const f=path.join(DOCS,decodeURIComponent(req.url.split("?")[0]).replace(/^\/+$/,"/index.html"));
  if(!f.startsWith(DOCS)||!fs.existsSync(f)||fs.statSync(f).isDirectory()){ res.writeHead(404); res.end(); return; }
  res.writeHead(200,{"content-type":TYPES[path.extname(f)]||"application/octet-stream"}); res.end(fs.readFileSync(f));
});

let fails=0;
function check(name,ok,extra){ if(!ok) fails++; console.log((ok?"PASS ":"FAIL ")+name+(ok||extra===undefined?"":"  -> "+JSON.stringify(extra))); }
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
const same=(a,b)=>JSON.stringify(a)===JSON.stringify(b);

// the page gets a fake WebHID: two vendor interfaces, the second one sends input reports; every sendReport is recorded
const FAKE_HID=()=>{
  window.__sent=[];
  class Dev extends EventTarget{
    constructor(i){ super(); this.idx=i; this.vendorId=0x28DE; this.productId=0x1304; this.opened=false;
      this.collections=[{usagePage:0xFF00,usage:1,inputReports:[],featureReports:[],
        outputReports:[0x83,0x80].map(id=>({reportId:id,items:[{reportSize:8,reportCount:63}]}))}]; }
    async open(){ this.opened=true; if(this.idx===1) this.iv=setInterval(()=>this.dispatchEvent(new Event("inputreport")),20); }
    async close(){ this.opened=false; clearInterval(this.iv); }
    async sendReport(id,data){ window.__sent.push({t:performance.now(),dev:this.idx,id,d:Array.from(data)}); }
  }
  const devs=[new Dev(0),new Dev(1)], hid=new EventTarget();
  hid.requestDevice=async()=>devs; hid.getDevices=async()=>devs;
  Object.defineProperty(navigator,"hid",{value:hid,configurable:true});
};

// format 0 SMF: 40 back-to-back 100 ms notes alternating C2 / E2 (65 Hz and 82 Hz, below the old 130 Hz floor)
function smf(us){
  const ev=[]; const vlq=v=>{ const b=[v&127]; while(v>>=7) b.unshift((v&127)|128); return b; };
  us=us||500000; ev.push(0,0xFF,0x51,3,(us>>16)&255,(us>>8)&255,us&255); // default 500000 us per quarter (120 BPM)
  for(let i=0;i<40;i++){ const p=i%2?40:36; ev.push(0,0x90,p,100, ...vlq(96),0x80,p,0); }
  ev.push(0,0xFF,0x2F,0);
  const tr=[...ev], len=tr.length;
  return [0x4D,0x54,0x68,0x64,0,0,0,6,0,0,0,1,0x01,0xE0, 0x4D,0x54,0x72,0x6B,(len>>24)&255,(len>>16)&255,(len>>8)&255,len&255,...tr];
}

// SMF builder: tracks = [{name, steps:[[{ch,p},...],...]}] ; every step is 96 ticks (100 ms at 120 BPM), all notes of a step start together
function smfTracks(format,tracks,us){
  const vlq=v=>{ const b=[v&127]; while(v>>=7) b.unshift((v&127)|128); return b; };
  us=us||500000;
  const trk=(t,i)=>{
    const ev=[];
    if(i===0) ev.push(0,0xFF,0x51,3,(us>>16)&255,(us>>8)&255,us&255);
    if(t.name){ ev.push(0,0xFF,0x03,t.name.length,...[...t.name].map(c=>c.charCodeAt(0))); }
    for(const st of t.steps){
      st.forEach(n=>ev.push(0,0x90|n.ch,n.p,100));
      st.forEach((n,k)=>ev.push(k?0:96,0x80|n.ch,n.p,0));
    }
    ev.push(0,0xFF,0x2F,0);
    const l=ev.length; return [0x4D,0x54,0x72,0x6B,(l>>24)&255,(l>>16)&255,(l>>8)&255,l&255,...ev];
  };
  return [0x4D,0x54,0x68,0x64,0,0,0,6,0,format,0,tracks.length,0x01,0xE0,...tracks.flatMap(trk)];
}

(async()=>{
  await new Promise(r=>server.listen(0,"127.0.0.1",r));
  const url="http://127.0.0.1:"+server.address().port+"/index.html";
  const browser=await chromium.launch({headless:true}); // bundled Chromium, not Edge
  const page=await browser.newPage({viewport:{width:1300,height:1500}});
  const errors=[]; let dialogMode="accept", lastDialog="";
  page.on("pageerror",e=>errors.push("pageerror: "+e.message));
  page.on("console",m=>{ if(m.type()==="error") errors.push("console.error: "+m.text()); });
  page.on("response",x=>{ if(x.status()>=400) errors.push("HTTP "+x.status()+" "+x.url()); });
  page.on("requestfailed",x=>errors.push("request failed: "+x.url()));
  page.on("dialog",d=>{ lastDialog=d.message(); return dialogMode==="dismiss"?d.dismiss():d.accept(); });
  await page.addInitScript(FAKE_HID);
  await page.goto(url); await sleep(500);
  const ev=(f,a)=>page.evaluate(f,a);

  // open the Sounds card without a puck: the page only hides it until a puck reports status protocol 25
  await ev(()=>{
    $("#panel").classList.remove("hide"); $("#sndCard").classList.remove("hide");
    sounds=sndEmpty(); soundsCap=true; soundsVer=2; soundsOnBlob([27]); tabShow("mapping/sounds"); $("#prSnap").value="50"; sndRender();
    pr.cv.scrollIntoView({block:"center"}); pr.fitNow=true; prDraw();
  });
  const mel=()=>ev(()=>sounds.mel[curMel][curLane].map(n=>({freq:n.freq,ms:n.ms})));
  const pos=(ms,row)=>ev(([ms,row])=>{ const r=pr.cv.getBoundingClientRect(); return {x:r.left+prXof(ms),y:r.top+(row==null?10:prYof(row)+pr.rowH/2)}; },[ms,row]);
  const click=async(ms,row)=>{ const p=await pos(ms,row); await page.mouse.move(p.x,p.y); await page.mouse.down(); await page.mouse.up(); };
  const drag=async(a,b)=>{ const p=await pos(...a), q=await pos(...b); await page.mouse.move(p.x,p.y); await page.mouse.down();
    await page.mouse.move((p.x+q.x)/2,(p.y+q.y)/2); await page.mouse.move(q.x,q.y); await page.mouse.up(); };
  const noOverlap=()=>ev(()=>{ let t=0; for(const n of prNotes()){ if(n.s<t) return false; t=n.e; } return sounds.mel[curMel][curLane].every(n=>n.ms>=SND_MS_MIN&&n.ms<=SND_MS_MAX); });
  const rows=()=>ev(()=>document.querySelectorAll("#sndNotes tr[id^=snote]").length);

  check("Player tab is gone",await ev(()=>!TAB_DEFS.some(t=>t[2].some(s=>s[0]==="player"))&&!document.getElementById("sub-mapping-player")&&!document.getElementById("playerCard")));

  // ---- connection ----
  await page.click("#plConnect");
  await page.waitForFunction(()=>/Connected/.test($("#plStatus").textContent),null,{timeout:3000}).catch(()=>{});
  const st=await ev(()=>({s:$("#plStatus").textContent,opts:$("#plIface").options.length,sel:$("#plIface").value}));
  check("connect picks the interface that sends input",/Connected: 28DE:1304, interface 2, output length 0x83: 63 B, 0x80: 63 B/.test(st.s)&&st.opts===2&&st.sel==="1",st);

  // ---- piano roll edits (snap 50) ----
  await click(110,69);
  check("a plain click adds a note of the default length 200 ms (start floored to the grid)",same(await mel(),[{freq:0,ms:100},{freq:440,ms:200}]),await mel());
  await ev(()=>{ sounds.mel[curMel][curLane]=[]; pr.selS=-1; pr.lastLen=50; sndRender(); });
  await click(110,69);
  check("a plain click uses the length of the last note used",same(await mel(),[{freq:0,ms:100},{freq:440,ms:50}]),await mel());
  await ev(()=>{ $("#prSnap").value="100"; pr.lastLen=100; });
  await click(410,72);
  check("snap 100 ms: note at 400, 100 ms long",same(await mel(),[{freq:0,ms:100},{freq:440,ms:50},{freq:0,ms:250},{freq:523,ms:100}]),await mel());
  await ev(()=>{ $("#prSnap").value="50"; });
  check("table follows the roll",(await rows())===4);
  await drag([125,69],[225,71]);
  check("drag moves in time and pitch",same(await mel(),[{freq:0,ms:200},{freq:494,ms:50},{freq:0,ms:150},{freq:523,ms:100}]),await mel());
  await drag([495,72],[595,72]);
  check("drag the right edge resizes",same(await mel(),[{freq:0,ms:200},{freq:494,ms:50},{freq:0,ms:150},{freq:523,ms:200}]),await mel());
  check("no overlaps so far",await noOverlap());
  await drag([225,71],[475,71]);
  check("moving over a note trims the other one",same(await mel(),[{freq:0,ms:400},{freq:523,ms:50},{freq:494,ms:50}]),await mel());
  check("no overlaps after the trim",await noOverlap());
  await page.keyboard.press("Delete");
  check("Delete of the last note leaves no trailing rest",same(await mel(),[{freq:0,ms:400},{freq:523,ms:50}]),await mel());
  await ev(()=>{ $("#prSnap").value="100"; pr.lastLen=100; });
  await click(834,69);
  check("snap 100 on a later click",same(await mel(),[{freq:0,ms:400},{freq:523,ms:50},{freq:0,ms:350},{freq:440,ms:100}]),await mel());
  check("table follows the roll after edits",(await rows())===4);
  // a trailing rest the melody already had survives an edit elsewhere; deleting the only note empties the melody
  await ev(()=>{ sounds.mel[curMel][curLane]=[{freq:440,ms:100},{freq:0,ms:200}]; pr.selS=-1; sndRender(); });
  await ev(()=>{ pr.lastLen=100; });
  await click(410,69);
  check("an existing trailing rest keeps its length after an edit",same(await mel(),[{freq:440,ms:100},{freq:0,ms:300},{freq:440,ms:100},{freq:0,ms:200}]),await mel());
  await ev(()=>{ sounds.mel[curMel][curLane]=[{freq:440,ms:200}]; pr.selS=-1; sndRender(); });
  await click(100,69); await page.keyboard.press("Delete");
  check("deleting the only note gives an empty melody",same(await mel(),[]),await mel());
  await ev(()=>{ $("#prSnap").value="50"; });

  // ---- drawing, deleting, cursors, fold, toolbar ----
  await drag([110,69],[410,69]);
  check("press and drag on empty space draws a note of the dragged length",same(await mel(),[{freq:0,ms:100},{freq:440,ms:300}]),await mel());
  await click(710,72);
  check("a click after that uses the drawn length (300 ms)",same(await mel(),[{freq:0,ms:100},{freq:440,ms:300},{freq:0,ms:300},{freq:523,ms:300}]),await mel());
  const cur=async(ms,row)=>{ const q=await pos(ms,row); await page.mouse.move(q.x,q.y); return ev(()=>pr.cv.style.cursor); };
  const cEmpty=await cur(1000,60), cBody=await cur(850,72), cGrip=await cur(995,72);
  check("cursors: crosshair on empty space, move on a note, ew-resize on the right grip",cEmpty==="crosshair"&&cBody==="move"&&cGrip==="ew-resize",[cEmpty,cBody,cGrip]);
  const rp=await pos(850,72); await page.mouse.click(rp.x,rp.y,{button:"right"});
  check("right-click on a note deletes it",same(await mel(),[{freq:0,ms:100},{freq:440,ms:300}]),await mel());
  await sleep(700);
  const dp=await pos(250,69); await page.mouse.dblclick(dp.x,dp.y);
  check("double-click on a note deletes it",same(await mel(),[]),await mel());
  const grip=await ev(()=>{ pr.range=null; return null; });
  check("toolbar above the roll holds Play on controller, Stop, Preview and the roll controls; the lower row does not",
    await ev(()=>{ const row=$("#sndLive").closest(".row"); return ["prSnap","sndLiveStop","sndPrev","prFold","prCrop"].every(id=>row.contains($("#"+id)))&&!$("#sndSave").closest(".row").contains($("#sndLive"))&&row.nextElementSibling===$("#prCanvas"); }));
  // ---- tempo ----
  check("default BPM is 120 and the default snap is 1/16",await ev(()=>$("#prBpm").value==="120"&&bpmOf(curMel)===120)&&await ev(()=>$("#prSnap").querySelector("option[selected]").value==="n16"));
  await ev(()=>{ sounds.mel[curMel][curLane]=[{freq:440,ms:200},{freq:0,ms:100},{freq:523,ms:200}]; sndRender(); });
  await page.fill("#prBpm","240"); await page.press("#prBpm","Tab");
  const half=await ev(()=>({m:sounds.mel[curMel][curLane].map(n=>n.ms),b:bpmOf(curMel),t:[...document.querySelectorAll("#sndNotes tr[id^=snote] td:nth-child(4) input")].map(i=>+i.value),ls:JSON.parse(localStorage.getItem("openpuck.melBpm"))[curMel]}));
  check("120 -> 240 BPM halves every entry, the table follows, the BPM is kept in localStorage",same(half.m,[100,50,100])&&same(half.t,[100,50,100])&&half.b===240&&half.ls===240,half);
  await ev(()=>{ sounds.mel[curMel][curLane]=[{freq:440,ms:200},{freq:0,ms:40}]; sndRender(); });
  await page.fill("#prBpm","480"); await page.press("#prBpm","Tab");
  const ref=await ev(()=>({m:sounds.mel[curMel][curLane].map(n=>n.ms),b:bpmOf(curMel),v:$("#prBpm").value,msg:$("#sndMsg").textContent}));
  check("BPM change is refused when an entry would drop under 30 ms (melody and BPM kept)",same(ref.m,[200,40])&&ref.b===240&&ref.v==="240"&&/BPM not changed/.test(ref.msg),ref);
  await ev(()=>{ bpmSet(curMel,120); sounds.mel[curMel][curLane]=[]; pr.selS=-1; pr.lastLen=125; $("#prSnap").value="n16"; sndRender(); });
  check("1/16 beat at 120 BPM is a 125 ms step",await ev(()=>prGrid()===125&&prBeatSnap()));
  await click(270,69);
  check("beat snap places a note on the 125 ms grid",same(await mel(),[{freq:0,ms:250},{freq:440,ms:125}]),await mel());
  await ev(()=>{ $("#prSnap").value="50"; bpmSet(curMel,120); $("#prBpm").value=120; });
  // fold: only the rows in use, a vertical drag moves between them
  await ev(()=>{ sounds.mel[curMel][curLane]=[{freq:262,ms:100},{freq:330,ms:100},{freq:523,ms:100}]; pr.selS=-1; pr.fitNow=true; sndRender(); });
  await page.check("#prFold");
  const fold=await ev(()=>({fl:pr.fl,h:prYof(64)-prYof(72)===pr.rowH,top:prYof(72)===PR_RH,ss:sessionStorage.getItem(PR_FOLD)}));
  check("Fold shows only the rows in use, high to low, and remembers the choice",same(fold.fl,[72,64,60])&&fold.h&&fold.top&&fold.ss==="1",fold);
  await drag([50,60],[50,64]);
  check("a vertical drag in fold moves the note to the next used row",same(await mel(),[{freq:330,ms:100},{freq:330,ms:100},{freq:523,ms:100}])&&same(await ev(()=>pr.fl),[72,64]),await mel());
  await page.uncheck("#prFold");
  check("Fold off: the continuous range is back",await ev(()=>pr.fl===null&&sessionStorage.getItem(PR_FOLD)==="0"));

  // ---- sndBuild is unchanged for a melody within 32 entries ----
  const build=await ev(()=>{ melBpm[0]=200; soundsVer=1; const s=sndEmpty(); s.mel[0][0]=[{freq:440,ms:200},{freq:0,ms:100}]; return sndBuild(s); });
  const exp=[1,250,0,1,2,3,4,5,6,7];
  for(let m=0;m<8;m++){ const e=m===0?[[440,200],[0,100]]:[]; exp.push(e.length); for(let k=0;k<32;k++){ const [f,ms]=e[k]||[0,0]; exp.push(f&255,f>>8,ms&255,ms>>8); } }
  const sum=exp.reduce((a,b)=>(a+b)&0xFFFF,0);
  check("sndBuild bytes for a short melody (v1 blob, BPM is not in it)",same(build,[0xB8,1,exp.length&255,exp.length>>8,...exp,sum&255,sum>>8]),build.length);
  // v2: four voices per melody, hand-computed layout
  const def={"0,0":[[440,200]],"0,1":[[0,100],[523,50]],"3,3":[[262,30]]};
  const pl2=[1,250,0,1,2,3,4,5,6,7];
  for(let m=0;m<8;m++) for(let v=0;v<4;v++){ const e=def[m+","+v]||[]; pl2.push(e.length); for(let k=0;k<32;k++){ const [f,ms]=e[k]||[0,0]; pl2.push(f&255,f>>8,ms&255,ms>>8); } }
  const sum2=pl2.reduce((a,b)=>(a+b)&0xFFFF,0), blob2=[0xB8,2,pl2.length&255,pl2.length>>8,...pl2,sum2&255,sum2>>8];
  const mk=()=>{ const s=sndEmpty(); s.confirm=1; s.gain=-6; s.mel[0][0]=[{freq:440,ms:200}]; s.mel[0][1]=[{freq:0,ms:100},{freq:523,ms:50}]; s.mel[3][3]=[{freq:262,ms:30}]; return s; };
  const b2=await ev(()=>{ const s=sndEmpty(); s.confirm=1; s.gain=-6; s.mel[0][0]=[{freq:440,ms:200}]; s.mel[0][1]=[{freq:0,ms:100},{freq:523,ms:50}]; s.mel[3][3]=[{freq:262,ms:30}]; return sndBuild(s,2); });
  check("v2 blob: 4144 bytes, payload 4138, voices in order",b2.length===4144&&pl2.length===4138&&same(b2,blob2),b2.length);
  const rt=await ev(blob=>{ const s=sndParse(new Uint8Array(blob)); return {ver:s.ver,mel:s.mel,confirm:s.confirm,gain:s.gain}; },blob2);
  check("v2 blob parses back to the same voices",rt.ver===2&&rt.confirm===1&&rt.gain===-6&&same(rt.mel[0][0],[{freq:440,ms:200}])&&same(rt.mel[0][1],[{freq:0,ms:100},{freq:523,ms:50}])&&same(rt.mel[3][3],[{freq:262,ms:30}])&&rt.mel[0][2].length===0&&rt.mel[5][0].length===0,rt.ver);
  const v1blob=[0xB8,1,exp.length&255,exp.length>>8,...exp,sum&255,sum>>8];
  const v1=await ev(blob=>{ const s=sndParse(new Uint8Array(blob)); return {ver:s.ver,m:s.mel[0]}; },v1blob);
  check("v1 blob parses into voices 0 and 1 (copy), voices 2 and 3 empty",v1.ver===1&&same(v1.m[0],[{freq:440,ms:200},{freq:0,ms:100}])&&same(v1.m[1],v1.m[0])&&v1.m[2].length===0&&v1.m[3].length===0,v1);
  // protocol 25/26 save path: only the Left trackpad voice goes out, as a v1 blob, with a warning
  const sv=await ev(async()=>{
    soundsOnBlob([26]);
    const warn=!$("#sndV1Warn").classList.contains("hide")&&/one voice \(Left trackpad\)/.test($("#sndV1Warn").textContent);
    sounds.mel[0][0]=[{freq:440,ms:200}]; sounds.mel[0][1]=[{freq:523,ms:100}]; sounds.confirm=1; sounds.gain=-6; sounds.ev=[0,1,2,3,4,5,6,7];
    let cap=null; const o={x:bindsXfer,w:soundsWrite,r:soundsRead};
    bindsXfer=async fn=>fn(); soundsWrite=async b=>{ cap=b; return true; }; soundsRead=async()=>{};
    dev={}; await soundsSave(false); dev=null;
    bindsXfer=o.x; soundsWrite=o.w; soundsRead=o.r;
    return {warn,cap,ver:soundsVer};
  });
  const pl1=[1,250,0,1,2,3,4,5,6,7]; for(let m=0;m<8;m++){ const e=m===0?[[440,200]]:[]; pl1.push(e.length); for(let k=0;k<32;k++){ const [f,ms]=e[k]||[0,0]; pl1.push(f&255,f>>8,ms&255,ms>>8); } }
  const s1=pl1.reduce((a,b)=>(a+b)&0xFFFF,0);
  check("protocol 26: the saved blob is v1 with the Left trackpad voice only, and the card warns",sv.warn&&sv.ver===1&&sv.cap&&same(sv.cap,[0xB8,1,pl1.length&255,pl1.length>>8,...pl1,s1&255,s1>>8]),sv.cap&&sv.cap.length);
  await ev(()=>{ soundsOnBlob([27]); });
  check("protocol 27: no warning, v2",await ev(()=>$("#sndV1Warn").classList.contains("hide")&&soundsVer===2));
  await ev(()=>{ sounds.mel[0][1]=[]; sounds.mel[0][0]=[]; sndRender(); });

  // ---- MIDI drop ----
  await ev(()=>{ sounds.mel[curMel][curLane].length=0; sndRender(); });
  await ev(bytes=>{
    const dt=new DataTransfer(); dt.items.add(new File([new Uint8Array(bytes)],"t.mid",{type:"audio/midi"}));
    pr.cv.dispatchEvent(new DragEvent("drop",{dataTransfer:dt,bubbles:true,cancelable:true}));
  },smf());
  await page.waitForSelector("#midiRow:not(.hide)",{timeout:3000});
  await page.click("#midiGo");
  check("MIDI import sets the BPM from the file (120)",await ev(()=>bpmOf(curMel)===120&&$("#prBpm").value==="120"));
  const imp=await ev(()=>({n:sounds.mel[curMel][curLane].length,f:sounds.mel[curMel][curLane].slice(0,3).map(n=>n.freq),ms:sounds.mel[curMel][curLane][0].ms,note:$("#midiNote").textContent}));
  check("MIDI drop imports all entries, no cut to 32, no octave shift",imp.n===40&&same(imp.f,[65,82,65])&&imp.ms===100,imp);
  check("import message names the entries and the save limit",/Imported: LT 40 entries/.test(imp.note)&&/32 or fewer/.test(imp.note),imp.note);
  const over=await ev(()=>({cnt:$("#sndCount").textContent,col:$("#sndCount").style.color,save:$("#sndSave").disabled,evs:$("#scSndSave").disabled,add:$("#sndAddNote").disabled}));
  check("counter warns and Save is disabled above 32",/40 \/ 32/.test(over.cnt)&&/Crop to 32/.test(over.cnt)&&/warn/.test(over.col)&&over.save&&over.evs&&over.add,over);
  const guard=await ev(async()=>{ window.__b=0; const o=sndBuild; sndBuild=function(...a){ window.__b++; return o(...a); };
    dev={}; await soundsSave(false); await soundsSave(true); dev=null; sndBuild=o; return {b:window.__b,msg:$("#sndMsg").textContent}; });
  check("saving a melody over 32 is refused before sndBuild",guard.b===0&&/not saved/.test(guard.msg),guard);

  // every sample file through the drop path: all notes keep their MIDI pitch (no octave shift, no cut)
  const fs2=require("fs"), dir=path.join(DOCS,"midi");
  for(const f of fs2.readdirSync(dir).filter(x=>/\.mid$/.test(x))){
    await ev(()=>{ sounds.mel[curMel][curLane].length=0; sndRender(); });
    await ev(bytes=>{ const dt=new DataTransfer(); dt.items.add(new File([new Uint8Array(bytes)],"x.mid"));
      pr.cv.dispatchEvent(new DragEvent("drop",{dataTransfer:dt,bubbles:true,cancelable:true})); },Array.from(fs2.readFileSync(path.join(dir,f))));
    await page.waitForSelector("#midiRow:not(.hide)",{timeout:3000});
    const pitches=await ev(()=>[...new Set(midiData.srcs[+$("#midiLane0").value.slice(1)].notes.map(n=>n.p))]);
    check("sample "+f+": one source goes to LT, the other voices keep their notes",await ev(()=>$("#midiLane0").value[0]==="s"&&[1,2,3].every(v=>$("#midiLane"+v).value==="keep")));
    const bpmExp=await ev(()=>{ const t=midiData.m.tempos; const t0=t.length?t.reduce((a,b)=>b.t<a.t?b:a):null; return t0?Math.round(60000000/t0.us):120; });
    await page.click("#midiGo");
    const got=await ev(()=>({n:sounds.mel[curMel][curLane].length,f:[...new Set(sounds.mel[curMel][curLane].filter(n=>n.freq).map(n=>n.freq))]}));
    const hz=p=>Math.round(440*Math.pow(2,(p-69)/12)), allowed=new Set(pitches.map(hz));
    check("sample "+f+" sets BPM "+bpmExp,await ev(e=>bpmOf(curMel)===e&&$("#prBpm").value===String(e),bpmExp));
    check("sample "+f+": "+got.n+" entries, every frequency is a MIDI pitch of the track",got.n>0&&got.f.length>0&&got.f.every(x=>allowed.has(x)),{got,pitches});
  }
  // a file with another tempo: 250000 us per quarter = 240 BPM
  await ev(()=>{ sounds.mel[curMel][curLane].length=0; sndRender(); });
  await ev(bytes=>{ const dt=new DataTransfer(); dt.items.add(new File([new Uint8Array(bytes)],"t.mid"));
    pr.cv.dispatchEvent(new DragEvent("drop",{dataTransfer:dt,bubbles:true,cancelable:true})); },smf(250000));
  await page.waitForSelector("#midiRow:not(.hide)",{timeout:3000}); await page.click("#midiGo");
  check("MIDI import sets 240 BPM from a 250000 us tempo",await ev(()=>bpmOf(curMel)===240&&$("#prBpm").value==="240"&&JSON.parse(localStorage.getItem("openpuck.melBpm"))[curMel]===240));
  await ev(()=>{ bpmSet(curMel,120); sndRender(); });
  // ---- import into all four voices ----
  const dropBytes=async bytes=>{
    await ev(b=>{ const dt=new DataTransfer(); dt.items.add(new File([new Uint8Array(b)],"m.mid",{type:"audio/midi"}));
      pr.cv.dispatchEvent(new DragEvent("drop",{dataTransfer:dt,bubbles:true,cancelable:true})); },bytes);
    await page.waitForSelector("#midiRow:not(.hide)",{timeout:3000});
  };
  const step=(...ns)=>ns.map(([ch,p])=>({ch,p}));
  // format 0, one track, channels 1 (high), 2, 3 and drums (channel 10)
  const f0=smfTracks(0,[{name:"All",steps:[72,74,72,74].map((hi,i)=>step([0,hi],[1,60+i],[2,48+i],[9,36]))}]);
  await ev(()=>{ for(const l of sounds.mel[curMel]) l.length=0; sndRender(); });
  await dropBytes(f0);
  const o0=await ev(()=>({sel:[0,1,2,3].map(v=>$("#midiLane"+v).value),opts:[...$("#midiLane0").options].map(o=>o.textContent),cnt:[0,1,2,3].map(v=>$("#midiCnt"+v).textContent),n:midiData.srcs.length,drumIdx:midiData.srcs.findIndex(x=>x.drums)}));
  check("format 0 with 3 channels + drums: whole track plus one source per channel, drums marked",o0.n===5&&o0.opts.slice(0,2).join()==="Keep current,Empty"&&/Track 1, ch 1/.test(o0.opts[3])&&/Track 1, ch 10.*drums/.test(o0.opts[6]),o0.opts);
  check("default: channels by average pitch to LT, RT, LG; RG empty; drums never picked",o0.sel.join()==="s1,s2,s3,empty"&&!o0.sel.includes("s"+o0.drumIdx),o0);
  check("each voice shows how many entries it would get",o0.cnt.join("|")==="4 entries|4 entries|4 entries|0 entries",o0.cnt);
  await ev(()=>{ sounds.mel[curMel][3]=[{freq:440,ms:100}]; sounds.mel[curMel][2]=[{freq:330,ms:100}]; sndRender(); });
  await page.selectOption("#midiLane2","keep");
  lastDialog=""; await page.click("#midiGo");
  const i0=await ev(()=>({m:sounds.mel[curMel].map(l=>l.map(n=>n.freq)),note:$("#midiNote").textContent}));
  check("Import fills LT, RT with the chosen channels, Keep current leaves LG, Empty clears RG",same(i0.m,[[523,587,523,587],[262,277,294,311],[330],[]]),i0.m);
  check("one confirm names the voices that get replaced; the message lists the entries per voice",/Right grip/.test(lastDialog)&&!/Left grip/.test(lastDialog)&&/Imported: LT 4, RT 4, RG 0 entries/.test(i0.note),[lastDialog,i0.note]);
  // format 1, two tracks: whole tracks, higher one first
  const f1=smfTracks(1,[{name:"Low",steps:[48,50,52].map(p=>step([0,p]))},{name:"High",steps:[72,74,76,77,79].map(p=>step([1,p]))}]);
  await dropBytes(f1);
  const o1=await ev(()=>({sel:[0,1,2,3].map(v=>$("#midiLane"+v).value),opts:[...$("#midiLane0").options].map(o=>o.textContent),cnt:[0,1,2,3].map(v=>$("#midiCnt"+v).textContent)}));
  check("format 1 with 2 tracks: whole-track sources, the higher track first, the rest empty",o1.opts.length===4&&/Track 1 - Low/.test(o1.opts[2])&&o1.sel.join()==="s1,s0,empty,empty"&&o1.cnt[0]==="5 entries"&&o1.cnt[1]==="3 entries",o1);
  await page.click("#midiGo");
  const i1=await ev(()=>sounds.mel[curMel].map(l=>l.length));
  check("format 1 import: 5 entries in LT, 3 in RT, LG and RG empty",same(i1,[5,3,0,0]),i1);
  // more than 32 entries: warn text and colour
  await dropBytes(smf());
  const o2=await ev(()=>({t:$("#midiCnt0").textContent,c:$("#midiCnt0").style.color}));
  check("a source over 32 entries is flagged in the warn colour with the Crop hint",/^40 entries \(over 32, use Crop to 32\)$/.test(o2.t)&&/warn/.test(o2.c),o2);
  await ev(()=>{ midiData=null; $("#midiRow").classList.add("hide"); });
  // replace confirmation: Cancel keeps the old melody
  await ev(()=>{ sounds.mel[curMel][curLane]=[{freq:440,ms:200}]; sndRender(); });
  await ev(bytes=>{ const dt=new DataTransfer(); dt.items.add(new File([new Uint8Array(bytes)],"t.mid"));
    pr.cv.dispatchEvent(new DragEvent("drop",{dataTransfer:dt,bubbles:true,cancelable:true})); },smf());
  await page.waitForSelector("#midiRow:not(.hide)",{timeout:3000});
  dialogMode="dismiss"; await page.click("#midiGo"); dialogMode="accept";
  check("Cancel in the replace dialog keeps the old melody",same(await mel(),[{freq:440,ms:200}]),await mel());
  // positive control: exactly 32 entries reach sndBuild once
  const ctl=await ev(async()=>{
    sounds.mel[curMel][curLane]=Array.from({length:32},(_,i)=>({freq:i%2?0:440,ms:100})); sndRender();
    const o={b:sndBuild,x:bindsXfer,w:soundsWrite,r:soundsRead}; let calls=0;
    sndBuild=function(...a){ calls++; return o.b(...a); }; bindsXfer=async fn=>fn(); soundsWrite=async()=>true; soundsRead=async()=>{};
    dev={}; await soundsSave(false); dev=null;
    sndBuild=o.b; bindsXfer=o.x; soundsWrite=o.w; soundsRead=o.r;
    return {calls,save:$("#sndSave").disabled,msg:$("#sndMsg").textContent};
  });
  check("exactly 32 entries: Save enabled and sndBuild called once",ctl.calls===1&&!ctl.save&&!/not saved/.test(ctl.msg),ctl);
  await ev(()=>{ sounds.mel[curMel][curLane]=Array.from({length:40},(_,i)=>({freq:i%2?82:65,ms:100})); sndRender(); });

  // ---- crop (Crop to 32) ----
  const set40=()=>ev(()=>{ sounds.mel[curMel][curLane]=Array.from({length:40},(_,i)=>({freq:i%2?82:65,ms:100})); pr.range=null; pr.selS=-1; sndRender(); });
  await set40();
  check("Crop to 32 is enabled without a selection above 32 entries",await ev(()=>!$("#prCrop").disabled));
  await page.click("#prCrop");
  const c1=await mel(), m1=await ev(()=>$("#sndMsg").textContent);
  check("Crop to 32 without a selection keeps exactly the first 32 entries",c1.length===32&&c1.every((n,i)=>n.ms===100&&n.freq===(i%2?82:65))&&/Kept 32 entries \(3\.20 s\)/.test(m1),[c1.length,m1]);
  await set40();
  await ev(()=>{ pr.range={a:500,b:3900}; prRefresh(); });
  await page.click("#prCrop");
  const c2=await mel();
  check("Crop to 32 with a selection holding more than 32 entries keeps 32 from the selection start",c2.length===32&&c2.every((n,i)=>n.ms===100&&n.freq===(i%2?65:82)),[c2.length,c2[0]]);
  await set40();
  await drag([1000,null],[3000,null]);
  const rg=await ev(()=>pr.range);
  check("ruler drag selects a range",rg&&rg.a===1000&&rg.b===3000,rg);
  await page.click("#prCrop");
  const crop=await mel();
  check("Crop to selection keeps the entries inside the range",crop.length===20&&crop.every((n,i)=>n.ms===100&&n.freq===(i%2?82:65)),crop.length);
  check("cropped melody is within the limit again",await ev(()=>!$("#sndSave").disabled&&$("#prCrop").disabled));

  // ---- live playback ----
  await ev(()=>{ sounds.mel[curMel][curLane]=[{freq:440,ms:200},{freq:0,ms:100},{freq:523,ms:200}]; sounds.gain=-12; pr.range=null; sndRender(); window.__sent.length=0; });
  check("Play on controller is enabled with a connection",await ev(()=>!$("#sndLive").disabled));
  await page.click("#sndLive"); await sleep(900);
  let sent=await ev(()=>window.__sent.filter(r=>r.id===0x83));
  const t0=sent[0]&&sent[0].t, tone=(r,side,f,ms)=>r.dev===1&&r.d.length===63&&r.d[0]===side&&r.d[1]===0xF4&&(r.d[2]|r.d[3]<<8)===f&&(r.d[4]|r.d[5]<<8)===ms&&r.d.slice(6).every(x=>x===0);
  const off=(r,side)=>r.d[0]===side&&r.d[1]===0x80&&r.d[2]===0&&r.d[3]===0&&r.d[4]===0&&r.d[5]===0x80;
  const at=(r,ms)=>Math.abs(r.t-t0-ms)<=30;
  check("live: one voice = one side (side 0 only), 4 reports",sent.length===4&&sent.every(r=>r.d[0]===0),sent.length);
  check("live: note 440 Hz, -12 dB, 200 ms on side 0 at t=0",sent.length===4&&tone(sent[0],0,440,200)&&at(sent[0],0),sent[0]);
  check("live: rest sends the off tone at 200 ms",sent.length===4&&off(sent[1],0)&&at(sent[1],200),sent[1]);
  check("live: note 523 Hz at 300 ms",sent.length===4&&tone(sent[2],0,523,200)&&at(sent[2],300),sent[2]);
  check("live: off tone at the end (500 ms)",sent.length===4&&off(sent[3],0)&&at(sent[3],500),sent[3]);
  check("live: Play enabled again after the end",await ev(()=>!$("#sndLive").disabled&&$("#sndLiveStop").disabled));

  await ev(()=>{ pr.range={a:200,b:500}; window.__sent.length=0; });
  await page.click("#sndLive"); await sleep(700);
  sent=await ev(()=>window.__sent.filter(r=>r.id===0x83));
  const t1=sent[0]&&sent[0].t;
  check("live: a selected range plays only the range (rest, 523 Hz at 100 ms, end at 300 ms)",
    sent.length===3&&off(sent[0],0)&&tone(sent[1],0,523,200)&&Math.abs(sent[1].t-t1-100)<=30&&off(sent[2],0)&&Math.abs(sent[2].t-t1-300)<=30,sent.map(r=>r.d.slice(0,6)));

  await ev(()=>{ sounds.mel[curMel][curLane]=[{freq:440,ms:3000}]; sndRender(); pr.range=null; window.__sent.length=0; }); // long note: a slow click cannot reach the next event
  await page.click("#sndLive"); await sleep(100); await page.click("#sndLiveStop"); await sleep(50);
  const nAfter=await ev(()=>window.__sent.length); await sleep(500);
  sent=await ev(()=>window.__sent.filter(r=>r.id===0x83));
  check("live: Stop silences all four sides (0,1,3,4) and nothing follows",sent.length===5&&same(sent.slice(1).map(r=>r.d[0]),[0,1,3,4])&&sent.slice(1).every(r=>off(r,r.d[0]))&&(await ev(()=>window.__sent.length))===nAfter,sent.map(r=>r.d.slice(0,6)));

  // keyboard strip: a click plays the note for 300 ms on sides 0 and 1; not while playback runs
  await ev(()=>{ sounds.mel[curMel][curLane]=[{freq:330,ms:600}]; pr.selS=-1; pr.range=null; pr.fitNow=true; sndRender(); window.__sent.length=0; });
  const key=async row=>{ const q=await ev(row=>{ const r=pr.cv.getBoundingClientRect(); return {x:r.left+10,y:r.top+prYof(row)+pr.rowH/2}; },row); await page.mouse.click(q.x,q.y); };
  await key(64); await sleep(150);
  sent=await ev(()=>window.__sent.filter(r=>r.id===0x83));
  check("keyboard strip click sends a 300 ms tone on the active voice's side (note, gain from the Volume setting)",sent.length===1&&tone(sent[0],0,330,300),sent.map(r=>r.d.slice(0,6)));
  await ev(()=>{ window.__sent.length=0; });
  await page.click("#sndLive"); await sleep(100); await key(66); await sleep(150);
  sent=await ev(()=>window.__sent.filter(r=>r.id===0x83));
  check("keyboard strip click sends nothing while live playback runs",sent.length>0&&sent.every(r=>(r.d[2]|r.d[3]<<8)===330||r.d[1]===0x80),sent.map(r=>r.d.slice(0,6)));
  await page.click("#sndLiveStop"); await sleep(50);
  // ---- v1 puck: live playback mirrors the puck (LT on both trackpads), the other voices are not played ----
  await ev(()=>{ soundsOnBlob([26]); const m=sounds.mel[curMel]; for(const l of m) l.length=0; m[0].push({freq:440,ms:200}); m[1].push({freq:330,ms:200}); m[3].push({freq:262,ms:200}); sounds.gain=-12; pr.range=null; sndRender(); window.__sent.length=0; });
  check("v1 puck: RT / LG / RG buttons are dimmed and say they are not saved",await ev(()=>[1,2,3].every(v=>$("#sndLanes").children[v].style.opacity==="0.55"&&/not saved/.test($("#sndLanes").children[v].title))&&$("#sndLanes").children[0].style.opacity===""));
  await page.click("#sndLive"); await sleep(500);
  sent=await ev(()=>window.__sent.filter(r=>r.id===0x83));
  check("v1 puck: Play on controller plays LT on sides 0 and 1 and ignores the other voices",sent.length===4&&sent.every(r=>r.d[0]===0||r.d[0]===1)&&tone(sent.find(r=>r.d[0]===0),0,440,200)&&tone(sent.find(r=>r.d[0]===1),1,440,200),sent.map(r=>r.d.slice(0,6)));
  await ev(()=>{ soundsOnBlob([27]); for(const l of sounds.mel[curMel]) l.length=0; sndRender(); });
  // ---- backup: sounds are written in the version the puck speaks ----
  const bk=await ev(async()=>{
    const old={send,bindsAck,bindsParse,soundsWrite,lastP,readFrame,bindsReadRaw,soundsReadRaw,stashDrop};
    const keep=sounds; // panelRestore drops the loaded sounds, as after a real restore
    const out={}; const hexOf=a=>[...a].map(x=>x.toString(16).padStart(2,"0")).join("");
    send=async()=>{}; bindsAck=async()=>true; bindsParse=()=>({active:0}); readFrame=async()=>null; stashDrop=()=>{};
    const mk=()=>{ const s=sndEmpty(); s.confirm=1; s.gain=-6; s.mel[0][0]=[{freq:440,ms:200}]; s.mel[0][1]=[{freq:523,ms:100}]; s.mel[0][3]=[{freq:262,ms:50}]; return s; };
    const v2=new Uint8Array(sndBuild(mk(),2)), v1=new Uint8Array(sndBuild(mk(),1));
    let wrote=null; soundsWrite=async b=>{ wrote=b; return true; };
    const restore=async(proto,blob)=>{ lastP=new Array(200).fill(0); lastP[0]=proto; wrote=null; const note=await panelRestore({binds:"00",sounds:{blob:hexOf(blob),names:[]}}); return {wrote,note}; };
    let x=await restore(26,v2); out.v2on1={len:x.wrote&&x.wrote.length,ver:x.wrote&&x.wrote[1],lane0:sndParse(new Uint8Array(x.wrote)).mel[0][0],note:x.note};
    x=await restore(27,v1); const y=sndParse(new Uint8Array(x.wrote)); out.v1on2={len:x.wrote.length,ver:x.wrote[1],l0:y.mel[0][0],l1:y.mel[0][1],note:x.note};
    x=await restore(27,v2); out.v2on2={same:JSON.stringify(x.wrote)===JSON.stringify([...v2]),note:x.note};
    // export: the right number of chunks is read, and the blob is stored as read
    let chunks=0; bindsReadRaw=async()=>new Uint8Array([1]); soundsReadRaw=async n=>{ chunks=n; return v2; };
    lastP=new Array(200).fill(0); lastP[0]=27; const e27=await panelSnapshot(); out.exp27={chunks,blob:e27&&e27.sounds&&e27.sounds.blob===hexOf(v2)};
    soundsReadRaw=async n=>{ chunks=n; return v1; };
    lastP[0]=26; const e26=await panelSnapshot(); out.exp26={chunks,blob:e26&&e26.sounds&&e26.sounds.blob===hexOf(v1)};
    ({send,bindsAck,bindsParse,soundsWrite,lastP,readFrame,bindsReadRaw,soundsReadRaw,stashDrop}=old);
    sounds=keep; sndRender();
    return out;
  });
  check("backup restore: a v2 file on a protocol 26 puck is written as v1 (LT voice) and the note says the other voices were dropped",bk.v2on1.len===1048&&bk.v2on1.ver===1&&same(bk.v2on1.lane0,[{freq:440,ms:200}])&&/without the Right trackpad, Left grip and Right grip/.test(bk.v2on1.note),bk.v2on1);
  check("backup restore: a v1 file on a protocol 27 puck is written as v2 (LT mirrored to RT)",bk.v1on2.len===4144&&bk.v1on2.ver===2&&same(bk.v1on2.l0,[{freq:440,ms:200}])&&same(bk.v1on2.l1,[{freq:440,ms:200}])&&!bk.v1on2.note,bk.v1on2);
  check("backup restore: a v2 file on a protocol 27 puck is written unchanged",bk.v2on2.same&&!bk.v2on2.note,bk.v2on2);
  check("backup export reads 44 chunks on protocol 27 and 11 on protocol 26 and keeps the blob",bk.exp27.chunks===44&&bk.exp27.blob&&bk.exp26.chunks===11&&bk.exp26.blob,[bk.exp27,bk.exp26]);
  // ---- four voices ----
  const lane=i=>page.click("#sndLanes button:nth-child("+(i+1)+")");
  const lanes=()=>ev(()=>sounds.mel[curMel].map(l=>l.map(n=>[n.freq,n.ms])));
  await ev(()=>{ for(const l of sounds.mel[curMel]) l.length=0; sounds.mel[curMel][0].push({freq:0,ms:200},{freq:440,ms:400}); pr.range=null; pr.selS=-1; pr.lastLen=100; pr.fitNow=true; sndRender(); });
  check("the Play on puck (0x31) button is gone",await ev(()=>!document.getElementById("sndPlayPuck")));
  await lane(1);
  check("voice buttons: LT / RT / LG / RG with counts, the active one in the accent style",await ev(()=>[...$("#sndLanes").children].map(b=>b.textContent).join("|")==="LT 2|RT|LG|RG"&&$("#sndLanes").children[1].classList.contains("primary")&&$("#sndLanes").children[2].title==="Left grip (side 3)"));
  await click(310,69);
  check("editing in voice RT leaves voice LT alone",same(await lanes(),[[[0,200],[440,400]],[[0,300],[440,100]],[],[]]),await lanes());
  const rp2=await pos(500,69); await page.mouse.click(rp2.x,rp2.y,{button:"right"});
  check("notes of an inactive voice cannot be grabbed (right-click on one does nothing)",same(await lanes(),[[[0,200],[440,400]],[[0,300],[440,100]],[],[]]),await lanes());
  check("the table shows only the active voice",(await rows())===2);
  // import goes into the active voice only
  await lane(2);
  await ev(bytes=>{ const dt=new DataTransfer(); dt.items.add(new File([new Uint8Array(bytes)],"t.mid"));
    pr.cv.dispatchEvent(new DragEvent("drop",{dataTransfer:dt,bubbles:true,cancelable:true})); },smf());
  await page.waitForSelector("#midiRow:not(.hide)",{timeout:3000}); await page.click("#midiGo");
  const li=await ev(()=>sounds.mel[curMel].map(l=>l.length));
  check("a single-source file fills LT only, whatever voice is active (the others keep their notes)",same(li,[40,2,0,0]),li);
  await lane(0);
  check("Octave and Clear act on the active voice only",await ev(()=>{ sndOctave(1); const a=sounds.mel[curMel][0][0].freq; sndOctave(-1); sndNotes().length=0; sndRender(); return a===131&&sounds.mel[curMel][1].length===2&&sounds.mel[curMel][0].length===0; }));
  check("Clear all voices empties all four voices after confirm, Cancel keeps them",await ev(()=>{ const m=sounds.mel[curMel]; const fill=()=>m.forEach((v,i)=>{ v.length=0; v.push({freq:440+i,ms:100}); }); const oc=window.confirm;
    fill(); window.confirm=()=>false; $("#sndClearAll").click(); const kept=m.every(v=>v.length===1);
    window.confirm=()=>true; $("#sndClearAll").click(); window.confirm=oc; return kept&&m.every(v=>v.length===0); }));
  // Crop to 32 across voices: cut where the first voice would pass 32 entries
  await ev(()=>{ const m=sounds.mel[curMel];
    m[0]=Array.from({length:40},(_,i)=>({freq:i%2?82:65,ms:100}));
    m[1]=Array.from({length:40},(_,i)=>({freq:i%2?82:65,ms:50}));
    m[2]=[]; m[3]=Array.from({length:20},(_,i)=>({freq:i%2?0:330,ms:200}));
    pr.range=null; pr.selS=-1; pr.fitNow=true; sndRender(); });
  await page.click("#prCrop");
  const cl=await ev(()=>({n:sounds.mel[curMel].map(l=>l.length),t:sounds.mel[curMel].map(l=>l.reduce((a,x)=>a+x.ms,0)),msg:$("#sndMsg").textContent}));
  check("Crop to 32 across voices keeps one range: 16 / 32 / 0 / 8 entries, all 1.6 s",same(cl.n,[16,32,0,8])&&same(cl.t,[1600,1600,0,1600])&&/Kept 32 entries \(1\.60 s\)/.test(cl.msg),cl);
  // live playback of two voices: LT on side 0, RG on side 4
  await ev(()=>{ const m=sounds.mel[curMel]; for(const l of m) l.length=0;
    m[0].push({freq:440,ms:200},{freq:0,ms:100},{freq:523,ms:200}); m[3].push({freq:330,ms:300});
    sounds.gain=-12; pr.range=null; pr.selS=-1; sndRender(); window.__sent.length=0; });
  await page.click("#sndLive"); await sleep(900);
  sent=await ev(()=>window.__sent.filter(r=>r.id===0x83));
  const s0=sent.filter(r=>r.d[0]===0), s4=sent.filter(r=>r.d[0]===4), tt=sent[0]&&sent[0].t;
  const rel=(r,ms)=>Math.abs(r.t-tt-ms)<=30;
  check("live, two voices: side 0 plays 440 / rest / 523 / end, with timing",sent.length===6&&s0.length===4&&tone(s0[0],0,440,200)&&rel(s0[0],0)&&off(s0[1],0)&&rel(s0[1],200)&&tone(s0[2],0,523,200)&&rel(s0[2],300)&&off(s0[3],0)&&rel(s0[3],500),sent.map(r=>r.d.slice(0,6)));
  check("live, two voices: side 4 plays 330 Hz for 300 ms, then its own off tone; no other side is touched",s4.length===2&&tone(s4[0],4,330,300)&&rel(s4[0],0)&&off(s4[1],4)&&rel(s4[1],300)&&sent.every(r=>r.d[0]===0||r.d[0]===4),sent.map(r=>r.d.slice(0,6)));
  // keyboard strip: the active voice's side
  await lane(2);
  await ev(()=>{ window.__sent.length=0; });
  await key(66); await sleep(150);
  sent=await ev(()=>window.__sent.filter(r=>r.id===0x83));
  check("keyboard strip click plays on the active voice's side (LG = side 3)",sent.length===1&&tone(sent[0],3,370,300),sent.map(r=>r.d.slice(0,6)));
  await lane(0);
  await ev(()=>{ sounds.mel[0][0]=[{freq:440,ms:200},{freq:0,ms:100},{freq:523,ms:200}]; for(const v of [1,2,3]) sounds.mel[0][v].length=0; sndRender(); });
  await page.click("#sndLive"); await sleep(60);
  await page.click("#sndTabs .slot-tab:nth-child(2)"); await sleep(50);
  check("switching the melody tab during live playback stops it",await ev(()=>!lv.w&&!lv.timer));
  await page.click("#sndTabs .slot-tab:nth-child(1)");
  await page.click("#sndLive"); await sleep(60);
  await ev(()=>{ window.__errs=0; soundsOnBlob([10]); }); await sleep(150);
  check("sounds becoming null during playback halts it without errors",await ev(()=>!lv.w&&!lv.timer&&sounds===null)&&!errors.some(e=>/pageerror/.test(e)),errors);
  await ev(()=>{ sounds=sndEmpty(); soundsCap=true; $("#sndCard").classList.remove("hide"); sounds.mel[0][0]=[{freq:440,ms:200},{freq:0,ms:100},{freq:523,ms:200}]; sndRender(); });
  await page.click("#sndLive"); await sleep(60); await ev(()=>{ window.__sent.length=0; });
  await page.click("#plDisconnect"); await sleep(100);
  const dis=await ev(()=>window.__sent.filter(r=>r.id===0x83).map(r=>r.d[0]));
  check("Disconnect silences all five sides and ends playback",same(dis,[0,1,2,3,4])&&await ev(()=>!lv.w&&!lv.timer),dis);
  check("buttons are disabled when disconnected",await ev(()=>$("#sndLive").disabled&&$("#plPlay").disabled&&$("#plStopAll").disabled));

  // ---- Debug test panel ----
  await page.click("#plConnect"); await page.waitForFunction(()=>/Connected/.test($("#plStatus2").textContent),null,{timeout:3000}).catch(()=>{});
  await ev(()=>{ tabShow("status/debug"); window.__sent.length=0; });
  check("Debug test panel is in Status > Debug",await ev(()=>!!document.querySelector("#sub-status-debug #plTestCard")));
  await page.click("#plPlay"); await sleep(100);
  let ps=await ev(()=>window.__sent);
  check("test panel: Play tone sends 0x83 side 0, -12 dB, 440 Hz, 1000 ms",ps.length===1&&ps[0].id===0x83&&tone(ps[0],0,440,1000),ps);
  await ev(()=>{ window.__sent.length=0; });
  await page.click("#plStopAll"); await sleep(150);
  ps=await ev(()=>window.__sent);
  check("test panel: Stop all = off tone on sides 0-4 and rumble off",ps.length===6&&same(ps.slice(0,5).map(r=>r.d[0]),[0,1,2,3,4])&&ps.slice(0,5).every(r=>r.id===0x83&&r.d[1]===0x80)&&ps[5].id===0x80&&ps[5].d.every(x=>x===0),ps.length);
  check("test panel side labels",await ev(()=>[...document.querySelectorAll("#plSides label")].map(l=>l.textContent.trim()).join("|"))==="0 Left trackpad|1 Right trackpad|2 Both trackpads|3 Left grip|4 Right grip");

  // ---- Shortcuts: one table for chords, melodies, Confirm with and Volume ----
  check("the Back4 chords, Confirmations and Events cards are gone",await ev(()=>!document.getElementById("sndEvCard")&&!document.getElementById("sndSetCard")&&![...document.querySelectorAll("h2")].some(h=>/Back4 chords|Confirmations|^Events$/.test(h.textContent))));
  const land=await ev(()=>{ dev=null; scKey=""; scUpdate(); return {hidden:$("#scEdit").classList.contains("hide"),groups:[...document.querySelectorAll("#scList h3")].map(h=>h.textContent),note:$("#scNote").textContent}; });
  check("without a puck the card is the read-only sheet (all four groups, no editor)",land.hidden&&land.groups.length===4&&land.groups[0]==="Switch USB mode"&&/firmware defaults/.test(land.note),land);
  await ev(()=>{
    window.__keep={send,soundsWrite,bindsXfer,soundsRead,readBlob,lastP};
    send=async()=>{}; readBlob=async()=>null; // a mode change: send the field, read the blob back (nothing here)
    dev={}; lastP=new Array(200).fill(0); lastP[0]=27; lastP[23]=3; lastP[24]=1; lastP[25]=4; [9,8,7,2].forEach((m,i)=>{ lastP[180+i]=m; });
    document.querySelectorAll("select.chord").forEach((s,i)=>{ s.value=String(lastP[23+i]); });
    document.querySelectorAll("select.chordD").forEach((s,i)=>{ s.value=String(lastP[180+i]); });
    sounds=sndEmpty(); soundsCap=true; soundsVer=2; sounds.ev=[0,1,2,3,4,5,6,7]; sounds.confirm=3;
    tabShow("mapping/shortcuts"); sndRender(); scKey=""; scUpdate();
  });
  const tb=await ev(()=>({rows:[...document.querySelectorAll("#scTable tbody tr")].filter(tr=>!tr.classList.contains("hide")).map(tr=>tr.cells[0].textContent),edit:!$("#scEdit").classList.contains("hide"),
    conf:$("#sndConfirm").value,vol:!!$("#sndVol"),list:[...document.querySelectorAll("#scList h3")].map(h=>h.textContent),inCard:!!$("#shortcutsCard #sndConfirm")&&!!$("#shortcutsCard #sndVol")}));
  check("with a puck the card has the table: Confirm with and Volume on top, then one row per shortcut and the connect row",tb.edit&&tb.inCard&&tb.conf==="3"&&same(tb.rows,["back4 + A","back4 + B","back4 + X","back4 + Y","back4 + D-pad left","back4 + D-pad up","back4 + D-pad right","back4 + D-pad down","back4 + RB / back4 + LB","Controller connects (no shortcut)"]),tb);
  check("the fixed shortcuts (Power and slots, Lizard) are listed below the table",same(tb.list,["Power and slots","Lizard (always) mode"]),tb.list);
  const mv=()=>ev(()=>Object.fromEntries([...document.querySelectorAll("#scTable tr[data-dyn]")].map(tr=>[tr.cells[0].textContent,{v:tr.querySelector(".scMel").value,note:tr.querySelector(".scShared").textContent,hidden:tr.querySelector(".scMel").classList.contains("hide")}])));
  let mm=await mv();
  check("melody cells follow the family of each mode (Lizard = other, Xbox 360 = Xbox, Switch Pro = Switch, PS3 / DS4 game / PS5 game = PlayStation)",mm["back4 + A"].v==="6"&&mm["back4 + B"].v==="6"&&mm["back4 + X"].v==="4"&&mm["back4 + Y"].v==="5"&&mm["back4 + D-pad left"].v==="3"&&mm["back4 + D-pad up"].v==="3"&&mm["back4 + D-pad right"].v==="3"&&mm["back4 + D-pad down"].v==="5",Object.values(mm).map(x=>x.v));
  check("shared melodies are noted in the cell",/back4 \+ A/.test(mm["back4 + B"].note)&&/back4 \+ D-pad down/.test(mm["back4 + Y"].note)&&/D-pad up, back4 \+ D-pad right/.test(mm["back4 + D-pad left"].note)&&mm["back4 + X"].note==="",mm);
  const fixedProfiles=await ev(()=>[...document.querySelectorAll('#scTable .scMel[data-ev="0"],#scTable .scMel[data-ev="1"],#scTable .scMel[data-ev="2"],#scTable .scMel[data-ev="7"]')].map(s=>s.value));
  check("Profile 1-3 and Controller connects have their own melody selects",same(fixedProfiles,["0","1","2","7"]),fixedProfiles);
  await page.selectOption("select.chord[data-i='1']","5");
  mm=await mv();
  check("changing back4 + X to PS5 moves its melody cell to the PlayStation event at once, and it becomes shared",mm["back4 + X"].v==="3"&&/D-pad left/.test(mm["back4 + X"].note),mm["back4 + X"]);
  await page.selectOption("select.chord[data-i='1']","1");
  mm=await mv();
  check("changing it back to Xbox 360 returns to the Xbox event",mm["back4 + X"].v==="4"&&mm["back4 + X"].note==="",mm["back4 + X"]);
  // a melody choice + Save to device: the ev[] array reaches the sounds save path
  const sv2=await ev(async()=>{
    let cap=null; bindsXfer=async fn=>fn(); soundsWrite=async b=>{ cap=b; return true; }; soundsRead=async()=>{};
    const sel=document.querySelector("#scTable tbody tr:nth-child(3) .scMel"); sel.value="6"; sel.dispatchEvent(new Event("change"));
    const ev1=sounds.ev.slice(); $("#scSndSave").click(); await new Promise(r=>setTimeout(r,200));
    return {ev1,cap:cap&&cap.slice(6,14),ver:cap&&cap[1],prob:$("#scSndProb").textContent};
  });
  check("choosing a melody for back4 + X (Xbox) and pressing Save writes ev[4] in the v2 blob",same(sv2.ev1,[0,1,2,3,6,5,6,7])&&same(sv2.cap,[0,1,2,3,6,5,6,7])&&sv2.ver===2&&sv2.prob==="",sv2);
  // a family no chord points at gets its own row; profile rows use the user's profile names; modes show a description
  await page.selectOption("select.chord[data-i='1']","5");
  const fam=await ev(()=>({vis:[...document.querySelectorAll("#scTable tr.scFam")].filter(tr=>!tr.classList.contains("hide")).map(tr=>tr.cells[0].textContent),v:document.querySelector('#scTable tr.scFam[data-fam="4"] .scMel').value}));
  check("with no chord on an Xbox mode the row 'Mode changed from the panel: Xbox' appears (PlayStation and Switch are still covered)",same(fam.vis,["Mode changed from the panel: Xbox"])&&fam.v==="6",fam);
  const fsv=await ev(async()=>{
    let cap=null; soundsWrite=async b=>{ cap=b; return true; };
    const sel=document.querySelector('#scTable tr.scFam[data-fam="4"] .scMel'); sel.value="7"; sel.dispatchEvent(new Event("change"));
    $("#scSndSave").click(); await new Promise(r=>setTimeout(r,200)); return cap&&cap.slice(6,14);
  });
  check("saving the family row writes ev[Mode: Xbox]",same(fsv,[0,1,2,3,7,5,6,7]),fsv);
  await ev(()=>{ sounds.ev[4]=4; });
  await page.selectOption("select.chord[data-i='1']","1");
  check("the family row disappears again once a chord points at the Xbox family",await ev(()=>[...document.querySelectorAll("#scTable tr.scFam")].every(tr=>tr.classList.contains("hide"))));
  const pn=await ev(()=>{ profileNames[0]="Work"; scEditSync(); const t=[...document.querySelectorAll("#scTable .scProfName")].map(x=>x.textContent); profileNames[0]=""; scEditSync(); return t; });
  check("the profile rows show the user's profile names",same(pn,["Work","Profile 2","Profile 3"]),pn);
  const dsc=await ev(()=>[...document.querySelectorAll("select.chord")].map(s=>s.parentNode.querySelector(".chordDesc").textContent));
  check("every mode select has a visible description (host, panel, keyboard/mouse)",dsc.length===3&&dsc.every(x=>/·/.test(x)||/Does nothing/.test(x)),dsc);
  // an invalid melody: the reason is shown and Save is disabled
  const inv=await ev(()=>{ sounds.mel[0][0]=Array.from({length:33},()=>({freq:440,ms:100})); sndRender(); return {dis:$("#scSndSave").disabled,prob:$("#scSndProb").textContent}; });
  check("an invalid melody disables Save in the table and shows the reason",inv.dis&&/33 entries/.test(inv.prob),inv);
  await ev(()=>{ sounds.mel[0][0]=[]; sndRender(); });
  // Confirm with: Vibration / Off dim the melody selects, Sound / Both do not
  await page.selectOption("#sndConfirm","1");
  const dim=await ev(()=>({o:[...document.querySelectorAll("#scTable .scMel")].map(s=>s.style.opacity),t:document.querySelector("#scTable .scMel").title,c:sounds.confirm}));
  check("Confirm with = Vibration dims every melody select (still editable) and says why",dim.c===1&&dim.o.every(x=>x==="0.55")&&/include[s]? Sound|includes Sound/.test(dim.t),dim);
  await page.selectOption("#sndConfirm","2");
  check("Confirm with = Sound removes the dimming",await ev(()=>[...document.querySelectorAll("#scTable .scMel")].every(s=>s.style.opacity==="")));
  // a chord set to None has no melody
  const none=await ev(()=>{ const sel=document.querySelector("select.chord[data-i='2']"); const o=document.createElement("option"); o.value="254"; o.textContent="None"; sel.prepend(o); sel.value="254"; sel.dispatchEvent(new Event("change"));
    const tr=document.querySelector("#scTable tbody tr:nth-child(4)"); const r={hidden:tr.querySelector(".scMel").classList.contains("hide"),dash:!tr.querySelector(".scDash").classList.contains("hide")}; sel.value="4"; sel.dispatchEvent(new Event("change")); return r; });
  check("a chord set to None shows - instead of a melody",none.hidden&&none.dash,none);
  // the PNG sheet is built with the melody column
  const png=await ev(async()=>{ try{ const cv=await scSheet(false); return {ok:cv.width>0&&cv.height>0}; }catch(e){ return {ok:false,err:e.message}; } });
  check("the PNG sheet builds with melodies loaded",png.ok,png);
  await sleep(600);
  await ev(()=>{ ({send,soundsWrite,bindsXfer,soundsRead,readBlob,lastP}=window.__keep); dev=null; scKey=""; scUpdate(); });

  check("no page errors, console errors or failed requests",errors.length===0,errors);
  await browser.close(); server.close();
  console.log(fails?"\n"+fails+" check(s) FAILED":"\nall checks passed");
  process.exit(fails?1:0);
})().catch(e=>{ console.error(e); process.exit(2); });
