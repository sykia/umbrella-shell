// Headless protocol/lifetime checks, never opens Settings or changes the taskbar.
#include <Windows.h>
#include <psapi.h>
#include <QCoreApplication>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QElapsedTimer>
#include <cstdio>
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc != 2) return 1;
    QProcess process;
    process.setProgram(app.arguments()[1]); process.setArguments({"--settings-blur-observer"});
    process.start();
    if (!process.waitForStarted(2500)) return 2;
    for (int epoch : {1, 2}) {
        const auto command = QByteArray::number(epoch) + " 0\n";
        if (process.write(command) != command.size() || !process.waitForBytesWritten(1000)) return 3;
        QElapsedTimer timer; timer.start(); QByteArray line;
        while (!line.endsWith('\n') && timer.elapsed() < 2500) {
            process.waitForReadyRead(100); line += process.readAllStandardOutput();
            if (line.size() > 1024) return 4;
        }
        const auto reply = QJsonDocument::fromJson(line).object();
        if (reply["epoch"].toString() != QString::number(epoch) || reply["window"].toString() != "0" || reply.contains("rect")) return 5;
    }
    HANDLE handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, static_cast<DWORD>(process.processId()));
    FILETIME created{}, exited{}, kernel1{}, user1{}, kernel2{}, user2{};
    PROCESS_MEMORY_COUNTERS memory{sizeof(memory)};
    const bool before = handle && GetProcessTimes(handle, &created, &exited, &kernel1, &user1);
    if (process.waitForReadyRead(500) || !process.readAllStandardError().isEmpty()) { if (handle) CloseHandle(handle); return 6; }
    if (before && GetProcessTimes(handle, &created, &exited, &kernel2, &user2) && GetProcessMemoryInfo(handle, &memory, sizeof(memory))) {
        const auto ticks = [](FILETIME t) { return (static_cast<unsigned long long>(t.dwHighDateTime) << 32) | t.dwLowDateTime; };
        std::printf("Observer idle 500ms: CPU %.3fms; working set %zu KiB (isolated fixture).\n",
            (ticks(kernel2) + ticks(user2) - ticks(kernel1) - ticks(user1)) / 10000.0, memory.WorkingSetSize / 1024);
    }
    if (handle) CloseHandle(handle);
    process.closeWriteChannel();
    if (!process.waitForFinished(2500) || process.exitStatus() != QProcess::NormalExit || process.exitCode()) return 7;
    std::puts("Observer: rejects absent target, accepts sequential epochs, idle emits nothing, EOF exits cleanly.");
    return 0;
}
