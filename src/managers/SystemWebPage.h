#pragma once

#include <Arduino.h>

const char SYSTEM_WEB_PAGE[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>墨水屏时钟 · 系统设置</title>
  <style>
    :root{--ink:#17202a;--muted:#667085;--line:#d0d5dd;--paper:#f7f7f2;--card:#fff;--accent:#185adb;--danger:#b42318}
    *{box-sizing:border-box}body{margin:0;background:var(--paper);color:var(--ink);font:15px/1.5 system-ui,-apple-system,"PingFang SC",sans-serif}
    button,input,select{font:inherit}.shell{display:grid;grid-template-columns:210px minmax(0,1fr);min-height:100vh}
    aside{background:#111827;color:#fff;padding:24px 14px;position:sticky;top:0;height:100vh}
    aside h1{font-size:18px;margin:0 10px 24px}.nav{display:grid;gap:8px}.nav button{border:0;color:#cbd5e1;background:transparent;text-align:left;padding:11px 12px;border-radius:8px}
    .nav button.active,.nav button:hover{color:#fff;background:#263246}main{padding:30px;max-width:1080px;width:100%}
    section{display:none}section.active{display:block}.head{display:flex;justify-content:space-between;align-items:center;gap:16px;margin-bottom:20px}
    h2{margin:0;font-size:25px}.sub{color:var(--muted);margin:4px 0 0}.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:18px;margin-bottom:14px}
    .grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:14px}.row{display:flex;gap:10px;align-items:center;flex-wrap:wrap}
    label{display:grid;gap:6px;color:#344054}input[type=text],input[type=password],input[type=time],input[type=number],select{border:1px solid var(--line);border-radius:8px;padding:9px;background:#fff;min-width:0}
    input[type=text],input[type=password]{width:100%}.btn{border:1px solid var(--line);background:#fff;color:var(--ink);padding:8px 13px;border-radius:8px;cursor:pointer}
    .btn.primary{background:var(--accent);border-color:var(--accent);color:#fff}.btn.danger{color:var(--danger);border-color:#fecdca}.btn.small{padding:5px 9px;font-size:13px}
    .list{display:grid;gap:10px}.item{border:1px solid var(--line);border-radius:10px;padding:12px}.days{display:flex;gap:5px;flex-wrap:wrap}.days label{display:flex;gap:3px;align-items:center;font-size:12px}
    .file{display:grid;grid-template-columns:minmax(0,1fr) auto auto auto;gap:10px;align-items:center;padding:10px 0;border-bottom:1px solid #eaecf0}.file:last-child{border:0}
    .status{font-size:13px;color:var(--muted)}#toast{position:fixed;right:20px;bottom:20px;max-width:320px;background:#101828;color:#fff;padding:12px 16px;border-radius:9px;opacity:0;pointer-events:none;transition:.2s}
    #toast.show{opacity:1}.modal-backdrop{display:none;position:fixed;inset:0;background:#10182899;align-items:center;justify-content:center;padding:20px}.modal-backdrop.open{display:flex}
    .modal{background:#fff;width:min(430px,100%);border-radius:12px;padding:20px}.modal h3{margin-top:0}.modal-actions{display:flex;justify-content:flex-end;gap:10px;margin-top:18px}
    @media(max-width:720px){.shell{grid-template-columns:1fr}aside{height:auto;position:static;padding:14px}aside h1{margin:0 8px 12px}.nav{display:flex;overflow:auto}.nav button{white-space:nowrap}main{padding:18px}.grid{grid-template-columns:1fr}.file{grid-template-columns:1fr auto}.file .secondary{display:none}}
  </style>
</head>
<body>
<div class="shell">
  <aside><h1>墨水屏时钟</h1><nav class="nav">
    <button class="active" data-page="todos">首页 Todo</button>
    <button data-page="alarms">闹钟配置</button>
    <button data-page="files">文件管理</button>
    <button data-page="radio">收音机</button>
    <button data-page="apis">API</button>
  </nav></aside>
  <main>
    <section id="todos" class="active"><div class="head"><div><h2>首页 Todo</h2><p class="sub">保存在设备 NVS，不依赖 SDCard。</p></div><button class="btn primary" id="saveTodos">保存</button></div><div id="todoList" class="list"></div><button class="btn" id="addTodo">新增事项</button></section>
    <section id="alarms"><div class="head"><div><h2>闹钟配置</h2><p class="sub">铃声可选择内置文件或 SDCard 根目录 MP3。</p></div><button class="btn primary" id="saveAlarms">保存</button></div><div id="alarmList" class="list"></div><button class="btn" id="addAlarm">新增闹钟</button></section>
    <section id="files"><div class="head"><div><h2>SDCard 文件管理</h2><p class="sub" id="filePath">/</p></div><button class="btn" id="refreshFiles">刷新</button></div><div class="card"><form id="uploadForm" class="row"><input type="file" id="uploadFile" required><button class="btn primary">上传</button></form></div><div class="card" id="fileList"></div></section>
    <section id="radio"><div class="head"><div><h2>收音机</h2><p class="sub">步进单位基于当前 100 kHz 信道模型。</p></div><button class="btn primary" id="saveRadio">保存</button></div><div class="card grid">
      <label>单步频率<select id="radioStep"><option value="10">0.1 MHz</option><option value="20">0.2 MHz</option><option value="50">0.5 MHz</option><option value="100">1.0 MHz</option></select></label>
      <label>Seek 阈值<input id="radioThreshold" type="number" min="0" max="15"></label>
      <label class="row"><input id="radioBass" type="checkbox">低音增强</label><label class="row"><input id="radioMono" type="checkbox">强制单声道</label>
      <label class="row"><input id="radioSoftMute" type="checkbox">软静音</label>
    </div><div class="card"><h3>频率中文名</h3><div id="stationList" class="list"></div></div></section>
    <section id="apis"><div class="head"><div><h2>API</h2><p class="sub">已保存的 Token 不会回传到浏览器；留空表示不修改。</p></div><button class="btn primary" id="saveApis">保存</button></div><div class="card grid">
      <label>天气 API Token<input id="weatherToken" type="password" autocomplete="off" placeholder="未配置"></label>
      <label>节假日 API Token<input id="holidayToken" type="password" autocomplete="off" placeholder="未配置"></label>
      <label class="row"><input id="clearWeather" type="checkbox">清除天气 Token</label><label class="row"><input id="clearHoliday" type="checkbox">清除节假日 Token</label>
    </div></section>
  </main>
</div>
<div id="toast"></div>
<div id="modalBackdrop" class="modal-backdrop"><div class="modal"><h3 id="modalTitle"></h3><p id="modalText"></p><label id="modalInputWrap" style="display:none">新名称<input id="modalInput" type="text"></label><div class="modal-actions"><button class="btn" id="modalCancel">取消</button><button class="btn primary" id="modalConfirm">确定</button></div></div></div>
<script>
  const state={todos:[],alarms:[],ringtones:["spiffs:/alarm.mp3"],radio:null,path:"/"};
  const days=["日","一","二","三","四","五","六"];
  const $=selector=>document.querySelector(selector);
  const toast=message=>{const el=$("#toast");el.textContent=message;el.classList.add("show");setTimeout(()=>el.classList.remove("show"),2200)};
  async function request(url,options={}){const response=await fetch(url,options);const data=await response.json();if(!response.ok)throw new Error(data.message||"请求失败");return data}
  async function run(action,success){try{await action();if(success)toast(success)}catch(error){console.error(error);toast(error.message)}}
  function jsonOptions(data){return{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(data)}}
  function showPage(id){document.querySelectorAll("section").forEach(el=>el.classList.toggle("active",el.id===id));document.querySelectorAll(".nav button").forEach(el=>el.classList.toggle("active",el.dataset.page===id))}
  document.querySelectorAll(".nav button").forEach(button=>button.addEventListener("click",()=>showPage(button.dataset.page)));
  function buildDays(mask,onChange){const box=document.createElement("div");box.className="days";days.forEach((name,index)=>{const label=document.createElement("label");const input=document.createElement("input");input.type="checkbox";input.checked=Boolean(mask&(1<<index));input.addEventListener("change",onChange);input.dataset.bit=index;label.append(input,"周"+name);box.append(label)});return box}
  function readDays(box){let mask=0;box.querySelectorAll("input").forEach(input=>{if(input.checked)mask|=1<<Number(input.dataset.bit)});return mask}
  function itemButton(text,handler,danger=false){const button=document.createElement("button");button.className="btn small"+(danger?" danger":"");button.type="button";button.textContent=text;button.addEventListener("click",handler);return button}
  function renderTodos(){const list=$("#todoList");list.replaceChildren();state.todos.forEach((todo,index)=>{const card=document.createElement("div");card.className="item";const row=document.createElement("div");row.className="row";const enabled=document.createElement("input");enabled.type="checkbox";enabled.checked=todo.e;enabled.addEventListener("change",()=>todo.e=enabled.checked);const time=document.createElement("input");time.type="time";time.value=String(todo.h).padStart(2,"0")+":"+String(todo.m).padStart(2,"0");time.addEventListener("change",()=>{[todo.h,todo.m]=time.value.split(":").map(Number)});const content=document.createElement("input");content.type="text";content.maxLength=64;content.value=todo.c;content.placeholder="事项内容";content.addEventListener("input",()=>todo.c=content.value);const priority=document.createElement("label");priority.className="row";const high=document.createElement("input");high.type="checkbox";high.checked=todo.p;high.addEventListener("change",()=>todo.p=high.checked);priority.append(high,"高优先级");row.append(enabled,time,content,priority,itemButton("移除",()=>{state.todos.splice(index,1);renderTodos()},true));const dayBox=buildDays(todo.w,()=>todo.w=readDays(dayBox));card.append(row,dayBox);list.append(card)})}
  function addTodo(){state.todos.push({id:0,h:9,m:0,c:"",p:false,w:127,e:true});renderTodos()}
  async function loadTodos(){state.todos=await request("/api/todos");renderTodos()}
  function ringtoneSelect(value){const select=document.createElement("select");state.ringtones.forEach(path=>select.add(new Option(path,path,false,path===value)));return select}
  function renderAlarms(){const list=$("#alarmList");list.replaceChildren();state.alarms.forEach((alarm,index)=>{const card=document.createElement("div");card.className="item";const row=document.createElement("div");row.className="row";const enabled=document.createElement("input");enabled.type="checkbox";enabled.checked=alarm.e;enabled.addEventListener("change",()=>alarm.e=enabled.checked);const time=document.createElement("input");time.type="time";time.value=String(alarm.h).padStart(2,"0")+":"+String(alarm.m).padStart(2,"0");time.addEventListener("change",()=>{[alarm.h,alarm.m]=time.value.split(":").map(Number)});const repeat=document.createElement("select");[["每天",0],["指定星期",1],["工作日",2]].forEach(item=>repeat.add(new Option(item[0],item[1],false,Number(alarm.r)===item[1])));repeat.addEventListener("change",()=>alarm.r=Number(repeat.value));const sound=ringtoneSelect(alarm.s);sound.addEventListener("change",()=>alarm.s=sound.value);row.append(enabled,time,repeat,sound,itemButton("移除",()=>{state.alarms.splice(index,1);renderAlarms()},true));const dayBox=buildDays(alarm.w,()=>alarm.w=readDays(dayBox));card.append(row,dayBox);list.append(card)})}
  async function loadAlarms(){const [alarms,ringtones]=await Promise.all([request("/api/alarms"),request("/api/ringtones")]);state.alarms=alarms;state.ringtones=ringtones;renderAlarms()}
  function addAlarm(){state.alarms.push({h:7,m:30,e:true,r:2,w:62,s:state.ringtones[0]});renderAlarms()}
  async function loadRadio(){state.radio=await request("/api/radio");$("#radioStep").value=state.radio.step;$("#radioThreshold").value=state.radio.threshold;$("#radioBass").checked=state.radio.bass;$("#radioMono").checked=state.radio.mono;$("#radioSoftMute").checked=state.radio.softMute;const list=$("#stationList");list.replaceChildren();state.radio.stations.forEach(station=>{const row=document.createElement("label");row.className="row";const frequency=document.createElement("span");frequency.textContent=(station.frequency/100).toFixed(1)+" MHz";const name=document.createElement("input");name.type="text";name.maxLength=24;name.value=station.name;name.addEventListener("input",()=>station.name=name.value);row.append(frequency,name);list.append(row)})}
  async function loadApis(){const data=await request("/api/api-settings");$("#weatherToken").placeholder=data.weatherConfigured?"已配置，留空不修改":"未配置";$("#holidayToken").placeholder=data.holidayConfigured?"已配置，留空不修改":"未配置"}
  function joinPath(name){const clean=name.startsWith("/")?name.slice(1):name;return state.path==="/"?"/"+clean:state.path+"/"+clean}
  function fileName(path){const parts=path.split("/");return parts[parts.length-1]||path}
  async function loadFiles(){const data=await request("/api/files?path="+encodeURIComponent(state.path));$("#filePath").textContent=data.path;const list=$("#fileList");list.replaceChildren();if(state.path!=="/"){const up=itemButton("返回上级",()=>{const parts=state.path.split("/").filter(Boolean);parts.pop();state.path="/"+parts.join("/");if(state.path.length>1&&state.path.endsWith("/"))state.path=state.path.slice(0,-1);run(loadFiles)});list.append(up)}data.items.forEach(file=>{const path=file.name.startsWith("/")?file.name:joinPath(file.name);const row=document.createElement("div");row.className="file";const name=document.createElement("strong");name.textContent=(file.directory?"📁 ":"")+fileName(file.name);const size=document.createElement("span");size.className="secondary status";size.textContent=file.directory?"目录":file.size+" B";row.append(name,size);if(file.directory)row.append(itemButton("打开",()=>{state.path=path;run(loadFiles)}));else{const link=document.createElement("a");link.className="btn small";link.textContent="下载";link.href="/api/files/download?path="+encodeURIComponent(path);row.append(link)}row.append(itemButton("重命名",()=>openRename(path)),itemButton("移到回收站",()=>openTrash(path),true));list.append(row)})}
  function openModal(title,text,inputValue,onConfirm){$("#modalTitle").textContent=title;$("#modalText").textContent=text;$("#modalInputWrap").style.display=inputValue===null?"none":"grid";$("#modalInput").value=inputValue||"";$("#modalBackdrop").classList.add("open");$("#modalConfirm").onclick=()=>{closeModal();onConfirm($("#modalInput").value)}}
  function closeModal(){$("#modalBackdrop").classList.remove("open");$("#modalConfirm").onclick=null}
  function openRename(path){openModal("重命名",fileName(path),fileName(path),name=>{if(!name||name.includes("/")||name.includes("\\"))return toast("名称无效");const base=path.slice(0,path.length-fileName(path).length);run(async()=>{await request("/api/files/rename",jsonOptions({from:path,to:base+name}));await loadFiles()},"已重命名")})}
  function openTrash(path){openModal("移到回收站","文件将移动到 .trash，不会立即擦除。",null,()=>run(async()=>{await request("/api/files/trash",jsonOptions({path}));await loadFiles()},"已移到回收站"))}
  $("#modalCancel").addEventListener("click",closeModal);$("#modalBackdrop").addEventListener("click",event=>{if(event.target===$("#modalBackdrop"))closeModal()});
  $("#addTodo").addEventListener("click",addTodo);$("#saveTodos").addEventListener("click",()=>run(()=>request("/api/todos",jsonOptions(state.todos)),"Todo 已保存"));
  $("#addAlarm").addEventListener("click",addAlarm);$("#saveAlarms").addEventListener("click",()=>run(()=>request("/api/alarms",jsonOptions(state.alarms)),"闹钟已保存"));
  $("#saveRadio").addEventListener("click",()=>run(()=>request("/api/radio",jsonOptions({step:Number($("#radioStep").value),threshold:Number($("#radioThreshold").value),bass:$("#radioBass").checked,mono:$("#radioMono").checked,softMute:$("#radioSoftMute").checked,stations:state.radio.stations})),"收音机设置已保存"));
  function persistApis(){run(()=>request("/api/api-settings",jsonOptions({weatherToken:$("#weatherToken").value,holidayToken:$("#holidayToken").value,clearWeather:$("#clearWeather").checked,clearHoliday:$("#clearHoliday").checked})),"API 设置已保存")}
  $("#saveApis").addEventListener("click",()=>{if($("#clearWeather").checked||$("#clearHoliday").checked)openModal("确认清除 Token","清除后相关在线数据将停止更新。",null,persistApis);else persistApis()});
  $("#refreshFiles").addEventListener("click",()=>run(loadFiles));$("#uploadForm").addEventListener("submit",event=>{event.preventDefault();run(async()=>{const data=new FormData();data.append("file",$("#uploadFile").files[0]);await request("/api/files/upload?path="+encodeURIComponent(state.path),{method:"POST",body:data});$("#uploadForm").reset();await loadFiles()},"文件已上传")});
  run(async()=>{await Promise.all([loadTodos(),loadAlarms(),loadRadio(),loadApis()]);await loadFiles()});
</script>
</body>
</html>
)rawliteral";
