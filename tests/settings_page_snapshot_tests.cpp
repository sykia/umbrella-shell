#include "core/settings/SettingsPagePolicy.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cstdio>
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc != 2) return 1;
    QFile file(app.arguments()[1]);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 65536) return 2;
    const auto data = QJsonDocument::fromJson(file.readAll()).object();
    const auto rect = [](const QJsonArray& a) { return umbrella::settings::Rect{a[0].toInt(), a[1].toInt(), a[2].toInt(), a[3].toInt()}; };
    std::vector<umbrella::settings::Node> nodes;
    for (const auto& entry : data["nodes"].toArray()) {
        const auto n = entry.toObject();
        nodes.push_back({n["parent"].toInt(-1), n["id"].toString().toStdWString(), rect(n["rect"].toArray()),
            n["visible"].toBool(), n["title"].toBool(), n["scroll"].toBool(), n["navigation"].toBool()});
    }
    const auto client = rect(data["client"].toArray()), expected = rect(data["expected"].toArray());
    const auto result = umbrella::settings::contentBounds(nodes, client, true);
    if (!result || result->left != expected.left || result->top != expected.top || result->right != expected.right || result->bottom != expected.bottom) return 3;
    for (auto& node : nodes) node.title = false;
    if (umbrella::settings::contentBounds(nodes, client, true)) return 4;
    std::printf("Real sanitized Settings snapshot: %zu nodes, negative monitor origin, exact viewport accepted; missing title rejected.\n", nodes.size());
    return 0;
}
