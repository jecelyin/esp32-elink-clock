#include "WebManager.h"

const char* INDEX_HTML = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>E-Ink Clock Config</title>
    <style>
        body { font-family: sans-serif; max-width: 800px; margin: 0 auto; padding: 20px; }
        .todo-item { border: 1px solid #ddd; padding: 10px; margin-bottom: 10px; border-radius: 5px; background: #f9f9f9; }
        .todo-header { display: flex; gap: 10px; margin-bottom: 5px; flex-wrap: wrap;}
        .todo-content { width: 100%; margin-top: 5px; }
        input[type="text"] { width: 100%; padding: 5px; box-sizing: border-box; }
        .days-selector { display: flex; gap: 5px; margin-top: 5px; font-size: 0.9em; flex-wrap: wrap;}
        .day-check { cursor: pointer; user-select: none; padding: 2px 6px; border: 1px solid #ccc; border-radius: 3px; }
        .day-check.selected { background: #007bff; color: white; border-color: #007bff; }
        button { cursor: pointer; padding: 8px 16px; background: #007bff; color: white; border: none; border-radius: 4px; }
        button.remove { background: #dc3545; padding: 4px 8px; font-size: 0.8em; margin-left: auto; }
        button:hover { opacity: 0.9; }
        .actions { margin-top: 20px; display: flex; gap: 10px; justify-content: flex-end; }
    </style>
</head>
<body>
    <h1>Todo List Configuration</h1>
    <div id="todo-list"></div>
    <div class="actions">
        <button onclick="addTodo()">+ Add Item</button>
        <button onclick="saveTodos()">Save to Device</button>
    </div>

    <script>
        let todos = [];
        const days = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];

        async function loadTodos() {
            try {
                const response = await fetch('/api/todos');
                todos = await response.json();
                render();
            } catch (e) {
                console.error('Err', e);
                // Demo data if fetch fails
                if(todos.length === 0) {
                   // todos = [{id:1, h:13, m:30, c:"Demo Task", p:true, w:127, e:true}];
                   render();
                }
            }
        }

        function render() {
            const container = document.getElementById('todo-list');
            container.innerHTML = '';
            todos.forEach((todo, index) => {
                const el = document.createElement('div');
                el.className = 'todo-item';
                
                let daysHtml = days.map((d, i) => {
                    const selected = (todo.w & (1 << i)) ? 'selected' : '';
                    return `<span class="day-check ${selected}" onclick="toggleDay(${index}, ${i})">${d}</span>`;
                }).join('');

                el.innerHTML = `
                    <div class="todo-header">
                        <input type="checkbox" ${todo.e ? 'checked' : ''} onchange="update(${index}, 'e', this.checked)" title="Enable">
                        <input type="number" value="${todo.h}" min="0" max="23" style="width:50px" onchange="update(${index}, 'h', this.value)"> :
                        <input type="number" value="${todo.m}" min="0" max="59" style="width:50px" onchange="update(${index}, 'm', this.value)">
                        <label><input type="checkbox" ${todo.p ? 'checked' : ''} onchange="update(${index}, 'p', this.checked)"> High Priority</label>
                        <button class="remove" onclick="removeTodo(${index})">Remove</button>
                    </div>
                    <div class="todo-content">
                        <input type="text" value="${todo.c}" placeholder="Task content" onchange="update(${index}, 'c', this.value)">
                    </div>
                    <div class="days-selector">
                        Repeat: ${daysHtml}
                        <span class="day-check" onclick="setDaily(${index})" style="margin-left:10px">All</span>
                        <span class="day-check" onclick="setOnce(${index})">Once</span>
                    </div>
                `;
                container.appendChild(el);
            });
        }

        function update(index, field, value) {
            if (field === 'h' || field === 'm') value = parseInt(value);
            todos[index][field] = value;
        }

        function toggleDay(index, dayBit) {
            todos[index].w ^= (1 << dayBit);
            render();
        }
        
        function setDaily(index) {
            todos[index].w = 127;
            render();
        }
        
        function setOnce(index) {
          // If "Once" means "Today", we can perhaps assume logic handled by backend or just 0
          // But backend treats 0 as never matching if logic requires bitmask match.
          // Let's set to current day? Or just 0 and let backend handle "one-shot".
          // For now, let's just make it 0 and assuming backend (or this UI) needs enhancement for true "One-shot date".
          // User request said "Cycle: Daily or specific week days". Doesn't strictly say "One shot date".
          // So I will just clear all days.
          todos[index].w = 0;
          render();
        }

        function addTodo() {
            todos.push({id: 0, h: 12, m: 0, c: "", p: false, w: 127, e: true});
            render();
        }

        function removeTodo(index) {
            todos.splice(index, 1);
            render();
        }

        async function saveTodos() {
            try {
                const response = await fetch('/api/save', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify(todos)
                });
                if (response.ok) alert('Saved!');
                else alert('Failed to save');
            } catch (e) {
                alert('Error: ' + e);
            }
        }

        loadTodos();
    </script>
</body>
</html>
)rawliteral";

WebManager::WebManager(TodoManager* todoMgr) : server(80), todoMgr(todoMgr) {}

void WebManager::begin() {
    server.on("/", HTTP_GET, [this]() { handleRoot(); });
    server.on("/api/todos", HTTP_GET, [this]() { handleGetTodos(); });
    server.on("/api/save", HTTP_POST, [this]() { handleSaveTodos(); });
    server.begin();
    Serial.println("Web Server started");
}

void WebManager::loop() {
    server.handleClient();
}

void WebManager::handleRoot() {
    server.send(200, "text/html", INDEX_HTML);
}

void WebManager::handleGetTodos() {
    String json = todoMgr->getTodosJSON();
    server.send(200, "application/json", json);
}

void WebManager::handleSaveTodos() {
    if (server.hasArg("plain") == false) {
        server.send(400, "text/plain", "Body not received");
        return;
    }
    String body = server.arg("plain");
    todoMgr->saveTodosFromJSON(body);
    server.send(200, "text/plain", "OK");
}
