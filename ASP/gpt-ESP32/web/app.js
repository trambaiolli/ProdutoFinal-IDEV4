const motors=[1,2,3], desired={};
const box=document.getElementById("motors");
motors.forEach(id=>{
  desired[id]="S";
  box.insertAdjacentHTML("beforeend",`
  <section class="motor">
    <h2>Motor ${id}</h2>
    <div class="telemetry">
      <div id="state${id}" class="badge">Estado: --</div>
      <div id="power${id}" class="badge">Potência: 35%</div>
      <div id="fa${id}" class="badge">Avanço: --</div>
      <div id="fr${id}" class="badge">Recuo: --</div>
    </div>
    <p>Potência: <strong id="pct${id}">35%</strong></p>
    <input id="pwm${id}" type="range" min="10" max="70" value="35">
    <div class="buttons">
      <button class="advance" data-id="${id}" data-a="A">▲ Avançar</button>
      <button class="reverse" data-id="${id}" data-a="R">▼ Recuar</button>
      <button class="stop" data-id="${id}" data-a="S">Parar</button>
    </div>
  </section>`);
});
const pwmValue=p=>Math.round(26+(p-10)*(178-26)/60);
async function send(id,a){
  const p=+document.getElementById("pwm"+id).value;
  desired[id]=a;
  try{await fetch(`/api/motor?id=${id}&acao=${a}&pwm=${pwmValue(p)}`,{cache:"no-store"})}catch(e){}
}
document.querySelectorAll("input[type=range]").forEach(e=>e.oninput=()=>{
  const id=e.id.replace("pwm","");
  document.getElementById("pct"+id).textContent=e.value+"%";
  document.getElementById("power"+id).textContent="Potência: "+e.value+"%";
});
document.querySelectorAll("button[data-id]").forEach(b=>{
  const id=+b.dataset.id,a=b.dataset.a;
  if(a==="S") b.onclick=()=>send(id,"S");
  else{
    b.onpointerdown=e=>{e.preventDefault();send(id,a)};
    b.onpointerup=e=>{e.preventDefault();send(id,"S")};
    b.onpointercancel=e=>{e.preventDefault();send(id,"S")};
    b.onpointerleave=e=>{if(e.buttons)send(id,"S")};
  }
});
setInterval(()=>motors.forEach(id=>{if(desired[id]!=="S")send(id,desired[id])}),150);
document.getElementById("stopAll").onclick=async()=>{
  motors.forEach(i=>desired[i]="S");
  try{await fetch("/api/stop",{cache:"no-store"})}catch(e){}
};
async function status(){
  try{
    const r=await fetch("/api/status",{cache:"no-store"}),d=await r.json();
    d.motors.forEach(m=>{
      document.getElementById("state"+m.id).textContent="Estado: "+m.state;
      const fa=document.getElementById("fa"+m.id),fr=document.getElementById("fr"+m.id);
      fa.textContent="Avanço: "+(m.forwardLimit?"ATIVO":"livre");
      fr.textContent="Recuo: "+(m.reverseLimit?"ATIVO":"livre");
      fa.classList.toggle("active",m.forwardLimit);fr.classList.toggle("active",m.reverseLimit);
    });
    dot.classList.add("ok");connection.textContent="ESP32 conectado";
  }catch(e){dot.classList.remove("ok");connection.textContent="Sem comunicação"}
}
setInterval(status,300);status();
document.addEventListener("visibilitychange",()=>{if(document.hidden)fetch("/api/stop",{keepalive:true}).catch(()=>{})});
