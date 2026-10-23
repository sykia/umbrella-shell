.pragma library
// Functional controls from TASKBAR_CONTROLS_INVENTORY.md, not a live visibility scan.
var catalog = [
    {id:"widgets", label:"Виджеты", seen:true, secondary:true, zone:"left"},
    {id:"start", label:"Пуск", seen:true, secondary:true, zone:"center"},
    {id:"search", label:"Поиск", seen:true, secondary:true, zone:"center"},
    {id:"taskview", label:"Представление задач", seen:true, secondary:true, zone:"center", native:true},
    {id:"apps", label:"Приложения", seen:true, secondary:true, zone:"center"},
    {id:"overflow", label:"Скрытые значки", seen:true, zone:"right"},
    {id:"input", label:"Индикаторы ввода", seen:true, zone:"right"},
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
    {id:"systemtray", label:"Системные индикаторы (вся группа)", zone:"right"},
    {id:"remote", label:"Удалённый сеанс / запись экрана", zone:"right"}
]
function component(id) { return catalog.filter(function(c) { return c.id === id })[0] }
function movementBlocker(id, primary, available) {
    if (!available) return "Нужны совместимые системные DLL и native-host рядом с приложением."
    if (id === "desktop") return "Показать рабочий стол сохраняет крайний правый слот."
    if (!movable(id)) return "Для этого вложенного или условного компонента адаптер пока не подтверждён."
    return ""
}
function dragIds(id) { return id === "network" || id === "volume" ? ["network","volume"] : [id] }
function movable(id) { return ["widgets","start","search","taskview","apps","overflow","input","network","volume","clock","systemtray","trayapps"].indexOf(id)>=0 }
function remaining(items,id) { var group=dragIds(id); return items.filter(v => group.indexOf(v)<0) }
function nativeConfiguration(value, primary, ids) {
    if (!valid(value)) return false
    if (primary===undefined) primary=true
    var managed=id => id!=="systemtray" && id!=="trayapps"
    if (JSON.stringify(value.available.filter(managed))!==JSON.stringify(defaults(primary,ids).available.filter(managed)) || (value.right.indexOf("desktop")>=0 && value.right[value.right.length-1]!=="desktop")) return false
    return ["left","center","right"].every(function(zone) {
        var list=value[zone], network=list.indexOf("network"), volume=list.indexOf("volume")
        return (network<0 && volume<0) || (network>=0 && volume===network+1)
    })
}
function defaults(primary, ids) {
    var result = {version:1, available:[], left:[], center:[], right:[], gap:8}
    if(Array.isArray(ids)) {
        catalog.forEach(c => { if(ids.indexOf(c.id)<0)result.available.push(c.id) })
        ids.forEach(id => { var c=component(id);if(c)result[c.zone].push(id) })
    } else catalog.forEach(function(c) { result[c.seen && (primary || c.secondary) ? c.zone : "available"].push(c.id) })
    if(result.right.indexOf("desktop")>=0){result.right=result.right.filter(id=>id!=="desktop");result.right.push("desktop")}
    return result
}
function reconcile(value, primary, ids) {
    var base=defaults(primary,ids)
    if(!valid(value))return base
    if(!Array.isArray(ids))return clone(value)
    var next={version:1,gap:value.gap,available:base.available,left:[],center:[],right:[]}, placed=[]
    for(var zone of ["left","center","right"])value[zone].forEach(id => {
        if(ids.indexOf(id)>=0){next[zone].push(id);placed.push(id)}
    })
    for(var key of ["left","center","right"])base[key].forEach(id => {
        if(placed.indexOf(id)>=0)return
        // A newly discovered optional group stays unmanaged until the user
        // places it, rather than displaying an unconfirmed new position.
        if((id==="systemtray" || id==="trayapps") && value.available.indexOf(id)>=0)next.available.push(id)
        else next[key].push(id)
    })
    next.available.sort((a,b) => catalog.indexOf(component(a))-catalog.indexOf(component(b)))
    if(next.right.indexOf("desktop")>=0){next.right=next.right.filter(id=>id!=="desktop");next.right.push("desktop")}
    return next
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
    for (var z of ["available","left","center","right"]) next[z] = remaining(next[z],id)
    if (!Number.isInteger(index) || index < 0 || index > next[zone].length) return null
    next[zone].splice.apply(next[zone],[index,0].concat(dragIds(id)))
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
