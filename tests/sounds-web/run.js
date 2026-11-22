// Browser test of docs/index.html: Sounds piano roll, MIDI drop, >32 guards, live playback over a fake WebHID,
// and the Debug test panel. Playwright's bundled Chromium, headless (never Edge). Run: npm test
const http=require("http"), fs=require("fs"), path=require("path");
const {chromium}=require("playwright");

const DOCS=path.resolve(__dirname,"../../docs");
const TYPES={".html":"text/html",".js":"text/javascript",".css":"text/css",".mid":"audio/midi",".json":"application/json",".png":"image/png"};
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

(async()=>{
  await new Promise(r=>server.listen(0,"127.0.0.1",r));
  const url="http://127.0.0.1:"+server.address().port+"/index.html";
  const browser=await chromium.launch({headless:true}); // bundled Chromium, not Edge
  const page=await browser.newPage({viewport:{width:1300,height:1500}});
  const errors=[]; let dialogMode="accept";
  page.on("pageerror",e=>errors.push("pageerror: "+e.message));
  page.on("console",m=>{ if(m.type()==="error") errors.push("console.error: "+m.text()); });
  page.on("response",x=>{ if(x.status()>=400) errors.push("HTTP "+x.status()+" "+x.url()); });
  page.on("requestfailed",x=>errors.push("request failed: "+x.url()));
  page.on("dialog",d=>dialogMode==="dismiss"?d.dismiss():d.accept());
  await page.addInitScript(FAKE_HID);
  await page.goto(url); await sleep(500);
  const ev=(f,a)=>page.evaluate(f,a);

  // open the Sounds card without a puck: the page only hides it until a puck reports status protocol 25
  await ev(()=>{
    $("#panel").classList.remove("hide"); $("#sndCard").classList.remove("hide");
    sounds=sndEmpty(); soundsCap=true; tabShow("mapping/sounds"); $("#prSnap").value="50"; sndRender();
    pr.cv.scrollIntoView({block:"center"}); pr.fitNow=true; prDraw();
  });
  const mel=()=>ev(()=>sounds.mel[curMel].map(n=>({freq:n.freq,ms:n.ms})));
  const pos=(ms,row)=>ev(([ms,row])=>{ const r=pr.cv.getBoundingClientRect(); return {x:r.left+prXof(ms),y:r.top+(row==null?10:prYof(row)+pr.rowH/2)}; },[ms,row]);
  const click=async(ms,row)=>{ const p=await pos(ms,row); await page.mouse.move(p.x,p.y); await page.mouse.down(); await page.mouse.up(); };
  const drag=async(a,b)=>{ const p=await pos(...a), q=await pos(...b); await page.mouse.move(p.x,p.y); await page.mouse.down();
    await page.mouse.move((p.x+q.x)/2,(p.y+q.y)/2); await page.mouse.move(q.x,q.y); await page.mouse.up(); };
  const noOverlap=()=>ev(()=>{ let t=0; for(const n of prNotes()){ if(n.s<t) return false; t=n.e; } return sounds.mel[curMel].every(n=>n.ms>=SND_MS_MIN&&n.ms<=SND_MS_MAX); });
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
  await ev(()=>{ sounds.mel[curMel]=[]; pr.selS=-1; pr.lastLen=50; sndRender(); });
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
  await ev(()=>{ sounds.mel[curMel]=[{freq:440,ms:100},{freq:0,ms:200}]; pr.selS=-1; sndRender(); });
  await ev(()=>{ pr.lastLen=100; });
  await click(410,69);
  check("an existing trailing rest keeps its length after an edit",same(await mel(),[{freq:440,ms:100},{freq:0,ms:300},{freq:440,ms:100},{freq:0,ms:200}]),await mel());
  await ev(()=>{ sounds.mel[curMel]=[{freq:440,ms:200}]; pr.selS=-1; sndRender(); });
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
  await ev(()=>{ sounds.mel[curMel]=[{freq:440,ms:200},{freq:0,ms:100},{freq:523,ms:200}]; sndRender(); });
  await page.fill("#prBpm","240"); await page.press("#prBpm","Tab");
  const half=await ev(()=>({m:sounds.mel[curMel].map(n=>n.ms),b:bpmOf(curMel),t:[...document.querySelectorAll("#sndNotes tr[id^=snote] td:nth-child(4) input")].map(i=>+i.value),ls:JSON.parse(localStorage.getItem("openpuck.melBpm"))[curMel]}));
  check("120 -> 240 BPM halves every entry, the table follows, the BPM is kept in localStorage",same(half.m,[100,50,100])&&same(half.t,[100,50,100])&&half.b===240&&half.ls===240,half);
  await ev(()=>{ sounds.mel[curMel]=[{freq:440,ms:200},{freq:0,ms:40}]; sndRender(); });
  await page.fill("#prBpm","480"); await page.press("#prBpm","Tab");
  const ref=await ev(()=>({m:sounds.mel[curMel].map(n=>n.ms),b:bpmOf(curMel),v:$("#prBpm").value,msg:$("#sndMsg").textContent}));
  check("BPM change is refused when an entry would drop under 30 ms (melody and BPM kept)",same(ref.m,[200,40])&&ref.b===240&&ref.v==="240"&&/BPM not changed/.test(ref.msg),ref);
  await ev(()=>{ bpmSet(curMel,120); sounds.mel[curMel]=[]; pr.selS=-1; pr.lastLen=125; $("#prSnap").value="n16"; sndRender(); });
  check("1/16 beat at 120 BPM is a 125 ms step",await ev(()=>prGrid()===125&&prBeatSnap()));
  await click(270,69);
  check("beat snap places a note on the 125 ms grid",same(await mel(),[{freq:0,ms:250},{freq:440,ms:125}]),await mel());
  await ev(()=>{ $("#prSnap").value="50"; bpmSet(curMel,120); $("#prBpm").value=120; });
  // fold: only the rows in use, a vertical drag moves between them
  await ev(()=>{ sounds.mel[curMel]=[{freq:262,ms:100},{freq:330,ms:100},{freq:523,ms:100}]; pr.selS=-1; pr.fitNow=true; sndRender(); });
  await page.check("#prFold");
  const fold=await ev(()=>({fl:pr.fl,h:prYof(64)-prYof(72)===pr.rowH,top:prYof(72)===PR_RH,ss:sessionStorage.getItem(PR_FOLD)}));
  check("Fold shows only the rows in use, high to low, and remembers the choice",same(fold.fl,[72,64,60])&&fold.h&&fold.top&&fold.ss==="1",fold);
  await drag([50,60],[50,64]);
  check("a vertical drag in fold moves the note to the next used row",same(await mel(),[{freq:330,ms:100},{freq:330,ms:100},{freq:523,ms:100}])&&same(await ev(()=>pr.fl),[72,64]),await mel());
  await page.uncheck("#prFold");
  check("Fold off: the continuous range is back",await ev(()=>pr.fl===null&&sessionStorage.getItem(PR_FOLD)==="0"));

  // ---- sndBuild is unchanged for a melody within 32 entries ----
  const build=await ev(()=>{ melBpm[0]=200; const s=sndEmpty(); s.mel[0]=[{freq:440,ms:200},{freq:0,ms:100}]; return sndBuild(s); });
  const exp=[1,250,0,1,2,3,4,5,6,7];
  for(let m=0;m<8;m++){ const e=m===0?[[440,200],[0,100]]:[]; exp.push(e.length); for(let k=0;k<32;k++){ const [f,ms]=e[k]||[0,0]; exp.push(f&255,f>>8,ms&255,ms>>8); } }
  const sum=exp.reduce((a,b)=>(a+b)&0xFFFF,0);
  check("sndBuild bytes for a short melody (BPM is not in the blob)",same(build,[0xB8,1,exp.length&255,exp.length>>8,...exp,sum&255,sum>>8]),build.length);

  // ---- MIDI drop ----
  await ev(()=>{ sounds.mel[curMel].length=0; sndRender(); });
  await ev(bytes=>{
    const dt=new DataTransfer(); dt.items.add(new File([new Uint8Array(bytes)],"t.mid",{type:"audio/midi"}));
    pr.cv.dispatchEvent(new DragEvent("drop",{dataTransfer:dt,bubbles:true,cancelable:true}));
  },smf());
  await page.waitForSelector("#midiRow:not(.hide)",{timeout:3000});
  await page.click("#midiGo");
  check("MIDI import sets the BPM from the file (120)",await ev(()=>bpmOf(curMel)===120&&$("#prBpm").value==="120"));
  const imp=await ev(()=>({n:sounds.mel[curMel].length,f:sounds.mel[curMel].slice(0,3).map(n=>n.freq),ms:sounds.mel[curMel][0].ms,note:$("#midiNote").textContent}));
  check("MIDI drop imports all entries, no cut to 32, no octave shift",imp.n===40&&same(imp.f,[65,82,65])&&imp.ms===100,imp);
  check("import message names the entries and the save limit",/Imported 40 entries/.test(imp.note)&&/32 or fewer/.test(imp.note),imp.note);
  const over=await ev(()=>({cnt:$("#sndCount").textContent,col:$("#sndCount").style.color,save:$("#sndSave").disabled,evs:$("#sndEvSave").disabled,play:$("#sndPlayPuck").disabled,add:$("#sndAddNote").disabled}));
  check("counter warns and Save / Play on puck are disabled above 32",/40 \/ 32/.test(over.cnt)&&/Crop to 32/.test(over.cnt)&&/warn/.test(over.col)&&over.save&&over.evs&&over.play&&over.add,over);
  const guard=await ev(async()=>{ window.__b=0; const o=sndBuild; sndBuild=function(...a){ window.__b++; return o(...a); };
    dev={}; await soundsSave(false); await soundsSave(true); dev=null; sndBuild=o; return {b:window.__b,msg:$("#sndMsg").textContent}; });
  check("saving a melody over 32 is refused before sndBuild",guard.b===0&&/not saved/.test(guard.msg),guard);

  // every sample file through the drop path: all notes keep their MIDI pitch (no octave shift, no cut)
  const fs2=require("fs"), dir=path.join(DOCS,"midi");
  for(const f of fs2.readdirSync(dir).filter(x=>/\.mid$/.test(x))){
    await ev(()=>{ sounds.mel[curMel].length=0; sndRender(); });
    await ev(bytes=>{ const dt=new DataTransfer(); dt.items.add(new File([new Uint8Array(bytes)],"x.mid"));
      pr.cv.dispatchEvent(new DragEvent("drop",{dataTransfer:dt,bubbles:true,cancelable:true})); },Array.from(fs2.readFileSync(path.join(dir,f))));
    await page.waitForSelector("#midiRow:not(.hide)",{timeout:3000});
    const pitches=await ev(()=>[...new Set(midiData.m.tracks[+$("#midiTrack").value].notes.map(n=>n.p))]);
    const bpmExp=await ev(()=>{ const t=midiData.m.tempos; const t0=t.length?t.reduce((a,b)=>b.t<a.t?b:a):null; return t0?Math.round(60000000/t0.us):120; });
    await page.click("#midiGo");
    const got=await ev(()=>({n:sounds.mel[curMel].length,f:[...new Set(sounds.mel[curMel].filter(n=>n.freq).map(n=>n.freq))]}));
    const hz=p=>Math.round(440*Math.pow(2,(p-69)/12)), allowed=new Set(pitches.map(hz));
    check("sample "+f+" sets BPM "+bpmExp,await ev(e=>bpmOf(curMel)===e&&$("#prBpm").value===String(e),bpmExp));
    check("sample "+f+": "+got.n+" entries, every frequency is a MIDI pitch of the track",got.n>0&&got.f.length>0&&got.f.every(x=>allowed.has(x)),{got,pitches});
  }
  // a file with another tempo: 250000 us per quarter = 240 BPM
  await ev(()=>{ sounds.mel[curMel].length=0; sndRender(); });
  await ev(bytes=>{ const dt=new DataTransfer(); dt.items.add(new File([new Uint8Array(bytes)],"t.mid"));
    pr.cv.dispatchEvent(new DragEvent("drop",{dataTransfer:dt,bubbles:true,cancelable:true})); },smf(250000));
  await page.waitForSelector("#midiRow:not(.hide)",{timeout:3000}); await page.click("#midiGo");
  check("MIDI import sets 240 BPM from a 250000 us tempo",await ev(()=>bpmOf(curMel)===240&&$("#prBpm").value==="240"&&JSON.parse(localStorage.getItem("openpuck.melBpm"))[curMel]===240));
  await ev(()=>{ bpmSet(curMel,120); sndRender(); });
  // replace confirmation: Cancel keeps the old melody
  await ev(()=>{ sounds.mel[curMel]=[{freq:440,ms:200}]; sndRender(); });
  await ev(bytes=>{ const dt=new DataTransfer(); dt.items.add(new File([new Uint8Array(bytes)],"t.mid"));
    pr.cv.dispatchEvent(new DragEvent("drop",{dataTransfer:dt,bubbles:true,cancelable:true})); },smf());
  await page.waitForSelector("#midiRow:not(.hide)",{timeout:3000});
  dialogMode="dismiss"; await page.click("#midiGo"); dialogMode="accept";
  check("Cancel in the replace dialog keeps the old melody",same(await mel(),[{freq:440,ms:200}]),await mel());
  // positive control: exactly 32 entries reach sndBuild once
  const ctl=await ev(async()=>{
    sounds.mel[curMel]=Array.from({length:32},(_,i)=>({freq:i%2?0:440,ms:100})); sndRender();
    const o={b:sndBuild,x:bindsXfer,w:soundsWrite,r:soundsRead}; let calls=0;
    sndBuild=function(...a){ calls++; return o.b(...a); }; bindsXfer=async fn=>fn(); soundsWrite=async()=>true; soundsRead=async()=>{};
    dev={}; await soundsSave(false); dev=null;
    sndBuild=o.b; bindsXfer=o.x; soundsWrite=o.w; soundsRead=o.r;
    return {calls,save:$("#sndSave").disabled,msg:$("#sndMsg").textContent};
  });
  check("exactly 32 entries: Save enabled and sndBuild called once",ctl.calls===1&&!ctl.save&&!/not saved/.test(ctl.msg),ctl);
  await ev(()=>{ sounds.mel[curMel]=Array.from({length:40},(_,i)=>({freq:i%2?82:65,ms:100})); sndRender(); });

  // ---- crop (Crop to 32) ----
  const set40=()=>ev(()=>{ sounds.mel[curMel]=Array.from({length:40},(_,i)=>({freq:i%2?82:65,ms:100})); pr.range=null; pr.selS=-1; sndRender(); });
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
  check("cropped melody is within the limit again",await ev(()=>!$("#sndSave").disabled&&!$("#sndPlayPuck").disabled&&$("#prCrop").disabled));

  // ---- live playback ----
  await ev(()=>{ sounds.mel[curMel]=[{freq:440,ms:200},{freq:0,ms:100},{freq:523,ms:200}]; sounds.gain=-12; pr.range=null; sndRender(); window.__sent.length=0; });
  check("Play on controller is enabled with a connection",await ev(()=>!$("#sndLive").disabled));
  await page.click("#sndLive"); await sleep(900);
  let sent=await ev(()=>window.__sent.filter(r=>r.id===0x83));
  const t0=sent[0]&&sent[0].t, tone=(r,side,f,ms)=>r.dev===1&&r.d.length===63&&r.d[0]===side&&r.d[1]===0xF4&&(r.d[2]|r.d[3]<<8)===f&&(r.d[4]|r.d[5]<<8)===ms&&r.d.slice(6).every(x=>x===0);
  const off=(r,side)=>r.d[0]===side&&r.d[1]===0x80&&r.d[2]===0&&r.d[3]===0&&r.d[4]===0&&r.d[5]===0x80;
  const at=(r,ms)=>Math.abs(r.t-t0-ms)<=30;
  check("live: 8 reports, sides 0 and 1 for every event",sent.length===8&&sent.every((r,i)=>r.d[0]===(i%2)),sent.length);
  check("live: note 440 Hz, -12 dB, 200 ms on sides 0 and 1 at t=0",sent.length===8&&tone(sent[0],0,440,200)&&tone(sent[1],1,440,200)&&at(sent[0],0),sent[0]);
  check("live: rest sends the off tone at 200 ms",sent.length===8&&off(sent[2],0)&&off(sent[3],1)&&at(sent[2],200),sent[2]);
  check("live: note 523 Hz at 300 ms",sent.length===8&&tone(sent[4],0,523,200)&&tone(sent[5],1,523,200)&&at(sent[4],300),sent[4]);
  check("live: off tone at the end (500 ms)",sent.length===8&&off(sent[6],0)&&off(sent[7],1)&&at(sent[6],500),sent[6]);
  check("live: Play enabled again after the end",await ev(()=>!$("#sndLive").disabled&&$("#sndLiveStop").disabled));

  await ev(()=>{ pr.range={a:200,b:500}; window.__sent.length=0; });
  await page.click("#sndLive"); await sleep(700);
  sent=await ev(()=>window.__sent.filter(r=>r.id===0x83));
  const t1=sent[0]&&sent[0].t;
  check("live: a selected range plays only the range (rest, 523 Hz at 100 ms, end at 300 ms)",
    sent.length===6&&off(sent[0],0)&&tone(sent[2],0,523,200)&&Math.abs(sent[2].t-t1-100)<=30&&off(sent[4],0)&&Math.abs(sent[4].t-t1-300)<=30,sent.map(r=>r.d.slice(0,6)));

  await ev(()=>{ pr.range=null; window.__sent.length=0; });
  await page.click("#sndLive"); await sleep(100); await page.click("#sndLiveStop"); await sleep(50);
  const nAfter=await ev(()=>window.__sent.length); await sleep(500);
  sent=await ev(()=>window.__sent.filter(r=>r.id===0x83));
  check("live: Stop sends off tones on sides 0 and 1 and nothing follows",sent.length===4&&off(sent[2],0)&&off(sent[3],1)&&(await ev(()=>window.__sent.length))===nAfter,sent.map(r=>r.d.slice(0,6)));

  // keyboard strip: a click plays the note for 300 ms on sides 0 and 1; not while playback runs
  await ev(()=>{ sounds.mel[curMel]=[{freq:330,ms:600}]; pr.selS=-1; pr.range=null; pr.fitNow=true; sndRender(); window.__sent.length=0; });
  const key=async row=>{ const q=await ev(row=>{ const r=pr.cv.getBoundingClientRect(); return {x:r.left+10,y:r.top+prYof(row)+pr.rowH/2}; },row); await page.mouse.click(q.x,q.y); };
  await key(64); await sleep(150);
  sent=await ev(()=>window.__sent.filter(r=>r.id===0x83));
  check("keyboard strip click sends a 300 ms tone on sides 0 and 1 (note, gain from Settings)",sent.length===2&&tone(sent[0],0,330,300)&&tone(sent[1],1,330,300),sent.map(r=>r.d.slice(0,6)));
  await ev(()=>{ window.__sent.length=0; });
  await page.click("#sndLive"); await sleep(100); await key(66); await sleep(150);
  sent=await ev(()=>window.__sent.filter(r=>r.id===0x83));
  check("keyboard strip click sends nothing while live playback runs",sent.length>0&&sent.every(r=>(r.d[2]|r.d[3]<<8)===330||r.d[1]===0x80),sent.map(r=>r.d.slice(0,6)));
  await page.click("#sndLiveStop"); await sleep(50);
  await ev(()=>{ sounds.mel[0]=[{freq:440,ms:200},{freq:0,ms:100},{freq:523,ms:200}]; sndRender(); });
  await page.click("#sndLive"); await sleep(60);
  await page.click("#sndTabs .slot-tab:nth-child(2)"); await sleep(50);
  check("switching the melody tab during live playback stops it",await ev(()=>!lv.w&&!lv.timer));
  await page.click("#sndTabs .slot-tab:nth-child(1)");
  await page.click("#sndLive"); await sleep(60);
  await ev(()=>{ window.__errs=0; soundsOnBlob([10]); }); await sleep(150);
  check("sounds becoming null during playback halts it without errors",await ev(()=>!lv.w&&!lv.timer&&sounds===null)&&!errors.some(e=>/pageerror/.test(e)),errors);
  await ev(()=>{ sounds=sndEmpty(); soundsCap=true; $("#sndCard").classList.remove("hide"); sounds.mel[0]=[{freq:440,ms:200},{freq:0,ms:100},{freq:523,ms:200}]; sndRender(); });
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

  check("no page errors, console errors or failed requests",errors.length===0,errors);
  await browser.close(); server.close();
  console.log(fails?"\n"+fails+" check(s) FAILED":"\nall checks passed");
  process.exit(fails?1:0);
})().catch(e=>{ console.error(e); process.exit(2); });
