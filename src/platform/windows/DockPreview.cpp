#include "DockPreview.h"
#include "DockThumbnail.h"
#include "core/DockPolicy.h"
#include <QWindow>
#include <Windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <array>
#include <algorithm>

namespace umbrella::platform {
namespace {
COLORREF color(QColor value) { return RGB(value.red(),value.green(),value.blue()); }
RECT native(umbrella::dock::Rect r) { return {r.x,r.y,r.x+r.width,r.y+r.height}; }
}
struct DockPreview::Impl {
    DockPreview* owner;
    HWND window=nullptr;
    HFONT font=nullptr;
    std::array<DockThumbnail,8> thumbnails;
    QList<DockWindow> sources;
    umbrella::dock::PreviewLayout layout{};
    QString title;
    QColor background, foreground, accent;
    double scale=1;
    bool inside=false;
    int pressed=-1;
    bool pressedClose=false;

    explicit Impl(DockPreview* value) : owner(value) {}
    ~Impl() {
        for(auto& thumbnail:thumbnails) thumbnail.reset();
        if(window) DestroyWindow(window);
        if(font) DeleteObject(font);
    }
    int hit(int x,int y) const {
        return umbrella::dock::previewHitTest(layout,x,y);
    }
    RECT closeRect(int index) const {
        auto r=native(layout.cell(index)); const int side=std::max(20,static_cast<int>(28*scale));
        r.left=r.right-side; r.bottom=r.top+side; return r;
    }
    void paint() {
        PAINTSTRUCT paint{}; const HDC dc=BeginPaint(window,&paint);
        RECT client{}; GetClientRect(window,&client);
        SetDCBrushColor(dc,color(background)); FillRect(dc,&client,static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
        SetBkMode(dc,TRANSPARENT); SetTextColor(dc,color(foreground));
        const auto previous=SelectObject(dc,font ? font : GetStockObject(DEFAULT_GUI_FONT));
        RECT heading{8,0,client.right-8,layout.header};
        DrawTextW(dc,reinterpret_cast<LPCWSTR>(title.utf16()),static_cast<int>(title.size()),&heading,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
        for(int i=0;i<sources.size();++i) {
            auto rect=native(layout.cell(i)); const auto close=closeRect(i);
            SetDCBrushColor(dc,color(sources[i].active ? accent : foreground));
            FrameRect(dc,&rect,static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
            RECT caption{rect.left+8,rect.top,close.left-4,close.bottom};
            DrawTextW(dc,reinterpret_cast<LPCWSTR>(sources[i].title.utf16()),static_cast<int>(sources[i].title.size()),&caption,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
            RECT closeText=close; DrawTextW(dc,L"×",1,&closeText,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
            if(!thumbnails[i]) {
                RECT fallback{rect.left+8,close.bottom+4,rect.right-8,rect.bottom-8};
                DrawTextW(dc,L"Предпросмотр недоступен",-1,&fallback,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
            }
        }
        SelectObject(dc,previous); EndPaint(window,&paint);
    }
    static LRESULT CALLBACK procedure(HWND hwnd,UINT message,WPARAM w,LPARAM l) {
        auto* self=reinterpret_cast<Impl*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
        if(message==WM_NCCREATE) {
            self=static_cast<Impl*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
            SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
        }
        if(!self) return DefWindowProcW(hwnd,message,w,l);
        switch(message) {
        case WM_MOUSEACTIVATE:return MA_NOACTIVATE;
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:self->paint();return 0;
        case WM_MOUSEMOVE: {
            if(!self->inside) { self->inside=true; emit self->owner->hovered(true); }
            TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,hwnd,0}; TrackMouseEvent(&track); return 0;
        }
        case WM_MOUSELEAVE:self->inside=false;emit self->owner->hovered(false);return 0;
        case WM_NCDESTROY:self->window=nullptr;return DefWindowProcW(hwnd,message,w,l);
        case WM_LBUTTONDOWN: {
            const POINT point{GET_X_LPARAM(l),GET_Y_LPARAM(l)};
            self->pressed=self->hit(point.x,point.y);
            if(self->pressed>=0) { const auto close=self->closeRect(self->pressed); self->pressedClose=PtInRect(&close,point); SetCapture(hwnd); }
            return 0;
        }
        case WM_CAPTURECHANGED:self->pressed=-1;return 0;
        case WM_LBUTTONUP: {
            const int x=GET_X_LPARAM(l), y=GET_Y_LPARAM(l), index=self->hit(x,y);
            const int pressed=self->pressed; const bool pressedClose=self->pressedClose;
            self->pressed=-1; if(GetCapture()==hwnd) ReleaseCapture();
            if(index>=0 && index==pressed) {
                const auto source=self->sources[index]; const auto close=self->closeRect(index);
                const bool releasedClose=PtInRect(&close,{x,y})!=FALSE;
                if(pressedClose && releasedClose) emit self->owner->closeRequested(source);
                else if(!pressedClose && !releasedClose) emit self->owner->activated(source);
            }
            return 0;
        }
        case WM_CLOSE:self->owner->hide();emit self->owner->hovered(false);return 0;
        }
        return DefWindowProcW(hwnd,message,w,l);
    }
    bool create(HWND parent) {
        if(window) { SetWindowLongPtrW(window,GWLP_HWNDPARENT,reinterpret_cast<LONG_PTR>(parent)); return true; }
        WNDCLASSEXW cls{sizeof(cls)}; cls.lpfnWndProc=procedure; cls.hInstance=GetModuleHandleW(nullptr);
        cls.lpszClassName=L"Umbrella.DockPreview.v1"; cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);
        if(!RegisterClassExW(&cls) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) return false;
        window=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,cls.lpszClassName,L"Umbrella — окна",WS_POPUP,
            0,0,1,1,parent,nullptr,cls.hInstance,this);
        return window!=nullptr;
    }
};
DockPreview::DockPreview(QObject* parent):QObject(parent),impl_(std::make_unique<Impl>(this)) {}
DockPreview::~DockPreview()=default;
bool DockPreview::visible() const { return impl_->window && IsWindowVisible(impl_->window); }
void DockPreview::hide() {
    for(auto& thumbnail:impl_->thumbnails) thumbnail.reset();
    impl_->sources.clear(); impl_->inside=false;
    if(impl_->window) { DestroyWindow(impl_->window); impl_->window=nullptr; }
    if(impl_->font) { DeleteObject(impl_->font); impl_->font=nullptr; }
}
bool DockPreview::show(QWindow& owner,const QRectF& anchor,const QString& edge,const QString& title,
    const QList<DockWindow>& windows,QColor background,QColor foreground,QColor accent) {
    const auto parent=reinterpret_cast<HWND>(owner.winId());
    if(!IsWindow(parent) || windows.isEmpty()) { hide(); return false; }
    QList<DockWindow> sources;
    for(const auto& source:windows) { if(DockWindows::current(source)) sources.append(source); if(sources.size()==8) break; }
    if(sources.isEmpty() || !impl_->create(parent)) { hide(); return false; }
    auto& state=*impl_;
    const double scale=GetDpiForWindow(parent)/96.0;
    if(state.font && state.scale!=scale) { DeleteObject(state.font); state.font=nullptr; }
    state.scale=scale;
    RECT ownerRect{}; GetWindowRect(parent,&ownerRect);
    MONITORINFO monitor{sizeof(monitor)};
    if(!GetMonitorInfoW(MonitorFromWindow(parent,MONITOR_DEFAULTTONEAREST),&monitor)) { hide(); return false; }
    using namespace umbrella::dock;
    const Rect work{monitor.rcWork.left,monitor.rcWork.top,monitor.rcWork.right-monitor.rcWork.left,monitor.rcWork.bottom-monitor.rcWork.top};
    const Rect target{ownerRect.left+qRound(anchor.x()*state.scale),ownerRect.top+qRound(anchor.y()*state.scale),qRound(anchor.width()*state.scale),qRound(anchor.height()*state.scale)};
    const auto side=edge=="top" ? Edge::top : edge=="left" ? Edge::left : edge=="right" ? Edge::right : Edge::bottom;
    state.layout=previewLayout(work,target,side,static_cast<int>(sources.size()),state.scale);
    sources=sources.mid(0,state.layout.count);
    const auto bounds=state.layout.bounds;
    if(!SetWindowPos(state.window,nullptr,bounds.x,bounds.y,bounds.width,bounds.height,SWP_NOACTIVATE|SWP_NOZORDER)) { hide(); return false; }
    state.title=title+(windows.size()>8 ? QStringLiteral(" · первые 8 окон; остальные — в меню") : QString{});
    state.background=background;state.foreground=foreground;state.accent=accent;
    if(!state.font) {
        NONCLIENTMETRICSW metrics{sizeof(metrics)};
        if(SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof(metrics),&metrics,0)) {
            metrics.lfMessageFont.lfHeight=-qRound(13*state.scale); state.font=CreateFontIndirectW(&metrics.lfMessageFont);
        }
    }
    for(int i=0;i<8;++i) {
        auto& thumbnail=state.thumbnails[i];
        const bool same=i<sources.size() && i<state.sources.size() && sources[i].handle==state.sources[i].handle && sources[i].processId==state.sources[i].processId && sources[i].processCreated==state.sources[i].processCreated;
        if(!same) thumbnail.reset();
        if(i>=sources.size()) continue;
        const auto cell=state.layout.cell(i); const int inset=std::max(2,qRound(6*state.scale));
        RECT viewport{cell.x+inset,cell.y+qRound(30*state.scale),cell.x+cell.width-inset,cell.y+cell.height-inset};
        thumbnail.show(state.window,reinterpret_cast<HWND>(sources[i].handle),viewport);
    }
    state.sources=sources;
    const DWM_WINDOW_CORNER_PREFERENCE rounded=DWMWCP_ROUND;
    DwmSetWindowAttribute(state.window,DWMWA_WINDOW_CORNER_PREFERENCE,&rounded,sizeof(rounded));
    const auto r=state.layout.bounds;
    if(!SetWindowPos(state.window,HWND_TOPMOST,r.x,r.y,r.width,r.height,SWP_NOACTIVATE|SWP_SHOWWINDOW)) { hide(); return false; }
    InvalidateRect(state.window,nullptr,FALSE); return true;
}
}
