.pragma library
// Functional controls from TASKBAR_CONTROLS_INVENTORY.md, not a live visibility scan.
var catalog = [
    {id:"widgets", label:"Виджеты", seen:true, secondary:true, zone:"left"},
    {id:"start", label:"Пуск", seen:true, secondary:true, zone:"center"},
    {id:"search", label:"Поиск", seen:true, secondary:true, zone:"center"},
    {id:"taskview", label:"Представление задач", seen:true, secondary:true, zone:"center", native:true},
    {id:"apps", label:"Приложения", seen:true, secondary:true, zone:"center"},
    {id:"overflow", label:"Скрытые значки", seen:true, zone:"right"},
    {id:"input", label:"Язык ввода", seen:true, zone:"right"},
    {id:"network", label:"Сеть", seen:true, zone:"right"},
    {id:"volume", label:"Громкость", seen:true, zone:"right"},
    {id:"clock", label:"Часы и дата", seen:true, secondary:true, zone:"right"},
    {id:"desktop", label:"Показать рабочий стол", seen:true, zone:"right"},
    {id:"trayapps", label:"Значки фоновых приложений", zone:"right"},
    {id:"appoverflow", label:"Переполнение приложений", zone:"center"},
    {id:"power", label:"Батарея / питание", zone:"right"},
    {id:"pen", label:"Меню пера", zone:"right"},
    {id:"keyboard", label:"Сенсорная клавиатура", zone:"right"},
    {id:"touchpad", label:"Виртуальная сенсорная панель", zone:"right"},
    {id:"emoji", label:"Эмодзи", zone:"right"},
    {id:"studio", label:"Studio Effects", zone:"right"},
    {id:"microphone", label:"Использование микрофона", zone:"right"},
    {id:"callmute", label:"Отключение микрофона в звонке", zone:"right"},
    {id:"privacy", label:"Камера / геолокация", zone:"right"},
    {id:"notifications", label:"Уведомления / DND", zone:"right"},
    {id:"eject", label:"Извлечение устройств", zone:"right"},
    {id:"vpn", label:"VPN (состояние сети)", zone:"right"},
    {id:"chat", label:"Chat / Teams (зависит от версии)", zone:"center"},
    {id:"copilot", label:"Copilot (зависит от версии)", zone:"center"},
    {id:"systemtray", label:"Системные / сторонние индикаторы", zone:"right"},
    {id:"remote", label:"Удалённый сеанс / запись экрана", zone:"right"}
]
function component(id) { return catalog.filter(function(c) { return c.id === id })[0] }
function movementBlocker(id, primary, available) {
    if (!available) return "Нужны совместимые системные DLL и native-host рядом с приложением."
    if (!primary) return "Native-перенос на дополнительной панели ещё не проверен."
    if (id === "apps" || id === "appoverflow") return "Нужен адаптер динамической модели приложений: Translation не меняет порядок и Win+номер."
    if (id === "widgets" || id === "start" || id === "search") return "Нужен адаптер раскладки ItemsRepeater и проверка системных flyout и клавиатурного порядка."
    if (id !== "taskview") return "Нужен проверенный адаптер контейнера SystemTray и его hit-test/flyout."
    return ""
}
function defaults(primary) {
    var result = {version:1, available:[], left:[], center:[], right:[], gap:8}
    catalog.forEach(function(c) { result[c.seen && (primary || c.secondary) ? c.zone : "available"].push(c.id) })
    return result
}
function clone(value) { return JSON.parse(JSON.stringify(value)) }
function valid(value) {
    if (!value || value.version !== 1 || !Number.isInteger(value.gap) || value.gap < 0 || value.gap > 96) return false
    var ids = []
    for (var z of ["available","left","center","right"]) {
        if (!Array.isArray(value[z])) return false
        ids = ids.concat(value[z])
    }
    return ids.length === catalog.length && catalog.every(function(c) { return ids.filter(function(id) { return id === c.id }).length === 1 })
}
// Destination index is measured AFTER removing the source: no same-zone off-by-one.
function move(value, id, zone, index) {
    if (!valid(value) || !component(id) || ["available","left","center","right"].indexOf(zone) < 0) return null
    var next = clone(value)
    for (var z of ["available","left","center","right"]) next[z] = next[z].filter(function(v) { return v !== id })
    if (!Number.isInteger(index) || index < 0 || index > next[zone].length) return null
    next[zone].splice(index, 0, id)
    return next
}
// Only this placement will be advertised after its desktop integration check.
function taskViewPlacement(value) {
    if (value.available.indexOf("taskview") >= 0) return "reset"
    var baseline=defaults(true)
    if (["left","center","right"].every(function(z) { return JSON.stringify(value[z])===JSON.stringify(baseline[z]) }) && value.gap===8) return "reset"
    if (value.left.indexOf("taskview")===1 && value.left[0]==="widgets") return "after-widgets"
    return "unsupported"
}
